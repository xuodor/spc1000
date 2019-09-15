/**
 * @file spcmain.c
 * @brief SPC-1000 emulator main (in,out, and main loop)
 * @author Kue-Hwan Sihn (ionique)
 * @date 2005~2006.10
 */

#include "AY8910.h" // AY-3-8910 Sound (Marat Fayzullin)
#include "MC6847.h" // Video Display Chip (ionique)
#include "Tables.h" // Z-80 emulator (Marat Fayzullin)
#include "Z80.h"    // Z-80 emulator (Marat Fayzullin)
#include "SDL_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(__ANDROID__)
#include <android/log.h>
#include "spc1000_jni.h"
#else
#include "tinyfiledialogs.h"
#endif

#include "common.h"
#include "spckey.h" // keyboard definition

#define NONE 0
#define EDGE 1
#define WAITEND 2

#define CAS_STOP 0
#define CAS_PLAY 1
#define CAS_REC 2

#define I_PERIOD 4000
#define INTR_PERIOD 16.6666

#define SPC_EMUL_VERSION "1.0 (2007.02.20)"
#define FCLOSE(x) fclose(x), (x) = NULL

SPCConfig spcConfig;

/**
 * Cassette structure for tape processing, included in the SPCIO
 */
typedef struct {
  int motor; // Motor Status
  int pulse; // Motor Pulse (0->1->0) causes motor state to flip
  int button;
  int rdVal;
  Uint32 startTime;
  Uint32 cnt0, cnt1;

  int wrVal;
  Uint32 wrRisingT; // rising time

  FILE *wfp;
  FILE *rfp;
} Cassette;

/**
 * SPC IO structure, registers and memories mappted to IO space
 */
typedef struct {
  int IPLK;          // IPLK switch for memory selection
  byte VRAM[0x2000]; // Video Memory (6KB)
  byte GMODE;        // GMODE (for video)

  byte psgRegNum; // Keep PSG (AY-3-8910) register number for next turn of I/O

  // int intrState;

  /* SPC-1000 keyboard matrix. Initially turned off. (high) */
  byte keyMatrix[10];

  Cassette cas;

  AY8910 ay8910;
} SPCIO;

/**
 * SPC system structure, Z-80, RAM, ROM, IO(VRAM), etc
 */
typedef struct {
  Z80 Z80R;        // Z-80 register
  byte RAM[65536]; // RAM area
  byte ROM[32768]; // ROM area
  SPCIO IO;
  Uint32 tick;
  int turbo;
  int refrTimer;   // timer for screen refresh
  int refrSet;     // init value for screen refresh timer
  double intrTime; // variable for interrupt timing
} SPCSystem;

SPCSystem spc;

/**
 * SPC simulation structure, realted to the timing
 */
typedef struct {
  Uint32 baseTick, curTick, prevTick;
} SPCSimul;

SPCSimul simul;

/*************************************************************/
/** INI file processing                                     **/
/*************************************************************/

/**
 * Strip white space in the given string
 * @param x string pointer to strip (in-place update)
 */
void StripWS(char *x) {
  char *tmp = x;

  for (; *x != '\0'; x++)
    if (strchr(" \t\r\n", *x) == NULL)
      *tmp = *x, tmp++;
  *tmp = '\0';
}

/**
 * Compare string, only to the length of first string
 * @param keyword the first string to compare
 * @param str the second string
 */
int str1ncmp(char *keyword, char *str) {
  return strncmp(keyword, str, sizeof(keyword));
}

/**
 * Get a integer value from the string of "KEYWORD=VALUE" form
 * @param str the string of "KEYWORD=VALUE" form
 */
int GetVal(char *str) {
  char *tmp;

  tmp = strchr(str, '=');
  if (tmp == NULL)
    return -1;
  return atoi(tmp + 1);
}

int read_from_buf(char *buf, char *out, int offset) {
  while (buf[offset] != '\n') {
    *out++ = buf[offset++];
  }
  *out = '\0';
  return offset + 1;
}

/**
 * Process INI file
 * @param ini_data INI file content
 * @param ini_len Length of the content
 */
