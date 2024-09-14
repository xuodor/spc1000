/**
 * @file common.c
 * @brief SPC-1000 emulator config structure
 * @author Kue-Hwan Sihn (ionique)
 * @date 2005~2007.1
 */

#pragma once

#define USE_SDL

#include <stdint.h>

typedef struct
{
	int colorMode;
	int scanLine;
	int frameSkip;
	int casTurbo;
	int soundVol;
	int scale;
        int keyLayout;
        int font;
} SPCConfig;

typedef unsigned char byte;
typedef unsigned long uint32;
typedef int bool;

#define SPCCOL_OLDREV		0
#define SPCCOL_NEW1			1
#define SPCCOL_NEW2			2
#define SPCCOL_GREEN		3

#define SCANLINE_ALL		0
#define SCANLINE_NONE		1
#define SCANLINE_045_ONLY	2

extern SPCConfig spcConfig;

#define MIN(x,y) (((x) <= (y)) ? (x) : (y))
#define MAX(x,y) (((x) <= (y)) ? (y) : (x))

//#define DEBUG_MODE 1

int null_printf(const char *format, ...);

#ifdef DEBUG_MODE
#define DLOG printf
#else
#define DLOG null_printf
#endif

extern uint32_t cas_start_time();
