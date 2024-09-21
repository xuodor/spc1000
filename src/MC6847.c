/**
 * @file MC6847.h
 * @brief SPC-1000 video emulation module
 * @author Kue-Hwan Sihn (ionique)
 * @date 2005~2006.12
 */
#include "MC6847.h"
#include "common.h"
#include "osd.h"

#include "SDL.h"

#define PAGE_SIZE (32*16)

typedef unsigned char byte;

byte *vram_;
static byte *spc_;

static byte *FONT_BUF_;
static byte *vdg_page_buf;

int currentPage = 0; // current text page

SDL_Window *window_;
SDL_Renderer *renderer_;

typedef struct {
  SDL_Texture *texture;
  SDL_Surface *surface;
  Uint8 bpp;
} Screen;

Screen main_screen_;

/* Current screen. */
Screen *screen_;

int width_ = 512, height_ = 384;
int is_fullscreen_ = 0;

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

void InitWindow(int scale);

void scr_init_bpp(Screen* s) {
  s->bpp = s->surface->format->BytesPerPixel;
}

byte *offset(Screen *s, int x, int y) {
  // Pitch doubled to take the scan line into account.
  return (byte *)s->surface->pixels + 2 * (s->surface->pitch * y + s->bpp * x);
}

void DrawSingleDot(Screen *s, unsigned char *addr, Uint32 c) {
  SDL_PixelFormat *fmt = s->surface->format;
  byte *p = addr;
  // Why should the RGB order be reversed to get the right colors?
  p[0] = ((c & fmt->Bmask) >> fmt->Bshift) << fmt->Bloss;
  p[1] = ((c & fmt->Gmask) >> fmt->Gshift) << fmt->Gloss;
  p[2] = ((c & fmt->Rmask) >> fmt->Rshift) << fmt->Rloss;
}

// Single char dot consists of 2 consecutive dots.
void DrawCharDot(Screen *s, int x, int y, Uint32 c) {
  byte *addr = offset(s, x, y);
  DrawSingleDot(s, addr         , c);
  DrawSingleDot(s, addr + s->bpp, c);
}

/**
 * Put a text character on SDL surface
 * @param x character x position. in graphics coord. [0..255-8]
 * @param y character y position. in graphics coord. [0..191-8]
 * @param attr attribute value of the character
 * @warning attribute value is not implemented correctly yet.
 */
static void PutChar(Screen *s, int x, int y, int ascii, int attr) {
  unsigned char *fontData;
  int i, j;
  Uint32 fgColor;
  Uint32 bgColor;

  if (attr & 0x04) {
    // semigrahpic
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
    if (96 <= ascii && ascii < 128)
      fontData = &(spc_[0x1600 + (ascii - 96) * 16]);
    else if (128 <= ascii && ascii < 224)
      fontData = &(spc_[0x1000 + (ascii - 128) * 16]);
    else {
      fontData = FONT_BUF_ + (ascii - 32) * 12;  // 8x12
    }
  }

  for (i = 0; i < 12; i++) {
    for (j = 0; j < 8; j++) {
      int color = fontData[i] & (0x80 >> j) ? fgColor : bgColor;
      DrawCharDot(s, j + x, y + i, color);
    }
  }
}

void SetMidResBuf(Screen *s, unsigned char *addr, unsigned char data) {
  Uint32 *scrnColor = colorTbl[screenMode];
  Uint32 c = scrnColor[(0xc0 & data) >> 6];
  int i;
  for (i = 0; i < 4; ++i) {
    DrawSingleDot(s, addr, c);
    addr += s->bpp;
  }
  c = scrnColor[(0x30 & data) >> 4];
  for (i = 0; i < 4; ++i) {
    DrawSingleDot(s, addr, c);
    addr += s->bpp;
  }
  c = scrnColor[(0x0c & data) >> 2];
  for (i = 0; i < 4; ++i) {
    DrawSingleDot(s, addr, c);
    addr += s->bpp;
  }
  c = scrnColor[(0x03 & data)];
  for (i = 0; i < 4; ++i) {
    DrawSingleDot(s, addr, c);
    addr += s->bpp;
  }
}

/**
 * Put Mid-resolution graphics pattern for a byte data
 * @param pos byte data position in flat byte index. [0..6144]
 * @param data a byte data to be put
 */