void ReadINI(char *ini_data, int ini_len) {
  static char inputstr[120];
  // default
  spcConfig.colorMode = SPCCOL_NEW1;
  spcConfig.scanLine = SCANLINE_045_ONLY;
  spcConfig.frameSkip = 0;
  spcConfig.casTurbo = 1;
  spcConfig.soundVol = 5;
  spcConfig.scale = 1;
  spcConfig.keyLayout = 0;
  spcConfig.font = 0;

  int offset = 0;
  while (offset < ini_len) {
    int val = 0;

    offset = read_from_buf(ini_data, inputstr, offset);

    StripWS(inputstr);
    if (!strcmp("", inputstr) || inputstr[0] == '#')
      continue;
    if (!str1ncmp("COLORSET", inputstr)) {
      val = GetVal(inputstr);
      if (val >= 0 && val <= SPCCOL_GREEN)
        spcConfig.colorMode = val;
      continue;
    }
    if (!str1ncmp("SCANLINE", inputstr)) {
      val = GetVal(inputstr);
      if (val >= 0 && val <= SCANLINE_045_ONLY)
        spcConfig.scanLine = val;
      continue;
    }
    if (!str1ncmp("FRAMESKIP", inputstr)) {
      val = GetVal(inputstr);
      if (val >= 0)
        spcConfig.frameSkip = val;
      continue;
    }
    if (!str1ncmp("CASTURBO", inputstr)) {
      val = GetVal(inputstr);
      if (val == 0 || val == 1)
        spcConfig.casTurbo = val;
      continue;
    }
    if (!str1ncmp("SOUNDVOL", inputstr)) {
      val = GetVal(inputstr);
      if (val >= 0 && val <= 40)
        spcConfig.soundVol = val;
      continue;
    }
    if (!str1ncmp("KEYLAYOUT", inputstr)) {
      val = GetVal(inputstr);
      if (val == 0 || val == 1)
        spcConfig.keyLayout = val;
      continue;
    }
    if (!str1ncmp("FONTDATA", inputstr)) {
      val = GetVal(inputstr);
      if (val == 0 || val == 1)
        spcConfig.font = val;
      continue;
    }
#if !defined(__ANDROID__)
    if (!str1ncmp("SCALEFACTOR", inputstr)) {
      val = GetVal(inputstr);
      if (0 <= val && val <= 4) spcConfig.scale = val;
      continue;
    }
#endif
    printf("The following line in the INI file is ignored:\n\t%s\n", inputstr);
  }
}

/*************************************************************/
/** Initialize IO                                           **/
/*************************************************************/

/**
 * Reset Cassette structure
 * @param cas cassette structure
 */
void ResetCassette(Cassette *cas) {
  cas->rdVal = -2;
  cas->cnt0 = cas->cnt1 = 0;

  cas->wrVal = 0; // correct location?
  cas->wrRisingT = 0;
}

/**
 * Initialize cassette structure
 * @param cas cassette structure
 */
void InitCassette(Cassette *cas) {
  cas->button = CAS_STOP;

  cas->wfp = NULL;
  cas->rfp = NULL;
  ResetCassette(cas);
}

/**
 * Initialize cassette structure
 * @param cas cassette structure
 */
void InitIOSpace(void) {
  int i;

  spc.IO.IPLK = 1;
  spc.IO.GMODE = 0;
  spc.IO.cas.motor = 0;
  spc.IO.cas.pulse = 0;
  spc.IO.psgRegNum = 0;
  // VRAM init?

  Reset8910(&(spc.IO.ay8910), 0);

  spc.turbo = 0;
  for (i = 0; i < 10; i++) // turn off all keys
    spc.IO.keyMatrix[i] = 0xff;
}

/*************************************************************/
/** Memory Read/Write                                       **/
/*************************************************************/

/**
 * Writing SPC-1000 RAM
 * @param Addr 0x0000~0xffff memory address
 * @param Value a byte value to be written
 */
void WrZ80(register word Addr, register byte Value) { spc.RAM[Addr] = Value; }

/**
 * Reading SPC-1000 RAM
 * @param Addr 0x0000~0xffff memory address
 * @warning Read behavior is ruled by IPLK
 */
byte RdZ80(register word Addr) {
  if (spc.IO.IPLK)
    return spc.ROM[Addr & 0x7fff];
  else
    return spc.RAM[Addr];
}

