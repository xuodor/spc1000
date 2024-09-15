/**
 * @file spcmain.c
 * @brief SPC-1000 emulator main (in,out, and main loop)
 * @author Kue-Hwan Sihn (ionique)
 * @date 2005~2006.10
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "Tables.h"
#include "spc1000.h"
#include "SDL.h"
#include "MC6847.h"

#if defined(__ANDROID__)
#include <android/log.h>
#include "spc1000_jni.h"
#endif

#include "common.h"
#include "dos.h"
#include "osd.h"

#define NONE 0
#define EDGE 1
#define WAITEND 2

#define I_PERIOD 4000
#define INTR_PERIOD 16.6666

#define SPC_EMUL_VERSION "1.1 (2024.07.14)"

SPCConfig spcConfig;

SPCSystem spc;

byte *cgbuf_;

/**
 * SPC simulation structure, realted to the timing
 */
typedef struct {
  uint32_t baseTick, curTick, prevTick;
} SPCSimul;

SPCSimul simul;

TKeyMap spcKeyMap[] = // the last item's keyMatIdx must be -1
{
	{ SDLK_RSHIFT, 0, 0x02, "SHIFT"},
	{ SDLK_LSHIFT, 0, 0x02, "SHIFT"},
	{ SDLK_LCTRL, 0, 0x04, "CTRL" },
	{ SDLK_RCTRL, 0, 0x04, "CTRL" },
	{ SDLK_PAUSE, 0, 0x10, "BREAK" },
	{ SDLK_LALT, 0, 0x40, "GRAPH" },
	{ SDLK_RALT, 0, 0x40, "GRAPH"  },

	{ SDLK_BACKQUOTE, 1, 0x01, "TILDE" },
	{ SDLK_HOME, 1, 0x02, "HOME" },
	{ SDLK_SPACE, 1, 0x04, "SPACE" },
	{ SDLK_RETURN, 1, 0x08, "RETURN" },
	{ SDLK_c, 1, 0x10, "C" },
	{ SDLK_a, 1, 0x20, "A" },
	{ SDLK_q, 1, 0x40, "Q" },
	{ SDLK_1, 1, 0x80, "1" },

	{ SDLK_TAB, 2, 0x01, "CAPS" },
	{ SDLK_z, 2, 0x04, "Z" },
	{ SDLK_RIGHTBRACKET, 2, 0x08, "]" },
	{ SDLK_v, 2, 0x10, "V" },
	{ SDLK_s, 2, 0x20, "S" },
	{ SDLK_w, 2, 0x40, "W" },
	{ SDLK_2, 2, 0x80, "2" },

	{ SDLK_BACKSPACE, 3, 0x01, "DEL" },
	{ SDLK_ESCAPE, 3, 0x04, "ESC" },
	{ SDLK_LEFTBRACKET, 3, 0x08, "]" },
	{ SDLK_b, 3, 0x10, "B" },
	{ SDLK_d, 3, 0x20, "D" },
	{ SDLK_e, 3, 0x40, "E" },
	{ SDLK_3, 3, 0x80, "3" },

	{ SDLK_RIGHT, 4, 0x04, "->" },
	{ SDLK_BACKSLASH, 4, 0x08, "\\" },
	{ SDLK_n, 4, 0x10, "N" },
	{ SDLK_f, 4, 0x20, "F" },
	{ SDLK_r, 4, 0x40, "R" },
	{ SDLK_4, 4, 0x80, "4" },

	{ SDLK_F1, 5, 0x02, "F1" },
	{ SDLK_LEFT, 5, 0x04, "->" },
	{ SDLK_m, 5, 0x10, "M" },
	{ SDLK_g, 5, 0x20, "G" },
	{ SDLK_t, 5, 0x40, "T" },
	{ SDLK_5, 5, 0x80, "5" },

	{ SDLK_F2, 6, 0x02, "F2" },
	{ SDLK_EQUALS, 6, 0x04, "@" },
	{ SDLK_x, 6, 0x08, "X" },
	{ SDLK_COMMA, 6, 0x10, "," },
	{ SDLK_h, 6, 0x20, "H" },
	{ SDLK_y, 6, 0x40, "Y" },
	{ SDLK_6, 6, 0x80, "6" },

	{ SDLK_F3, 7, 0x02, "F3" },
	{ SDLK_UP, 7, 0x04, "UP" },
	{ SDLK_p, 7, 0x08, "P" },
	{ SDLK_PERIOD, 7, 0x10, "." },
	{ SDLK_j, 7, 0x20, "J" },
	{ SDLK_u, 7, 0x40, "U" },
	{ SDLK_7, 7, 0x80, "7" },

	{ SDLK_F4, 8, 0x02, "F4" },
	{ SDLK_DOWN, 8, 0x04, "DN" },
	{ SDLK_QUOTE, 8, 0x08, ":" },
	{ SDLK_SLASH, 8, 0x10, "/" },
	{ SDLK_k, 8, 0x20, "K" },
	{ SDLK_i, 8, 0x40, "I" },
	{ SDLK_8, 8, 0x80, "8" },

	{ SDLK_F5, 9, 0x02, "F5" },
	{ SDLK_MINUS, 9, 0x04, "-" },
	{ SDLK_0, 9, 0x08, "0" },
	{ SDLK_SEMICOLON, 9, 0x10, ";" },
	{ SDLK_l, 9, 0x20, "L" },
	{ SDLK_o, 9, 0x40, "O" },
	{ SDLK_9, 9, 0x80, "9" },

	{ 0, -1, 0, "LAST KEY" }
};

