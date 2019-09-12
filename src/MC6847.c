/**
 * @file MC6847.h
 * @brief SPC-1000 video emulation module
 * @author Kue-Hwan Sihn (ionique)
 * @date 2005~2006.12
 */

#include "MC6847.h"
#include "common.h"

static unsigned char
    *VRAM;           // VRAM pointer. Real storage is located in spcmain.c
static unsigned char *FONT_BUF_;
int currentPage = 0; // current text page
int XWidth = 0;      // stride for Y+1

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *texture;
SDL_Surface *screen;

Uint8 *frameBuf;
Uint8 bpp;
extern unsigned char CGROM[]; // CGROM, defined at the end of this file
int sdlLockReq;
int screenMode;
Uint32 colorTbl[6][4];  // Color Table for text and mid,hi-res graphics
Uint32 semiColorTbl[9]; // Color Table for semigraphic

enum colorNum {
  COLOR_BLACK,
  COLOR_GREEN,
  COLOR_YELLOW,
  COLOR_BLUE,
  COLOR_RED,
  COLOR_BUFF,
  COLOR_CYAN,
  COLOR_MAGENTA,
  COLOR_ORANGE,
  COLOR_CYANBLUE,
  COLOR_LGREEN,
  COLOR_DGREEN
};

Uint32 colorMap[12]; // color value for the above color index

unsigned char semiGrFont0[16 * 12]; // semigraphic pattern for mode 0
unsigned char semiGrFont1[64 * 12]; // semigraphic pattern for mode 1

void PutSinglePixel(unsigned char *addr, Uint32 c, SDL_Surface *surface) {
  SDL_PixelFormat *fmt = surface->format;
  // Why should the RGB order be reversed to get the right colors?
  *addr++ = ((c & fmt->Bmask) >> fmt->Bshift) << fmt->Bloss;
  *addr++ = ((c & fmt->Gmask) >> fmt->Gshift) << fmt->Gloss;
  *addr++ = ((c & fmt->Rmask) >> fmt->Rshift) << fmt->Rloss;
}

void PutCharPixel(int x, int y, Uint32 c, SDL_Surface *surface) {
  // Pitch doubled to take the scan line into account.
  unsigned int offset = surface->pitch * 2 * y + x * bpp * 2;
  unsigned char *pixels = (unsigned char *)surface->pixels;
  PutSinglePixel(pixels + offset, c, surface);
  PutSinglePixel(pixels + offset + bpp, c, surface);
}

/**
 * Put a text character on SDL surface
 * @param x character x position. in graphics coord. [0..255-8]
 * @param y character y position. in graphics coord. [0..191-8]
 * @param attr attribute value of the character
 * @warning attribute value is not implemented correctly yet.
 */
static void PutChar(int x, int y, int ascii, int attr) {
  unsigned char *fontData;
  int i, j;
  Uint32 fgColor;
  Uint32 bgColor;

  if (attr & 0x04) // semigrahpic
  {
    bgColor = semiColorTbl[0];
    switch (attr & 0x08) {
    case 0:
      fgColor = semiColorTbl[((ascii & 0x70) >> 4) + 1];
      fontData = &semiGrFont0[(ascii & 0x0f) * 12];
      break;
    case 0x08:
      fgColor =
          semiColorTbl[(((attr & 0x02) << 1) | ((ascii & 0xc0) >> 6)) + 1];
      fontData = &semiGrFont1[(ascii & 0x3f) * 12];
      break;
    }
  } else {
    switch (attr & 0x03) {
    case 0x00:
      fgColor = colorTbl[0][1];
      bgColor = colorTbl[0][0];
      break;
    case 0x01:
      fgColor = colorTbl[0][0];
      bgColor = colorTbl[0][1];
      break;
    case 0x02:
      fgColor = colorTbl[0][3];
      bgColor = colorTbl[0][2];
      break;
    case 0x03:
      fgColor = colorTbl[0][2];
      bgColor = colorTbl[0][3];
      break;
    }
    if (ascii < 32 && !(attr & 0x08))
      ascii = 32;
    if ((attr & 0x08) && (ascii < 96))
      ascii += 128;
    if (ascii >= 96 && ascii < 128)
      fontData = &(VRAM[0x1600 + (ascii - 96) * 16]);
    else if (ascii >= 128 && ascii < 224)
      fontData = &(VRAM[0x1000 + (ascii - 128) * 16]);
    else {
      fontData = FONT_BUF_ + (ascii - 32) * 12;  // 8x12
    }
  }

  SDL_Surface *surface = screen;
  unsigned int offset = XWidth * y + x * bpp * 2;
  unsigned char *pixels = (unsigned char *)surface->pixels;
  for (i = 0; i < 12; i++) {
    for (j = 0; j < 8; j++) {
      PutCharPixel(j + x, y + i, fontData[i] & (0x80 >> j) ? fgColor : bgColor,
                   screen);
    }
  }
}