/*************************************************************/
/** Image File (Z80+Memory+IO+VRAM Save File) Read/Write    **/
/*************************************************************/

#if !defined(__ANDROID__)
int LoadImageFile(void) {
  char const *filters[2] = {"*.sav", "*.SAV"};
  char const *name;
  int res = -1;
  FILE *fp;
  name = tinyfd_openFileDialog("Load System Image", "", 2, filters,
                               "image files", 0);
  if (name && (fp = fopen(name, "rb")) != NULL) {
    fread(&spc, sizeof(spc), 1, fp);
    fclose(fp);
    res = 0;
  }
  return res;
}

int SaveImageFile(void) {
  char const *filters[2] = {"*.sav", "*.SAV"};
  char const *name;
  int res = -1;
  FILE *fp;
  name = tinyfd_saveFileDialog("Save System Image", "RAMIMAGE.SAV", 2, filters,
                               NULL);
  if (name && (fp = fopen(name, "wb")) != NULL) {
    fwrite(&spc, sizeof(spc), 1, fp);
    fclose(fp);
    res = 0;
  }
  return res;
}
#else
int LoadImageFile(void) {
  FILE *fp;
  char name[256];
  int res = OpenImageDialog(name);
  if (!res) {
    if ((fp = fopen(name, "rb")) == NULL) {
      res = -1;
    } else {
      fread(&spc, sizeof(spc), 1, fp);
      fclose(fp);
    }
  }
  return res;
}

int SaveImageFile(void) {
  FILE *fp;
  char name[256];
  int res = SaveImageDialog(name);
  if (!res) {
    if ((fp = fopen(name, "wb")) == NULL) {
      res = -1;
    } else {
      fwrite(&spc, sizeof(spc), 1, fp);
      fclose(fp);
    }
  }
  return res;
}
#endif // __ANDROID__

/*************************************************************/
/** Cassette Tape Processing                                **/
/*************************************************************/
#if !defined(__ANDROID__)
int OpenTapeFile(void) {
  char const *filters[2] = {"*.tap", "*.TAP"};
  char const *name;
  int res = -1;
  name =
      tinyfd_openFileDialog("Read tape file", "", 2, filters, "tape files", 0);
  if (name && (spc.IO.cas.rfp = fopen(name, "rb")) != NULL) {
    res = 0;
  }
  return res;
}

int SaveAsTapeFile(void) {
  char const *filters[2] = {"*.tap", "*.TAP"};
  char const *name;
  char defaultPath[256];
  int res = -1;

  strncpy(defaultPath, &(spc.RAM[0x1397]), 16);
  defaultPath[16] = '\0';
  strcat(defaultPath, ".TAP");
  name = tinyfd_saveFileDialog("Save program to tape file", defaultPath, 2,
                               filters, NULL);
  if (name && (spc.IO.cas.wfp = fopen(name, "wb")) != NULL) {
    res = 0;
  }
  return res;
}
#else
int OpenTapeFile(void) {
  char name[256];
  int res = OpenTapeDialog(name);
  if (!res) {
    if ((spc.IO.cas.rfp = fopen(name, "rb")) == NULL) {
      res = -1;
    }
  }
  return res;
}

int SaveAsTapeFile(void) {
  char name[256] = "\0";
  strncpy(name, &(spc.RAM[0x1397]), 16);
  strcat(name, ".TAP");
  int res = SaveTapeDialog(name);
  if (!res) {
    if ((spc.IO.cas.wfp = fopen(name, "wb")) == NULL) {
      res = -1;
    }
  }
  return res;
}

#endif // __ANDROID__

int ReadVal(void) {
  int c;

  if (spc.IO.cas.rfp != NULL) {
    static int EOF_flag = 0;

    c = fgetc(spc.IO.cas.rfp);
    if (c == EOF) {
      if (!EOF_flag)
        printf("EOF\n"), EOF_flag = 1;
      c = -1;
    } else {
      EOF_flag = 0;
      c -= '0';
    }
    return c;
  }
  return -1;
}

