#ifndef DOS_H_
#define DOS_H_

#include "common.h"
#include "cassette.h"

#define DOSCMD_SAVE 0xd3
#define DOSCMD_LOAD 0xd4
#define DOSCMD_VIEW 0xd5
#define DOSCMD_DEL  0xd6

#define OFFSET_PREAMBLE (0x55f0 + 0x28 + 0x28 + 1)
#define OFFSET_FCB_EXT ((1 + 17 + 2 + 2 + 2) * 9)
#define MAX_FILES 15
#define FIB_TAP_SIZE 24679
#define MAX_BODY_TAP (0x2af8+0x14+0x14+(1+256+2)*9+1)
#define FNF_TAP "fnf.bin"
#define CANCEL_TAP "cancel.bin"

typedef unsigned long uint32;

typedef struct {
  byte buf[FIB_TAP_SIZE + MAX_BODY_TAP];
  size_t len;
  int p;
} DosBuf;

int dos_exec(DosBuf *db, Cassette *cas, uint32 start_time);
void dos_putc(DosBuf *db, byte b);
void dos_put_byte(DosBuf *db, byte b);
void dos_rewind(DosBuf *db);
void dos_reset(DosBuf *db);
int dos_hasdata(DosBuf *db);
int dos_read(DosBuf* db);
#endif
