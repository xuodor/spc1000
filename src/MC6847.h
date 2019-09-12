#ifndef __MC6847_H__
#define __MC6847_H__

#include "SDL.h"

#include <stdlib.h>
#include <memory.h>

#define SET_TEXTPAGE	1
#define SET_GRAPHIC		2
#define SET_TEXTMODE	3

#define MC6847_UPDATEALL	0xffff

void UpdateMC6847Text(int changLoc);
void UpdateMC6847Gr(int port);
int SetMC6847Mode(int command, int param);
void InitMC6847(unsigned char* in_VRAM, int scale, unsigned char* cgbuf);
void CloseMC6847(void);
void SDLWaitQuit(void);
void ScaleWindow(int scale);

#endif // __MC6847_H__