int CasRead(Cassette *cas) {
  Uint32 curTime;
  int bitTime;

  curTime =
      (spc.tick * 125) + ((4000 - spc.Z80R.ICount) >> 5) - spc.IO.cas.startTime;
  bitTime = (cas->rdVal == 0) ? 56 : (cas->rdVal == 1) ? 112 : 0;

  while (curTime >= (cas->cnt0 * 56 + cas->cnt1 * 112 + bitTime)) {
    if (cas->rdVal == 0)
      cas->cnt0++;
    if (cas->rdVal == 1)
      cas->cnt1++;

    cas->rdVal = ReadVal();

    if (cas->rdVal == -1)
      break;

    bitTime = (cas->rdVal == 0) ? 56 : (cas->rdVal == 1) ? 112 : 0;
  }

  curTime -= (cas->cnt0 * 56 + cas->cnt1 * 112);

  switch (cas->rdVal) {
  case 0:
    if (curTime < 28)
      return 1; // high
    else
      return 0; // low

  case 1:
    if (curTime < 56)
      return 1; // high
    else
      return 0; // low
  }
  return 0; // low for other cases
}

void CasWrite(Cassette *cas, int val) {
  Uint32 curTime;

  curTime = (spc.tick * 125) + ((4000 - spc.Z80R.ICount) >> 5);
  if (!cas->wrVal & val) // rising edge
    cas->wrRisingT = curTime;
  if (cas->wrVal & !val) // falling edge
  {
    if (spc.IO.cas.wfp != NULL) {
      if ((curTime - cas->wrRisingT) < 32)
        fputc('0', cas->wfp);
      else
        fputc('1', cas->wfp);
      fflush(cas->wfp);
    }
  }

  cas->wrVal = val;
}

/*************************************************************/
/** Output I/O Processing                                   **/
/*************************************************************/

/**
 * Put a value on Z-80 out port
 * @param Port 0x0000~0xffff port address
 * @param Value a byte value to be written
 * @warning modified to accomodate 64K I/O address from original Marat's source
 */
void OutZ80(register word Port, register byte Value) {
  if ((Port & 0xE000) == 0x0000) // VRAM area
    spc.IO.VRAM[Port] = Value;

  else if ((Port & 0xE000) == 0xA000) // IPLK area
  {
    spc.IO.IPLK = (spc.IO.IPLK) ? 0 : 1; // flip IPLK switch
  } else if ((Port & 0xE000) == 0x2000)  // GMODE setting
  {
    if (spc.IO.GMODE != Value) {
      if (Value & 0x08) // XXX Graphic screen refresh
        SetMC6847Mode(SET_GRAPHIC, Value);
      else
        SetMC6847Mode(SET_TEXTMODE, Value);
    }
    spc.IO.GMODE = Value;
#ifdef DEBUG_MODE
    printf("GMode:%02X\n", Value);
#endif
  } else if ((Port & 0xE000) == 0x6000) // SMODE
  {
    if (spc.IO.cas.button != CAS_STOP) {

      if ((Value & 0x02)) // Motor
      {
        if (spc.IO.cas.pulse == 0) {
          spc.IO.cas.pulse = 1;
        }
      } else {
        if (spc.IO.cas.pulse) {
          spc.IO.cas.pulse = 0;
          if (spc.IO.cas.motor) {
            spc.IO.cas.motor = 0;
#ifdef DEBUG_MODE
            printf("Motor Off\n");
#endif
          } else {
            spc.IO.cas.motor = 1;
#ifdef DEBUG_MODE
            printf("Motor On\n");
#endif
            spc.IO.cas.startTime =
                (spc.tick * 125) + ((4000 - spc.Z80R.ICount) >> 5);
            ResetCassette(&spc.IO.cas);
          }
        }
      }
    }

    if (spc.IO.cas.button == CAS_REC && spc.IO.cas.motor) {
      CasWrite(&spc.IO.cas, Value & 0x01);
    }
  } else if ((Port & 0xFFFE) == 0x4000) // PSG
  {

    if (Port & 0x01) // Data
    {
      Write8910(&spc.IO.ay8910, (byte)spc.IO.psgRegNum, Value);
    } else // Reg Num
    {
      spc.IO.psgRegNum = Value;
      WrCtrl8910(&spc.IO.ay8910, Value);
    }
  }
}

/*************************************************************/
/** Input I/O Processing                                    **/
/*************************************************************/

/**
 * Keyboard hashing table structure
 */
typedef struct {
  int numEntry;
  TKeyMap *keys;
} TKeyHashTab;

