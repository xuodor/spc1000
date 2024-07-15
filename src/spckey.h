// SPC-1000 key mapping for SDL
#ifndef SPCKEY_H_
#define SPCKEY_H_

#include "SDL_keycode.h"
#include "SDL_version.h"

#if SDL_MAJOR_VERSION == 2
#define SDLKey SDL_Keycode
#endif

typedef struct
{
	SDLKey sym;
	int keyMatIdx;
	byte keyMask;
	char *keyName; // for debugging
} TKeyMap;

#endif
