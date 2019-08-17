/**
 * @file common.c
 * @brief SPC-1000 emulator config structure
 * @author Kue-Hwan Sihn (ionique)
 * @date 2005~2007.1
 */

#ifndef __SPC_COMMON_H__
#define __SPC_COMMON_H__

typedef struct 
{
	int colorMode;
	int scanLine;
	int frameSkip;
	int casTurbo;
	int soundVol;
} SPCConfig;

#define SPCCOL_OLDREV		0
#define SPCCOL_NEW1			1
#define SPCCOL_NEW2			2
#define SPCCOL_GREEN		3

#define SCANLINE_ALL		0
#define SCANLINE_NONE		1
#define SCANLINE_045_ONLY	2

extern SPCConfig spcConfig;

#endif