/**
 * Keyboard Hashing table definition.
 * initially empty.
 */
TKeyHashTab KeyHashTab[256] = {0, NULL};

/**
 * Build Keyboard Hashing Table
 * Call this once at the initialization phase.
 */
void BuildKeyHashTab(void) {
  int i;
  static int hashPos[256] = {0};

  for (i = 0; spcKeyMap[i].keyMatIdx != -1; i++) {
    int index = spcKeyMap[i].sym % 256;

    KeyHashTab[index].numEntry++;
    hashPos[index]++;
  }

  for (i = 0; i < 256; i++) {
    KeyHashTab[i].keys = malloc(sizeof(TKeyMap) * hashPos[i]);
  }

  for (i = 0; spcKeyMap[i].keyMatIdx != -1; i++) {
    int index = spcKeyMap[i].sym % 256;

    hashPos[index]--;
    if (hashPos[index] < 0)
      printf("Fatal: out of range in %s:BuildKeyHashTab().\n", __FILE__),
          exit(1);
    KeyHashTab[index].keys[hashPos[index]] = spcKeyMap[i];
  }
}

#if SDL_MAJOR_VERSION == 2
#undef SDLK_SCROLLOCK
#define SDLK_SCROLLOCK SDLK_SCROLLLOCK
#endif

/**
 * SDL Key-Down processing. Special Keys only for Emulator
 * @param sym SDL key symbol
 */
void ProcessSpecialKey(SDLKey sym) {
  int index = sym % 256;
  FILE *rfp_save;
  FILE *wfp_save;

  switch (sym) {
  case SDLK_SCROLLOCK:               // turbo mode
    spc.turbo = (spc.turbo) ? 0 : 1; // toggle
    printf("turbo %s\n", (spc.turbo) ? "on" : "off");
    break;
  case SDLK_F8: // PLAY button
    if (spc.IO.cas.rfp != NULL)
      FCLOSE(spc.IO.cas.rfp);
    if (spc.IO.cas.wfp != NULL)
      FCLOSE(spc.IO.cas.wfp);
    if (OpenTapeFile() < 0)
      break;
    spc.IO.cas.button = CAS_PLAY;
    spc.IO.cas.motor = 1;
    spc.IO.cas.startTime = (spc.tick * 125) + ((4000 - spc.Z80R.ICount) >> 5);
    ResetCassette(&spc.IO.cas);
    printf("play button\n");
    break;
  case SDLK_F9: // REC button
    if (spc.IO.cas.rfp != NULL)
      FCLOSE(spc.IO.cas.rfp);
    if (spc.IO.cas.wfp != NULL)
      FCLOSE(spc.IO.cas.wfp);
    if (SaveAsTapeFile() < 0)
      break;
    spc.IO.cas.button = CAS_REC;
    spc.IO.cas.motor = 1;
    ResetCassette(&spc.IO.cas);
    printf("rec button\n");
    break;
  case SDLK_F10: // STOP button
    spc.IO.cas.button = CAS_STOP;
    spc.IO.cas.motor = 0;
    if (spc.IO.cas.rfp != NULL)
      FCLOSE(spc.IO.cas.rfp);
    if (spc.IO.cas.wfp != NULL)
      FCLOSE(spc.IO.cas.wfp);
    printf("stop button\n");
    break;

  case SDLK_PAGEUP: // Image Save
    SaveImageFile();
    simul.curTick = SDL_GetTicks() - simul.baseTick;
    spc.tick = simul.curTick;
    printf("Image Save\n");
    break;

  case SDLK_PAGEDOWN: // Image Load
    if (spc.IO.cas.rfp != NULL)
      FCLOSE(spc.IO.cas.rfp);
    if (spc.IO.cas.wfp != NULL)
      FCLOSE(spc.IO.cas.wfp);
    LoadImageFile();
    simul.curTick = SDL_GetTicks() - simul.baseTick;
    spc.tick = simul.curTick;
    spc.IO.cas.rfp = NULL;
    spc.IO.cas.wfp = NULL;
    spc.IO.cas.button = CAS_STOP;
    spc.IO.cas.motor = 0;
    if (spc.IO.GMODE & 0x08) {
      SetMC6847Mode(SET_GRAPHIC, spc.IO.GMODE);
      UpdateMC6847Gr(MC6847_UPDATEALL);
    } else {
      SetMC6847Mode(SET_TEXTMODE, spc.IO.GMODE);
      UpdateMC6847Text(MC6847_UPDATEALL);
    }
    printf("Image Load\n");
    break;

  case SDLK_F11:
    // No scaling on Android.
#if !defined(__ANDROID__)
    ScaleWindow(-1);
#endif
    break;
  case SDLK_F12: // Reset
    printf("Reset (keeping tape pos.)\n");
    rfp_save = spc.IO.cas.rfp;
    wfp_save = spc.IO.cas.wfp;
    InitIOSpace();
    SndQueueInit();
    SetMC6847Mode(SET_TEXTMODE, 0);
    spc.IO.cas.rfp = rfp_save;
    spc.IO.cas.wfp = wfp_save;

    spc.Z80R.ICount = I_PERIOD;
    simul.baseTick = SDL_GetTicks();
    simul.prevTick = simul.baseTick;
    spc.intrTime = INTR_PERIOD;
    spc.tick = 0;
    ResetZ80(&spc.Z80R);
    break;
  }
}

