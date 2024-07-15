#include "spc1000.h"

#define DOSCMD_SAVE 0xd3
#define DOSCMD_LOAD 0xd4
#define DOSCMD_VIEW 0xd5
#define DOSCMD_DEL  0xd6

#define OFFSET_PREAMBLE (0x55f0 + 0x28 + 0x28 + 1)
#define OFFSET_FCB_EXT ((1 + 17 + 2 + 2 + 2) * 9)
#define MIN(x,y) (((x) <= (y)) ? (x) : (y))
#define MAX_FILES 15
#define FIB_TAP_SIZE 24679
#define FNF_TAP "fnf.bin"

typedef struct {
  byte buf[FIB_TAP_SIZE];
  size_t len;
  int p;
} DosBuf;

int exec_doscmd(DosBuf *db, Cassette *cas, Uint32 start_time);
void dos_putc(DosBuf *db, byte b);
void dos_put_byte(DosBuf *db, byte b);
void dos_rewind(DosBuf *db);
void dos_reset(DosBuf *db);
int dos_hasdata(DosBuf *db);
int dos_read(DosBuf* db);