void SetMidResBuf(unsigned char *addr, unsigned char data) {
  Uint32 *scrnColor = colorTbl[screenMode];
  Uint32 c = scrnColor[(0xc0 & data) >> 6];
  int i;
  for (i = 0; i < 4; ++i) {
    PutSinglePixel(addr, c, screen);
    addr += bpp;
  }
  c = scrnColor[(0x30 & data) >> 4];
  for (i = 0; i < 4; ++i) {
    PutSinglePixel(addr, c, screen);
    addr += bpp;
  }
  c = scrnColor[(0x0c & data) >> 2];
  for (i = 0; i < 4; ++i) {
    PutSinglePixel(addr, c, screen);
    addr += bpp;
  }
  c = scrnColor[(0x03 & data)];
  for (i = 0; i < 4; ++i) {
    PutSinglePixel(addr, c, screen);
    addr += bpp;
  }
}

/**
 * Put Mid-resolution graphics pattern for a byte data
 * @param pos byte data position in flat byte index. [0..6144]
 * @param data a byte data to be put
 */
static void PutMidGr(int pos, unsigned char data) {
  int x, y;

  Uint32 *scrnColor;
  scrnColor = colorTbl[screenMode];

  if (pos < 0 || pos >= 6144)
    return;

  x = (pos % 32) * 8;
  y = pos / 32;

  Uint8 *bufp = (Uint8 *)(frameBuf + (y * XWidth) + x * 2 * bpp);
  SetMidResBuf(bufp, data);
  bufp += screen->pitch;
  SetMidResBuf(bufp, data);
}

/**
 * Put Hi-resolution graphics pattern for a byte data
 * @param pos byte data position in flat byte index. [0..6144]
 * @param data a byte data to be put
 */
static void PutHiGr(int pos, unsigned char data) {

  int i;
  int x, y;

  Uint32 fgColor = colorTbl[screenMode][1];
  Uint32 bgColor = colorTbl[screenMode][0];

  if (pos < 0 || pos >= 6144)
    return;
  x = (pos % 32) * 8;
  y = pos / 32;

  unsigned int offset = XWidth * y + x * bpp * 2;
  unsigned char *pixels = (unsigned char *)screen->pixels;
  unsigned char *addr = pixels + offset;

  for (i = 0; i < 16; i++) {
    PutSinglePixel(addr, bgColor, screen);
    addr += bpp;
  }
  addr = pixels + offset;
  for (i = 0; i < 8; i++) {
    if (data & (0x80 >> i)) {
      PutSinglePixel(addr, fgColor, screen);
      addr += bpp;
      PutSinglePixel(addr, fgColor, screen);
    } else {
      PutSinglePixel(addr, bgColor, screen);
      addr += bpp;
      PutSinglePixel(addr, bgColor, screen);
    }
    addr += bpp;
  }
}

void UpdateRect(SDL_Surface *screen, Sint32 x, Sint32 y, Sint32 w, Sint32 h) {
  SDL_Rect r;
  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  SDL_UpdateTexture(texture, NULL, screen->pixels, screen->pitch);
  SDL_RenderCopy(renderer, texture, NULL, &r);
  SDL_RenderPresent(renderer);
}

/**
 * Update text area specified by changeLoc
 * @param changeLoc byte data position in flat byte index. [0..6143]
 */