/**
 * SDL Key-Down processing. Search Hash table and set appropriate keyboard
 * matrix.
 * @param sym SDL key symbol
 */
void ProcessKeyDown(SDLKey sym) {
  int i;
  int index = sym % 256;

  for (i = 0; i < KeyHashTab[index].numEntry; i++) {
    if (KeyHashTab[index].keys[i].sym == sym) {
      spc.IO.keyMatrix[KeyHashTab[index].keys[i].keyMatIdx] &=
          ~(KeyHashTab[index].keys[i].keyMask);
#ifdef DEBUG_MODE
      printf("%08x [%s] key down\n", KeyHashTab[index].keys[i].sym,
             KeyHashTab[index].keys[i].keyName);
#endif
      break;
    }
  }
}

/**
 * SDL Key-Up processing. Search Hash table and set appropriate keyboard matrix.
 * @param sym SDL key symbol
 */
void ProcessKeyUp(SDLKey sym) {
  int i;
  int index = sym % 256;

  for (i = 0; i < KeyHashTab[index].numEntry; i++) {
    if (KeyHashTab[index].keys[i].sym == sym) {
      spc.IO.keyMatrix[KeyHashTab[index].keys[i].keyMatIdx] |=
          (KeyHashTab[index].keys[i].keyMask);
#ifdef DEBUG_MODE
      printf("%08x [%s] key up\n", KeyHashTab[index].keys[i].sym,
             KeyHashTab[index].keys[i].keyName);
#endif
      break;
    }
  }
}

/**
 * Check if any keyboard event exists and process it
 */
void CheckKeyboard(void) {
  SDL_Event event; // SDL event

  while (SDL_PollEvent(&event) > 0) {
    switch (event.type) {
    case SDL_KEYDOWN:
      ProcessSpecialKey(event.key.keysym.sym);
      ProcessKeyDown(event.key.keysym.sym);
      break;
    case SDL_KEYUP:
      ProcessKeyUp(event.key.keysym.sym);
      break;
    case SDL_QUIT:
      printf("Quit requested, quitting.\n");
      exit(0);
      break;
    }
  }
}

/**
 * Read and trash any remaining keyboard event
 */
void flushSDLKeys(void) {
  SDL_Event event; // SDL event
  int cnt = 0;

  while (SDL_PollEvent(&event) > 0) {
    cnt++;
  }
  printf("Total %d events flushed.\n", cnt);
}

/**
 * Read a value on Z-80 port
 * @param Port 0x0000~0xffff port address
 * @warning modified to accomodate 64K I/O address from original Marat's source
 */
