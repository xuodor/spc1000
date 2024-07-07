#include "spc1000.h"

#define DOS_LISTFILE ".dosfile"
#define DOSCMD_SAVE 0xd3
#define DOSCMD_LOAD 0xd4
#define DOSCMD_VIEW 0xd5
#define DOSCMD_DEL  0xd6

int exec_doscmd(byte *buf, Cassette *cas, Uint32 start_time);