void UpdateMC6847Text(int changeLoc) {
  int pageStart;
  int attrStart;
  int i, j;
  int x, y; // in text coord.

  pageStart = currentPage * 0x200;
  attrStart = pageStart + 0x800;

  if (changeLoc != MC6847_UPDATEALL) {
    if (!((changeLoc >= pageStart) && (changeLoc < (pageStart + 512))) &&
        !((changeLoc >= attrStart) && (changeLoc < (attrStart + 512))))
      return; // out of range
  }

  if (sdlLockReq)
    if (SDL_LockSurface(screen) < 0)
      return;

  if (changeLoc == MC6847_UPDATEALL) {
    for (i = 0; i < 16; i++)
      for (j = 0; j < 32; j++) {
        PutChar(j * 8, i * 12, VRAM[pageStart + i * 32 + j],
                VRAM[attrStart + i * 32 + j]);
      }
  } else {
    if (changeLoc >= attrStart)
      changeLoc -= 0x800;
    y = (changeLoc - pageStart);
    x = y % 32;
    y = y / 32;
    PutChar(x * 8, y * 12, VRAM[changeLoc], VRAM[changeLoc + 0x800]);
  }

  if (sdlLockReq)
    SDL_UnlockSurface(screen);

  if (changeLoc == MC6847_UPDATEALL) {
    UpdateRect(screen, 0, 0, 512, 384);
  } else {
    UpdateRect(screen, x * 8 * 2, y * 12 * 2, 16, 24);
  }
}

/**
 * Update graphic area specified by changeLoc
 * @param pos byte data position in flat byte index. [0..6144]
 */

void UpdateMC6847Gr(int pos) {
  int i;

  if (sdlLockReq)
    if (SDL_LockSurface(screen) < 0)
      return;

  if (pos != MC6847_UPDATEALL) {
    if (screenMode <= 3)
      PutMidGr(pos, VRAM[pos]);
    else
      PutHiGr(pos, VRAM[pos]);
  } else {
    for (i = 0; i < 6144; i++)
      if (screenMode <= 3)
        PutMidGr(i, VRAM[i]);
      else
        PutHiGr(i, VRAM[i]);
  }

  if (sdlLockReq)
    SDL_UnlockSurface(screen);

  if (pos != MC6847_UPDATEALL)
    UpdateRect(screen, (pos % 32) * 8 * 2, (pos / 32) * 2, 16, 2);
  else
    UpdateRect(screen, 0, 0, 512, 384);
}

/**
 * Set Video mode of MC6874
 * @param command one from { SET_TEXTPAGE, SET_GRAPHIC, SET_TEXTMODE }
 * @param param argument for the command. pagenumber for text mode, gmode for
 * graphic mode
 */
int SetMC6847Mode(int command, int param) {
  SDL_Rect video_rect;
  Uint32 bgColor = SDL_MapRGB(screen->format, 0, 0, 0);
  int prevScreenMode;

  video_rect.x = 0;
  video_rect.y = 0;
  video_rect.w = 512;
  video_rect.h = 192 * 2;

  prevScreenMode = screenMode;
  switch (command) {
  case SET_TEXTPAGE:
    currentPage = param;
    UpdateMC6847Text(MC6847_UPDATEALL);
    break;
  case SET_GRAPHIC:
    switch (param & 0x8e) {
    case 0x80: // 128x96, not implemented
    case 0x0a: // color
    case 0x0c: // mono
      screenMode = 2;
      break;
    case 0x8a: // color
    case 0x8c: // mono
      screenMode = 3;
      break;
    case 0x0e:
      screenMode = 4;
      break;
    case 0x8e:
      screenMode = 5;
      break;
    default:
      screenMode = 5;
      break;
    }
    if (prevScreenMode != screenMode)
      UpdateMC6847Gr(MC6847_UPDATEALL);
    break;
  case SET_TEXTMODE:
    currentPage = (param & 0x30) >> 4;
    screenMode = 0;
    if (sdlLockReq)
      if (SDL_LockSurface(screen) < 0)
        return -1;
    SDL_FillRect(screen, &video_rect, bgColor);
    if (sdlLockReq)
      SDL_UnlockSurface(screen);
    UpdateRect(screen, 0, 0, 512, 384);
    break;
  }

  return 0; // success
}

int greenMode = 0;