byte InZ80(register word Port) {
  // Keyboard Matrix
  if (Port >= 0x8000 && Port <= 0x8009) {
    if (!(spc.IO.cas.motor && spcConfig.casTurbo))
      CheckKeyboard();
    switch (Port) {
    case 0x8000:
    case 0x8001:
    case 0x8002:
    case 0x8003:
    case 0x8004:
    case 0x8005:
    case 0x8006:
    case 0x8007:
    case 0x8008:
    case 0x8009:
      return spc.IO.keyMatrix[Port - 0x8000];
      break;
    }
    return 0xff;
  } else if ((Port & 0xE000) == 0xA000) {
    spc.IO.IPLK = (spc.IO.IPLK) ? 0 : 1;
  } else if ((Port & 0xE000) == 0x2000) {
    return spc.IO.GMODE;
  } else if ((Port & 0xE000) == 0x0000) {
    return spc.IO.VRAM[Port];
  } else if ((Port & 0xFFFE) == 0x4000) {
    // Data
    if (Port & 0x01) {
      if (spc.IO.psgRegNum == 14) {
        byte retval = 0x1f;
        // 0x80 - cassette data input
        // 0x40 - motor status
        if (spc.IO.cas.button == CAS_PLAY && spc.IO.cas.motor) {
          if (CasRead(&spc.IO.cas) == 1)
            retval |= 0x80; // high
          else
            retval &= 0x7f; // low
        }
        if (spc.IO.cas.motor)
          return (retval & (~(0x40))); // 0 indicates Motor On
        else
          return (retval | 0x40);

      } else
        return RdData8910(&spc.IO.ay8910);
    }
  }
  return 0;
}

void setModernKeyLayout() {
    spc.ROM[0x1311] = 0x27; // 3b '(AT), :(SPC)
    spc.ROM[0x1320] = 0x3d;
    spc.ROM[0x1331] = 0x7c;
    spc.ROM[0x1341] = 0x5d;
    spc.ROM[0x1346] = 0x60;
    spc.ROM[0x1350] = 0x7f;
    spc.ROM[0x1351] = 0x29;
    spc.ROM[0x1352] = 0x3a; // 22 :(AT), +(SPC)
    spc.ROM[0x1355] = 0x28;
    spc.ROM[0x1359] = 0x22; // 3a "(AT), *(SPC)
    spc.ROM[0x135d] = 0x2a;
    spc.ROM[0x1365] = 0x26;
    spc.ROM[0x1368] = 0x2b;
    spc.ROM[0x136d] = 0x5e; // 53
    spc.ROM[0x1379] = 0x5c;
    spc.ROM[0x138d] = 0x40;
    spc.ROM[0x138e] = 0x7e;
}

/*************************************************************/
/** Misc. Callback Functions                                **/
/*************************************************************/

void CheckHook(register Z80 *R) {}

void PatchZ80(register Z80 *R) {}

word LoopZ80(register Z80 *R) { return INT_NONE; }

void ShowCredit(void) {
  printf("[SPC-1000 Emulator V%s]\n\n", SPC_EMUL_VERSION);
  printf("Written by ionique (K.-H. Sihn).\n");
  printf("Thanks to zanny, loderunner, zzapuno, kosmo, mayhouse.\n");
  printf("contact: http://blog.naver.com/ionique\n\n");

  printf("This emulator uses Z-80 and AY-3-8910 emulation from Marat "
         "Fayzullin.\n");
  printf("Find more about them at http://fms.komkon.org/\n\n");

  printf("Brief usage of keyboard:\n");
  printf("F8 : cassette PLAY button\n");
  printf("F9 : cassette REC button\n");
  printf("F10: cassette STOP button\n");
  printf("F11: Change screen size\n");
  printf("F12: Reset (keeping tape position)\n");
  printf("Scroll Lock: Turbo mode\n");
  printf("TAB: LOCK key\n");
  printf("PgUp/PgDn: Save/Load current status\n\n");

  printf("For other settings, see SPCEMUL.INI\n");

  printf("This program is freeware, provided with no warranty.\n");
  printf("The author takes no responsibility for ");
  printf("any damage or legal issue\ncaused by using this program.\n");
}

/**
 * Starting point and main loop for SPC-1000 emulator
 */
