// SPC-1000 key mapping for SDL
#pragma once

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

#define KeyCode SDLKey

#define VK_RETURN SDLK_RETURN
#define VK_ESCAPE SDLK_ESCAPE
#define VK_UP     SDLK_UP
#define VK_DOWN   SDLK_DOWN