uint32_t cas_start_time() {
  return (spc.tick * 125) + ((4000 - spc.Z80R.ICount) >> 5);
}


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

int null_printf(const char *format, ...) {}
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
    DLOG("The following line in the INI file is ignored:\n\t%s\n", inputstr);
  }
}

/*************************************************************/
/** Initialize IO                                           **/
/*************************************************************/

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

  dosbuf_ = (DosBuf *)malloc(sizeof(DosBuf));
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

/*
 * System Snapshot (Z80+Memory+IO+VRAM) Read/Write
 */

int spc_save_snapshot(char *notused) {
  char name[32];
  time_t t;
  struct tm *tmp;
  FILE *fp;

  t = time(NULL);
  tmp = localtime(&t);
  if (tmp == NULL) {
    perror("Error localtime");
    return -1;
  }

  if (strftime(name, sizeof(name), "%d%H%M%S.sav", tmp) == 0) {
    perror("Error in stftime");
    return -2;
  }

  if ((fp = fopen(name, "wb")) != NULL) {
    fwrite(&spc, sizeof(spc), 1, fp);
    fclose(fp);
    DLOG("Image saved: %s\n", name);

    /* Limit the snapshot files count below 32. Remove older ones. */

    return 0;
  }
  return -3;
}

void spc_load_snapshot(char *filename, char* errmsg) {
  if (filename) {
    FILE *fp = fopen(filename, "rb");
    fread(&spc, sizeof(spc), 1, fp);
    fclose(fp);

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
  }
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
    DLOG("GMode:%02X\n", Value);
  } else if ((Port & 0xE000) == 0x6000) {
    CasIOWrite(&spc.IO.cas, Value);
  } else if ((Port & 0xFFFE) == 0x4000) {
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
void ProcessSpecialKey(SDL_Keysym sym) {
  FILE *rfp_save;
  FILE *wfp_save;

  switch (sym.sym) {
  case SDLK_SCROLLOCK:               // turbo mode
    spc.turbo = (spc.turbo) ? 0 : 1; // toggle
    DLOG("turbo %s\n", (spc.turbo) ? "on" : "off");
    break;

  case SDLK_F10: // STOP button
    spc.IO.cas.button = CAS_STOP;
    spc.IO.cas.motor = 0;
    if (spc.IO.cas.rfp != NULL)
      FCLOSE(spc.IO.cas.rfp);
    if (spc.IO.cas.wfp != NULL)
      FCLOSE(spc.IO.cas.wfp);
    DLOG("stop button\n");
    break;

  case SDLK_PAGEUP: // Image Save
    if (!spc_save_snapshot(NULL)) {
      simul.curTick = SDL_GetTicks() - simul.baseTick;
      spc.tick = simul.curTick;
      osd_toast("SNAPSHOT TAKEN", 0, 0);
    } else {
      osd_toast("SNAPSHOT ERROR", 0, 0);
    }
    break;

  case SDLK_PAGEDOWN: // Image Load
    if (spc.IO.cas.rfp != NULL)
      FCLOSE(spc.IO.cas.rfp);
    if (spc.IO.cas.wfp != NULL)
      FCLOSE(spc.IO.cas.wfp);
    if (!osd_dialog_on()) osd_open_dialog("SNAPSHOT", "*.sav", spc_load_snapshot);
    break;

  case SDLK_F11:
    // No scaling on Android.
#if !defined(__ANDROID__)
    ScaleWindow(-1);
#endif
    break;
  case SDLK_F12: // Reset
    DLOG("Reset (keeping tape pos.)\n");
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
      DLOG("%08x [%s] key down\n", KeyHashTab[index].keys[i].sym,
             KeyHashTab[index].keys[i].keyName);
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
      DLOG("%08x [%s] key up\n", KeyHashTab[index].keys[i].sym,
             KeyHashTab[index].keys[i].keyName);
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
      ProcessSpecialKey(event.key.keysym);
      if (!osd_dialog_on()) ProcessKeyDown(event.key.keysym.sym);
      else osd_process_key(event.key.keysym.sym);
      break;
    case SDL_KEYUP:
      if (!osd_dialog_on()) ProcessKeyUp(event.key.keysym.sym);
      break;
    case SDL_QUIT:
      DLOG("Quit requested, quitting.\n");
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
  DLOG("Total %d events flushed.\n", cnt);
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
        return CasIORead(&spc.IO.cas);
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
  printf("Updated by Jindor (Jinsuk Kim)\n");
  printf("Thanks to zanny, loderunner, zzapuno, kosmo, mayhouse.\n");
  printf("contact: http://blog.naver.com/ionique\n\n");
  printf("         https://github.com/xuodor/spc1000\n\n");
  printf("Z-80/AY-3-8910 emulation from Marat Fayzullin.\n");
  printf("Find more about them at http://fms.komkon.org/\n\n");

  printf("Help:\n");
  printf("F8 : cassette PLAY button\n");
  printf("F9 : cassette REC button\n");
  printf("F10: cassette STOP button\n");
  printf("F11: Change screen size\n");
  printf("F12: Reset (keeping tape position)\n");
  printf("Scroll Lock: Turbo mode\n");
  printf("TAB: LOCK key\n");
  printf("Ins: Save snapshot\n");
  printf("PgUp/PgDn: Load snapshot\n\n");

  printf("For other settings, see SPCEMUL.INI\n");

  printf("This program is a freeware, provided with no warranty.\n");
  printf("The author takes no responsibility for ");
  printf("any damage or legal issue\ncaused by using this program.\n");
}

extern byte rom_image[];

/**
 * Starting point and main loop for SPC-1000 emulator
 */
int main(int argc, char *argv[]) {
  Z80 *R = &spc.Z80R; // Z-80 register
  int prevTurboState = 0;
  int turboState = 0;

  memcpy(spc.ROM, rom_image, 32768);

#if defined(__ANDROID__)
  unsigned char ini_buf[512];
  memset(ini_buf, 0, 512);
  Bootup(ini_buf);
  ReadINI(ini_buf, strlen(ini_buf));
#else
  if (argc >= 2 &&
      (!strcmp("-h", argv[1]) || !strcmp("--help", argv[1]))) {
    ShowCredit();
    exit(1);
  }

  FILE *fp;
  if ((fp = fopen("SPCEMUL.INI", "rb")) == NULL) {
    DLOG("SPCEMUL.INI not found.\n");
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
  cgbuf_ = &spc.RAM[0x524A];
  InitIOSpace();
  InitMC6847(spc.IO.VRAM, spcConfig.scale,
	     spcConfig.font ? cgbuf_ : 0);
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
  free(dosbuf_);
  osd_exit();
  return 0;
}
