#pragma once

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

typedef unsigned long uint32;

typedef struct {
  byte buf[(FIB_TAP_SIZE + MAX_BODY_TAP)/8+1];
  unsigned int len;
  int p;
} DosBuf;

#ifdef __cplusplus
extern "C" {
#endif

int dos_exec(DosBuf *db, Cassette *cas, uint32 start_time);
void dos_putb(DosBuf *db, int b);
void dos_rewind(DosBuf *db);
void dos_reset(DosBuf *db);
int dos_hasdata(DosBuf *db);
int dos_read(DosBuf* db);
void dos_build_load_resp(DosBuf *db, char *msg, char *data, size_t body_len);

extern DosBuf *dosbuf_;

#ifdef __cplusplus
}
#endif