static void PutMidGr(Screen *s, int pos, unsigned char data) {
  int x, y;

  if (pos < 0 || pos >= 6144)
    return;

  x = (pos % 32) * 8;
  y = pos / 32;
  byte *addr = offset(s, x, y);
  SetMidResBuf(s, addr, data);
  addr += s->surface->pitch;
  SetMidResBuf(s, addr, data);
}

/**
 * Put Hi-resolution graphics pattern for a byte data
 * @param pos byte data position in flat byte index. [0..6144]
 * @param data a byte data to be put
 */
static void PutHiGr(Screen *s, int pos, unsigned char data) {
  int i;
  int x, y;
  Uint32 fgColor = colorTbl[screenMode][1];
  Uint32 bgColor = colorTbl[screenMode][0];
  byte *base, *addr;

  if (pos < 0 || pos >= 6144)
    return;
  x = (pos % 32) * 8;
  y = pos / 32;

  base = addr = offset(s, x, y);
  for (i = 0; i < 16; i++) {
    DrawSingleDot(s, addr, bgColor);
    addr += s->bpp;
  }
  addr = base;
  for (i = 0; i < 8; i++) {
    Uint32 color = (data & (0x80 >> i)) ? fgColor : bgColor;
    DrawSingleDot(s, addr, color);
    addr += s->bpp;
    DrawSingleDot(s, addr, color);
    addr += s->bpp;
  }
}

void UpdateRect(Screen *s, Sint32 x, Sint32 y, Sint32 w, Sint32 h) {
  SDL_Rect r;
  r.x = x;
  r.y = y;
  r.w = w;
  r.h = h;
  SDL_UpdateTexture(s->texture, NULL, s->surface->pixels, s->surface->pitch);
  SDL_RenderCopy(renderer_, s->texture, NULL, &r);
  SDL_RenderPresent(renderer_);
}

void vdg_save_page() {
  char *page_addr = spc_ + currentPage * 0x200;
  memcpy(vdg_page_buf, page_addr, PAGE_SIZE);
  memcpy(vdg_page_buf + PAGE_SIZE, page_addr + 0x800, PAGE_SIZE);
}

void vdg_restore_page() {
  char *page_addr = spc_ + currentPage * 0x200;
  memcpy(page_addr, vdg_page_buf, PAGE_SIZE);
  memcpy(page_addr + 0x800, vdg_page_buf + PAGE_SIZE, PAGE_SIZE);
}

inline char *vdg_text_pos(int x, int y) {
  return spc_ + currentPage * 0x200 + y * 32 + x;
}

byte vram_data(int addr) {
  return spc_[addr];
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
    if (SDL_LockSurface(screen_->surface) < 0)
      return;

  if (changeLoc == MC6847_UPDATEALL) {
    for (i = 0; i < 16; i++)
      for (j = 0; j < 32; j++) {
        PutChar(screen_, j * 8, i * 12, vram_data(pageStart + i * 32 + j),
                vram_data(attrStart + i * 32 + j));
      }
  } else {
    if (changeLoc >= attrStart)
      changeLoc -= 0x800;
    y = (changeLoc - pageStart);
    x = y % 32;
    y = y / 32;
    PutChar(screen_, x * 8, y * 12, vram_data(changeLoc), vram_data(changeLoc + 0x800));
  }

  if (sdlLockReq)
    SDL_UnlockSurface(screen_->surface);

  if (changeLoc == MC6847_UPDATEALL) {
    UpdateRect(screen_, 0, 0, width_, height_);
    if (osd_should_close_toast()) osd_show(0);
  } else {
    UpdateRect(screen_,
	       x * 8 * 2,
	       y * 12 * 2,
	       16,
	       24);
  }
}

/**
 * Update graphic area specified by changeLoc
 * @param pos byte data position in flat byte index. [0..6144]
 */