/**
 * Set colorMap for MC6847, according to the colorMode
 * @param colorMode = SPCCOL_OLDREV, SPCCOL_NEW1, SPCCOL_NEW2, SPCCOL_GREEN
 */
void MC6847ColorMode(int colorMode) {
  if (colorMode == SPCCOL_GREEN) {
    colorMap[COLOR_BLACK] = SDL_MapRGB(screen->format, 0, 0, 0);
    colorMap[COLOR_GREEN] = SDL_MapRGB(screen->format, 0, 255, 0);
    colorMap[COLOR_YELLOW] = SDL_MapRGB(screen->format, 32, 255, 32);
    colorMap[COLOR_BLUE] = SDL_MapRGB(screen->format, 0, 32, 0);
    colorMap[COLOR_RED] = SDL_MapRGB(screen->format, 0, 100, 0);
    colorMap[COLOR_BUFF] = SDL_MapRGB(screen->format, 0, 24, 0);
    colorMap[COLOR_CYAN] = SDL_MapRGB(screen->format, 0, 224, 0);
    colorMap[COLOR_MAGENTA] = SDL_MapRGB(screen->format, 0, 128, 0);
    colorMap[COLOR_ORANGE] = SDL_MapRGB(screen->format, 0, 224, 0);
    colorMap[COLOR_CYANBLUE] =
        SDL_MapRGB(screen->format, 0, 200, 0); // CYAN/BLUE?
    colorMap[COLOR_LGREEN] = SDL_MapRGB(screen->format, 32, 255, 32);
    colorMap[COLOR_DGREEN] =
        SDL_MapRGB(screen->format, 0, 192, 0); // for screen 2
  } else {
    colorMap[COLOR_BLACK] = SDL_MapRGB(screen->format, 0, 0, 0);
    colorMap[COLOR_GREEN] = SDL_MapRGB(screen->format, 0, 255, 0);
    colorMap[COLOR_YELLOW] = SDL_MapRGB(screen->format, 255, 255, 192);
    colorMap[COLOR_BLUE] = SDL_MapRGB(screen->format, 0, 0, 255);
    colorMap[COLOR_RED] = SDL_MapRGB(screen->format, 255, 0, 0);
    colorMap[COLOR_BUFF] = SDL_MapRGB(screen->format, 96, 0, 0);
    colorMap[COLOR_CYAN] = SDL_MapRGB(screen->format, 0, 255, 255);
    colorMap[COLOR_MAGENTA] = SDL_MapRGB(screen->format, 255, 0, 255);
    colorMap[COLOR_ORANGE] = SDL_MapRGB(screen->format, 255, 128, 0);
    colorMap[COLOR_CYANBLUE] =
        SDL_MapRGB(screen->format, 0, 128, 255); // CYAN/BLUE?
    colorMap[COLOR_LGREEN] = SDL_MapRGB(screen->format, 64, 255, 64);
  }

  // Semi Graphic
  semiColorTbl[0] = colorMap[COLOR_BLACK];
  semiColorTbl[1] = colorMap[COLOR_GREEN];
  semiColorTbl[2] = colorMap[COLOR_YELLOW];
  semiColorTbl[3] = colorMap[COLOR_BLUE];
  semiColorTbl[4] = colorMap[COLOR_RED];
  semiColorTbl[5] = colorMap[COLOR_BUFF];
  semiColorTbl[6] = colorMap[COLOR_CYAN];
  semiColorTbl[7] = colorMap[COLOR_MAGENTA];
  semiColorTbl[8] = colorMap[COLOR_ORANGE];

  // Text Mode
  colorTbl[0][0] = colorMap[COLOR_BLACK];
  colorTbl[0][1] = colorMap[COLOR_GREEN];
  colorTbl[0][2] = colorMap[COLOR_BUFF];
  colorTbl[0][3] = colorMap[COLOR_ORANGE];

  // Screen 2
  colorTbl[2][0] = colorMap[COLOR_GREEN];
  colorTbl[2][1] = colorMap[COLOR_YELLOW];
  colorTbl[2][2] = colorMap[COLOR_BLUE];
  colorTbl[2][3] = colorMap[COLOR_RED];

  // Screen 3
  colorTbl[3][0] = colorMap[COLOR_BLUE];
  colorTbl[3][1] = colorMap[COLOR_CYANBLUE];
  colorTbl[3][2] = colorMap[COLOR_MAGENTA];
  colorTbl[3][3] = colorMap[COLOR_ORANGE];

  // Screen 4
  colorTbl[4][0] = colorMap[COLOR_BLACK];
  colorTbl[4][1] = colorMap[COLOR_GREEN];

  // Screen 5
  colorTbl[5][0] = colorMap[COLOR_BLACK];
  colorTbl[5][1] = colorMap[COLOR_LGREEN];

  switch (colorMode) {
  case SPCCOL_OLDREV:
    colorTbl[0][2] = colorMap[COLOR_BLACK];
    colorTbl[0][3] = colorMap[COLOR_CYAN];

    colorTbl[5][1] = colorMap[COLOR_CYAN];
    break;
  case SPCCOL_NEW1:
    colorTbl[0][2] = colorMap[COLOR_BLACK];
    colorTbl[0][3] = colorMap[COLOR_ORANGE];
    break;
  case SPCCOL_NEW2:
    break;
  case SPCCOL_GREEN:
    colorTbl[2][0] = colorMap[COLOR_DGREEN];
    break;
  }
}