int main(int argc, char *argv[]) {
  Z80 *R = &spc.Z80R; // Z-80 register
  int prevTurboState = 0;
  int turboState = 0;

#if defined(__ANDROID__)
  unsigned char ini_buf[512];
  memset(ini_buf, 0, 512);
  Bootup(spc.ROM, ini_buf);
  ReadINI(ini_buf, strlen(ini_buf));
#else
  if (argc >= 2 &&
      (!strcmp("-h", argv[1]) || !strcmp("--help", argv[1]))) {
    ShowCredit();
    exit(1);
  }
  FILE *fp;
  if ((fp = fopen("spcall.rom", "rb")) == NULL) {
    printf("spcall.rom (32KB) not found.\n");
    exit(1);
  }
  fread(spc.ROM, 1, 32768, fp); // read ROM file
  fclose(fp);

  if ((fp = fopen("SPCEMUL.INI", "rb")) == NULL) {
    printf("SPCEMUL.INI not found.\n");
    exit(1);
  }
  fseek(fp, 0, SEEK_END);
  size_t size = ftell(fp);
  fseek(fp, 0, SEEK_SET);

  char *ini = (char *)malloc(size);
  fread(ini, 1, size, fp);
  ReadINI(ini, size);
  free(ini);
  fclose(fp);
#endif

  if (spcConfig.keyLayout) setModernKeyLayout();
  InitIOSpace();
  InitMC6847(spc.IO.VRAM, spcConfig.scale,
	     spcConfig.font == 0 ? NULL : &spc.RAM[0x524A]);
  SetMC6847Mode(SET_TEXTPAGE, 0); // set text page to 0
  OpenSoundDevice();
  BuildKeyHashTab(); // Init keyboard hash table

  ResetZ80(R);

  R->ICount = I_PERIOD;
  simul.baseTick = SDL_GetTicks();
  simul.prevTick = simul.baseTick;
  spc.intrTime = INTR_PERIOD;
  spc.tick = 0;
  spc.refrTimer = 0;                 // timer for screen refresh
  spc.refrSet = spcConfig.frameSkip; // init value for screen refresh timer

  Loop8910(&spc.IO.ay8910, 0);

  while (1) {
    // Special Processing, if needed
    CheckHook(R);

    // 1 msec counter expired
    if (R->ICount <= 0) {
      spc.tick++;            // advance spc tick by 1 msec
      R->ICount += I_PERIOD; // re-init counter (including compensation)
      spc.intrTime -= 1.0;

      // 1/60 sec interrupt timer expired
      if (spc.intrTime < 0) {
        spc.intrTime += INTR_PERIOD; // re-init interrupt timer

        // check refresh timer
        if (spc.refrTimer <= 0) {
          if (spc.IO.GMODE & 0x08)
            UpdateMC6847Gr(MC6847_UPDATEALL);
          else
            UpdateMC6847Text(MC6847_UPDATEALL);

          spc.refrTimer = spc.refrSet; // re-init refresh timer
          if (spc.IO.cas.motor && spcConfig.casTurbo)
            CheckKeyboard(); // check keyboard if cassette motor is on
        }
        if (spc.refrSet) // decrease refresh timer if refreshSet > 0
          spc.refrTimer--;

        if (turboState) // if turbo state, refresh only per 1/3 sec
          spc.refrSet = 20;
        else {
          // restore refresh timer start value
          if (spc.refrSet != spcConfig.frameSkip) {
            spc.refrSet = spcConfig.frameSkip;
            spc.refrTimer = 0;
          }
        }

        // if interrupt enabled, call Z-80 interrupt routine
        if (R->IFF & IFF_EI) {
          R->IFF |= IFF_IM1 | IFF_1;
          IntZ80(R, 0);
        }
      }

      simul.curTick = SDL_GetTicks() - simul.baseTick; // current simulator tick

      // elapsed >= 1msec?
      if (simul.curTick - simul.prevTick > 0) {
        Loop8910(&spc.IO.ay8910, simul.curTick - simul.prevTick);
        // call AY-3-8910 routine periodically
        simul.prevTick = simul.curTick;
      }

      turboState = (spc.IO.cas.motor && spcConfig.casTurbo) || spc.turbo;
      if (prevTurboState && !turboState && simul.curTick < spc.tick) {
        spc.tick = simul.curTick; // adjust timing if turbo state turned off
      }
      prevTurboState = turboState;

      // Simulation is faster than real
      while (!turboState && (simul.curTick < spc.tick)) {
        SDL_Delay(1); // Put delay
        simul.curTick = SDL_GetTicks() - simul.baseTick;
        // Loop8910(&spc.IO.ay8910, curTick - prevTick);
      }
    }

    ExecZ80(R); // Z-80 emulation
  }
  CloseMC6847();
  return 0;
}