void UpdateMC6847Gr(int pos) {
  int i;

  if (sdlLockReq)
    if (SDL_LockSurface(screen_->surface) < 0)
      return;

  if (pos != MC6847_UPDATEALL) {
    if (screenMode <= 3)
      PutMidGr(screen_, pos, vram_data(pos));
    else
      PutHiGr(screen_, pos, vram_data(pos));
  } else {
    for (i = 0; i < 6144; i++)
      if (screenMode <= 3)
        PutMidGr(screen_, i, vram_data(i));
      else
        PutHiGr(screen_, i, vram_data(i));
  }

  if (sdlLockReq)
    SDL_UnlockSurface(screen_->surface);

  if (pos != MC6847_UPDATEALL)
    UpdateRect(screen_,
	       (pos % 32) * 8 * 2,
	       (pos / 32) * 2,
	       16,
	       2);
  else {
    UpdateRect(screen_, 0, 0, width_, height_);
    if (osd_should_close_toast()) osd_show(0);
  }
}

/**
 * Set Video mode of MC6874
 * @param command one from { SET_TEXTPAGE, SET_GRAPHIC, SET_TEXTMODE }
 * @param param argument for the command. pagenumber for text mode, gmode for
 * graphic mode
 */
int SetMC6847Mode(int command, int param) {
  SDL_Rect video_rect;
  Uint32 bgColor = SDL_MapRGB(screen_->surface->format, 0, 0, 0);
  int prevScreenMode;

  video_rect.x = 0;
  video_rect.y = 0;
  video_rect.w = width_;
  video_rect.h = height_;

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
      if (SDL_LockSurface(screen_->surface) < 0)
        return -1;
    SDL_FillRect(screen_->surface, &video_rect, bgColor);
    if (sdlLockReq)
      SDL_UnlockSurface(screen_->surface);
    UpdateRect(screen_, 0, 0, width_, height_);
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
    colorMap[COLOR_BLACK] = SDL_MapRGB(screen_->surface->format, 0, 0, 0);
    colorMap[COLOR_GREEN] = SDL_MapRGB(screen_->surface->format, 0, 255, 0);
    colorMap[COLOR_YELLOW] = SDL_MapRGB(screen_->surface->format, 32, 255, 32);
    colorMap[COLOR_BLUE] = SDL_MapRGB(screen_->surface->format, 0, 32, 0);
    colorMap[COLOR_RED] = SDL_MapRGB(screen_->surface->format, 0, 100, 0);
    colorMap[COLOR_BUFF] = SDL_MapRGB(screen_->surface->format, 0, 24, 0);
    colorMap[COLOR_CYAN] = SDL_MapRGB(screen_->surface->format, 0, 224, 0);
    colorMap[COLOR_MAGENTA] = SDL_MapRGB(screen_->surface->format, 0, 128, 0);
    colorMap[COLOR_ORANGE] = SDL_MapRGB(screen_->surface->format, 0, 224, 0);
    colorMap[COLOR_CYANBLUE] =
        SDL_MapRGB(screen_->surface->format, 0, 200, 0); // CYAN/BLUE?
    colorMap[COLOR_LGREEN] = SDL_MapRGB(screen_->surface->format, 32, 255, 32);
    colorMap[COLOR_DGREEN] =
        SDL_MapRGB(screen_->surface->format, 0, 192, 0); // for screen 2
  } else {
    colorMap[COLOR_BLACK] = SDL_MapRGB(screen_->surface->format, 0, 0, 0);
    colorMap[COLOR_GREEN] = SDL_MapRGB(screen_->surface->format, 0, 255, 0);
    colorMap[COLOR_YELLOW] = SDL_MapRGB(screen_->surface->format, 255, 255, 192);
    colorMap[COLOR_BLUE] = SDL_MapRGB(screen_->surface->format, 0, 0, 255);
    colorMap[COLOR_RED] = SDL_MapRGB(screen_->surface->format, 255, 0, 0);
    colorMap[COLOR_BUFF] = SDL_MapRGB(screen_->surface->format, 96, 0, 0);
    colorMap[COLOR_CYAN] = SDL_MapRGB(screen_->surface->format, 0, 255, 255);
    colorMap[COLOR_MAGENTA] = SDL_MapRGB(screen_->surface->format, 255, 0, 255);
    colorMap[COLOR_ORANGE] = SDL_MapRGB(screen_->surface->format, 255, 128, 0);
    colorMap[COLOR_CYANBLUE] =
        SDL_MapRGB(screen_->surface->format, 0, 128, 255); // CYAN/BLUE?
    colorMap[COLOR_LGREEN] = SDL_MapRGB(screen_->surface->format, 64, 255, 64);
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
 * @param mem pointer to VRAM array, must be preapred by caller
 * @param scale Screen scale factor. 0 if fullscreen
 * @param cgbuf (0x524A system RAM) containing 8x12 character data
 */
void InitMC6847(byte *mem, int scale, byte *cgbuf) {
  int i, j;

  spc_ = mem;
  vram_ = spc_;
  FONT_BUF_ = cgbuf ? cgbuf : CGROM;

  vdg_page_buf = (byte *)malloc(PAGE_SIZE*2);

  if ((SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) == -1)) {
    printf("Could not initialize SDL: %s.\n", SDL_GetError());
    exit(-1);
  }

  Uint32 wflags = SDL_WINDOW_OPENGL;
  int wwidth = width_;
  int wheight = height_;

#if defined(__ANDROID__)
  // Always use fullscreen on Android.
  wflags |= SDL_WINDOW_FULLSCREEN;
  wwidth = 0;
  wheight = 0;
#endif

  window_ = SDL_CreateWindow("SPC-1000 Emulator", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, wwidth, wheight, wflags);
  if (!window_) {
    SDL_Log("Unable to create a window: %s\n", SDL_GetError());
    exit(1);
  }
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED);
  if (!renderer_) {
    SDL_Log("Unable to create a renderer: %s\n", SDL_GetError());
    exit(1);
  }

  screen_ = &main_screen_;

  osd_init();

  // Scales the display to fit the window which can be resized programmatically.
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
  SDL_RenderSetLogicalSize(renderer_, 512, 384);

  InitWindow(scale);

  atexit(SDL_Quit);
  scr_init_bpp(&main_screen_);
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

  sdlLockReq = SDL_MUSTLOCK(screen_->surface);
}