/**
 * Initialize 6847 mode, SDL screen, and semigraphic pattern
 * @param in_VRAM pointer to VRAM array, must be preapred by caller
 * @param cgbuf (0x524A system RAM) containing 8x12 character data
 */
void InitMC6847(unsigned char *in_VRAM, unsigned char *cgbuf) {
  int i, j;
  int width = 512;
  int height = 384;

  VRAM = in_VRAM;
  FONT_BUF_ = cgbuf ? cgbuf : CGROM;

  if ((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1)) {
    printf("Could not initialize SDL: %s.\n", SDL_GetError());
    exit(-1);
  }

  Uint32 wflags = SDL_WINDOW_OPENGL;
  int wwidth = width;
  int wheight = height;

#if defined(__ANDROID__)
  wflags |= SDL_WINDOW_FULLSCREEN;
  wwidth = 0;
  wheight = 0;
#endif

  window = SDL_CreateWindow("SPC-1000 Emulator", SDL_WINDOWPOS_CENTERED,
                            SDL_WINDOWPOS_CENTERED, wwidth, wheight, wflags);
  if (!window) {
    SDL_Log("Unable to create a window: %s\n", SDL_GetError());
    exit(1);
  }
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer) {
    SDL_Log("Unable to create a renderer: %s\n", SDL_GetError());
    exit(1);
  }

// Scales the display to fit the entire device screen.
#if defined(__ANDROID__)
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  SDL_RenderSetLogicalSize(renderer, 512, 384);
#endif

  screen = SDL_CreateRGBSurface(0, width, height, 32, 0, 0, 0, 0);
  if (!screen) {
    SDL_Log("SDL_CreateRGBSurface() failed: %s", SDL_GetError());
    exit(1);
  }

  texture = SDL_CreateTextureFromSurface(renderer, screen);
  if (!texture) {
    SDL_Log("Unable to init texture: %s\n", SDL_GetError());
    exit(1);
  }

  atexit(SDL_Quit);
  bpp = screen->format->BytesPerPixel;
  XWidth = bpp * width * 2; // (2)scanline

  frameBuf = (Uint8 *)screen->pixels;
  MC6847ColorMode(spcConfig.colorMode);

  // initialize semigraphic pattern
  for (i = 0; i < 16; i++)
    for (j = 0; j < 12; j++) {
      unsigned char val = 0;
      if (j < 6) {
        if (i & 0x08)
          val |= 0xf0;
        if (i & 0x04)
          val |= 0x0f;
      } else {
        if (i & 0x02)
          val |= 0xf0;
        if (i & 0x01)
          val |= 0x0f;
      }
      semiGrFont0[i * 12 + j] = val;
    }

  for (i = 0; i < 64; i++)
    for (j = 0; j < 12; j++) {
      unsigned char val = 0;
      if (j < 4) {
        if (i & 0x20)
          val |= 0xf0;
        if (i & 0x10)
          val |= 0x0f;
      } else if (j < 8) {
        if (i & 0x08)
          val |= 0xf0;
        if (i & 0x04)
          val |= 0x0f;
      } else {
        if (i & 0x02)
          val |= 0xf0;
        if (i & 0x01)
          val |= 0x0f;
      }
      semiGrFont1[i * 12 + j] = val;
    }

  sdlLockReq = SDL_MUSTLOCK(screen);
}