void init_screen(Screen *screen) {
  screen->surface = SDL_CreateRGBSurface(0, width_, height_, 32, 0, 0, 0, 0);
  if (!screen->surface) {
    SDL_Log("SDL_CreateRGBSurface() failed: %s", SDL_GetError());
    exit(1);
  }
  screen->texture = SDL_CreateTextureFromSurface(renderer_, screen->surface);
  if (!screen->texture) {
    SDL_Log("Unable to init texture: %s\n", SDL_GetError());
    exit(1);
  }
}

/**
 * Init window
 */
#if defined(__ANDROID__)
void InitWindow(int scale) {
  SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);

  init_screen(&main_screen_);
  SDL_RenderClear(renderer_);
  UpdateRect(screen_, 0, 0, width_, height_);
}
#else
void InitWindow(int scale) {
  ScaleWindow(scale);
  // screen_ is initialized in ScaleWindow.
}
#endif

/**
 * Scale window
 * @param scale Scale factor. 0 for fullscreen.
 *         -1 for scaling up the current size by 2
 */
void ScaleWindow(int scale) {
  static int current_scale_ = 1;

  if (!scale && is_fullscreen_) return;
  do {
    if (is_fullscreen_ || (!screen_->surface && !screen_->texture)) {
      // Set to default 512x384 if we were in fullscreen or first-time init.
      SDL_SetWindowFullscreen(window_, 0);
      width_ = 512;
      height_ = 384;
      SDL_SetWindowSize(window_, width_, height_);
      is_fullscreen_ = 0;
      current_scale_ = 1;
    } else {
      // Turn to fullscreen if can't make bigger.
      SDL_DisplayMode mode;
      SDL_GetCurrentDisplayMode(0, &mode);
      if (width_ * 2 > mode.w || height_ * 2 > mode.h) {
        SDL_SetWindowFullscreen(window_, SDL_WINDOW_FULLSCREEN_DESKTOP);
        is_fullscreen_ = 1;
        scale = current_scale_ = 0;
      } else {
        width_ *= 2;
        height_ *= 2;
        current_scale_++;
        SDL_SetWindowSize(window_, width_, height_);
      }
    }
    if (screen_->surface) SDL_FreeSurface(screen_->surface);
    if (screen_->texture) SDL_DestroyTexture(screen_->texture);
    init_screen(screen_);
    if ((is_fullscreen_ && scale == 0) || scale < 0) break;
  } while (scale != current_scale_);
  SDL_RenderClear(renderer_);
  UpdateRect(screen_, 0, 0, width_, height_);
}

void CloseMC6847(void) {
  osd_exit();
  SDL_Quit();
}

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

int vdg_display_char() {
  return screenMode == 0;
}