void CloseMC6847(void) { SDL_Quit(); }

void SDLWaitQuit(void) {
  SDL_Event event;

  while (SDL_WaitEvent(&event) >= 0) {
    switch (event.type) {
    case SDL_QUIT: {
      printf("Quit requested, quitting.\n");
      exit(0);
    } break;
    }
  }
  SDL_Quit();
}

// clang-format off

/**
 * MC6847 Character Generator Internal ROM Data. (provided by Zanny)
 */
unsigned char CGROM[] =
{
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //32
 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x14, 0x14, 0x3E, 0x14, 0x3E, 0x14, 0x14, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x1E, 0x28, 0x1C, 0x0A, 0x3C, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x10, 0x28, 0x10, 0x28, 0x26, 0x24, 0x1A, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x10, 0x10, 0x08, 0x04, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x04, 0x04, 0x08, 0x10, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x08, 0x1C, 0x3E, 0x1C, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x0C, 0x12, 0x12, 0x12, 0x12, 0x12, 0x0C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x1C, 0x20, 0x20, 0x3E, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x0C, 0x02, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x04, 0x0C, 0x14, 0x24, 0x3E, 0x04, 0x04, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x20, 0x3C, 0x02, 0x02, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x20, 0x3C, 0x22, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x02, 0x02, 0x04, 0x08, 0x08, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1C, 0x22, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x1E, 0x02, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x3E, 0x00, 0x3E, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x02, 0x04, 0x08, 0x00, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x06, 0x0A, 0x0A, 0x06, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x22, 0x22, 0x3C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3C, 0x12, 0x12, 0x12, 0x12, 0x12, 0x3C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x20, 0x20, 0x3C, 0x20, 0x20, 0x3E, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x20, 0x20, 0x3C, 0x20, 0x20, 0x20, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x20, 0x26, 0x22, 0x22, 0x1E, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3E, 0x22, 0x22, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x0E, 0x04, 0x04, 0x04, 0x04, 0x24, 0x18, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3E, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x36, 0x2A, 0x2A, 0x22, 0x22, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x32, 0x2A, 0x26, 0x22, 0x22, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3E, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x20, 0x20, 0x20, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x2A, 0x26, 0x1E, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3C, 0x22, 0x22, 0x3C, 0x28, 0x24, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x22, 0x20, 0x1C, 0x02, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x2A, 0x2A, 0x36, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3E, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x1C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1C, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x40, 0x20, 0x10, 0x08, 0x04, 0x02, 0x01, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x38, 0x08, 0x08, 0x08, 0x08, 0x08, 0x38, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x10, 0x20, 0x7F, 0x20, 0x10, 0x00, 0x00, 0x00, 0x00,

 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, //64
 0x00, 0x00, 0x00, 0x3C, 0x42, 0x42, 0x42, 0x42, 0x3C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3C, 0x7E, 0x7E, 0x7E, 0x7E, 0x3C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x2A, 0x1C, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x04, 0x7E, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x10, 0x10, 0x10, 0x10, 0x10, 0xFF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x10, 0x10, 0x10, 0x10, 0x10, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
 0x10, 0x10, 0x10, 0x10, 0x10, 0xF0, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10,
 0x00, 0x72, 0x8A, 0x8A, 0x72, 0x02, 0x3E, 0x02, 0x3E, 0x20, 0x3E, 0x00,
 0x00, 0x72, 0x8A, 0x72, 0xFA, 0x2E, 0x42, 0x3E, 0x3E, 0x20, 0x3E, 0x00,
 0x00, 0x22, 0x22, 0xFA, 0x02, 0x72, 0x8B, 0x8A, 0x72, 0x22, 0xFA, 0x00,
 0x00, 0x10, 0x10, 0x28, 0x44, 0x82, 0x00, 0xFE, 0x10, 0x10, 0x10, 0x10,
 0x00, 0x7C, 0x44, 0x7C, 0x10, 0xFE, 0x00, 0x7C, 0x04, 0x04, 0x04, 0x00,
 0x00, 0x7C, 0x04, 0x04, 0x00, 0xFE, 0x00, 0x7C, 0x44, 0x44, 0x7C, 0x00,
 0x00, 0x7C, 0x40, 0x78, 0x40, 0x40, 0x7C, 0x10, 0x10, 0x10, 0xFE, 0x00,
 0x00, 0x82, 0x8E, 0x82, 0x8E, 0x82, 0xFA, 0x02, 0x40, 0x40, 0x7E, 0x00,
 0x00, 0x02, 0x22, 0x22, 0x22, 0x52, 0x52, 0x8A, 0x8A, 0x02, 0x02, 0x00,
 0x00, 0x44, 0x7C, 0x44, 0x7C, 0x00, 0xFE, 0x10, 0x50, 0x40, 0x7C, 0x00,
 0x00, 0x10, 0x10, 0xFE, 0x28, 0x44, 0x82, 0x10, 0x10, 0x10, 0xFE, 0x00,
 0x00, 0x01, 0x05, 0xF5, 0x15, 0x15, 0x17, 0x25, 0x45, 0x85, 0x05, 0x00,
 0x00, 0x01, 0x05, 0xF5, 0x85, 0x85, 0x87, 0x85, 0xF5, 0x05, 0x05, 0x00,
 0x00, 0x02, 0x72, 0x8A, 0x8A, 0x8A, 0x72, 0x02, 0x42, 0x40, 0x7E, 0x00,
 0x00, 0x00, 0x7C, 0x40, 0x40, 0x40, 0x7C, 0x10, 0x10, 0x10, 0xFE, 0x00,
 0x00, 0x02, 0x72, 0x8A, 0x72, 0xFA, 0x2E, 0x42, 0x22, 0x20, 0x3E, 0x00,

 0x00, 0x00, 0x00, 0x3E, 0x22, 0x3E, 0x22, 0x3E, 0x00, 0x00, 0x00, 0x00, // 128
 0x00, 0x00, 0x3E, 0x22, 0x3E, 0x22, 0x3E, 0x22, 0x42, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x54, 0x54, 0x10, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x12, 0xFC, 0x38, 0x34, 0x52, 0x91, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x10, 0xFE, 0x10, 0x38, 0x54, 0x92, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x28, 0x7C, 0x92, 0x7C, 0x54, 0xFE, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x10, 0x10, 0x7C, 0x10, 0x10, 0xFE, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x20, 0x7E, 0x80, 0x7C, 0x50, 0xFE, 0x10, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x08, 0xFC, 0xA8, 0xFE, 0xA4, 0xFE, 0x14, 0x04, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x28, 0x44, 0xFE, 0x14, 0x24, 0x48, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x74, 0x24, 0xF5, 0x65, 0xB2, 0xA4, 0x28, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x10, 0x10, 0x54, 0x92, 0x30, 0x10, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x10, 0xFE, 0x10, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x10, 0x10, 0x10, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x28, 0x7C, 0x82, 0x7C, 0x44, 0x7C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x3C, 0x44, 0xA8, 0x10, 0x3E, 0xE2, 0x3E, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x7F, 0x08, 0x7F, 0x08, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x06, 0x18, 0x20, 0x18, 0x06, 0x00, 0x3E, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x30, 0x0C, 0x02, 0x0C, 0x30, 0x00, 0x3E, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x08, 0x08, 0x3E, 0x08, 0x08, 0x00, 0x3E, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x49, 0x7F, 0x49, 0x3E, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x7F, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x7F, 0x22, 0x14, 0x08, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x22, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x08, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x22, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00,
 0x0C, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x60,
 0x00, 0x0F, 0x08, 0x08, 0x08, 0x48, 0xA8, 0x18, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x7E, 0x20, 0x10, 0x20, 0x7E, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x3E, 0x14, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00,

 0x00, 0x60, 0x90, 0x90, 0x90, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 160
 0x00, 0x20, 0x60, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x60, 0x90, 0x20, 0x40, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x60, 0x90, 0x20, 0x90, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x60, 0xA0, 0xA0, 0xF0, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0xF0, 0x80, 0xF0, 0x10, 0xE0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x60, 0x80, 0xF0, 0x90, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0xF0, 0x10, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x60, 0x90, 0x60, 0x90, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x60, 0x90, 0xF0, 0x10, 0x60, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x02, 0x34, 0x48, 0x48, 0x36, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x18, 0x24, 0x38, 0x24, 0x24, 0x38, 0x20, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x4E, 0x30, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x18, 0x24, 0x24, 0x3C, 0x24, 0x24, 0x18, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x1C, 0x20, 0x20, 0x18, 0x24, 0x24, 0x18, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x08, 0x1C, 0x2A, 0x2A, 0x1C, 0x08, 0x00, 0x00, 0x00,
 0x80, 0x40, 0x40, 0x20, 0x10, 0x10, 0x08, 0x04, 0x04, 0x02, 0x01, 0x01,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x7C, 0x00, 0x7C, 0x00, 0xFE, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0xFE, 0xAA, 0xAA, 0xAA, 0xFE, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x7C, 0x10, 0x7C, 0x14, 0x14, 0xFE, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x10, 0xFE, 0x00, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x20, 0x20, 0x20, 0xFE, 0x20, 0x20, 0x3E, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x28, 0x44, 0x82, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x20, 0x20, 0xFC, 0x24, 0x24, 0x44, 0x86, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x36, 0x49, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x18, 0x20, 0x18, 0x24, 0x18, 0x04, 0x18, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x08, 0x22, 0x14, 0x49, 0x14, 0x22, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x38, 0x00, 0x7C, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00,
 0x00, 0x60, 0x90, 0x6E, 0x11, 0x10, 0x10, 0x11, 0x0E, 0x00, 0x00, 0x00,
 0x01, 0x02, 0x02, 0x04, 0x08, 0x08, 0x10, 0x20, 0x20, 0x40, 0x80, 0x80,

 0x00, 0x00, 0x20, 0x10, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 192
 0x00, 0x00, 0x00, 0x00, 0x3C, 0x02, 0x1E, 0x22, 0x1F, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x20, 0x20, 0x2C, 0x32, 0x22, 0x32, 0x2C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x20, 0x22, 0x1C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x02, 0x02, 0x1A, 0x26, 0x22, 0x26, 0x1A, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x3E, 0x20, 0x1E, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x0C, 0x10, 0x10, 0x7C, 0x10, 0x10, 0x10, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x1A, 0x26, 0x22, 0x26, 0x1A, 0x02, 0x1C, 0x00,
 0x00, 0x00, 0x20, 0x20, 0x2C, 0x32, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x08, 0x00, 0x18, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x04, 0x00, 0x0C, 0x04, 0x04, 0x04, 0x24, 0x18, 0x00, 0x00,
 0x00, 0x00, 0x20, 0x20, 0x22, 0x24, 0x28, 0x34, 0x22, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x18, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x76, 0x49, 0x49, 0x49, 0x49, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x2C, 0x32, 0x22, 0x22, 0x22, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x1C, 0x22, 0x22, 0x22, 0x1C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x2C, 0x32, 0x22, 0x32, 0x2C, 0x20, 0x20, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x1A, 0x26, 0x22, 0x26, 0x1A, 0x02, 0x02, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x2E, 0x30, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x1E, 0x20, 0x1C, 0x02, 0x3C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x10, 0x38, 0x10, 0x10, 0x12, 0x0C, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x26, 0x1A, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x49, 0x49, 0x49, 0x49, 0x36, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x22, 0x14, 0x08, 0x14, 0x22, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x26, 0x1A, 0x02, 0x1C, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x3E, 0x04, 0x08, 0x10, 0x3E, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x08, 0x10, 0x10, 0x20, 0x10, 0x10, 0x08, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x54, 0xFE, 0x54, 0xFE, 0x54, 0x28, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x10, 0x08, 0x08, 0x04, 0x08, 0x08, 0x10, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x4C, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x01, 0x3E, 0x54, 0x14, 0x14, 0x00, 0x00, 0x00,

 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // 224
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
};

// clang-format on
