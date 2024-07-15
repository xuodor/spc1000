#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Z80.h"
#include "dos.h"

/*
 * Handles disk-like operations with cassette interface.
 *
 * TODO:
 *  - Devise dialogbox-less UI for image save/load
 *  - Support more files (15 for now)
 */

void dos_putc(DosBuf *db, byte b) {
  db->buf[db->p++] = b;
  db->len++;
}

void dos_put_byte(DosBuf *db, byte b) {
  dos_putc(db, '1');
  for (int i = 7; i >= 0; --i)
    dos_putc(db, (b >> i) & 1 ? '1' : '0');
}

void dos_rewind(DosBuf *db) {
  db->p = 0;
  printf("dos_rewind left: %ld\n", db->len - db->p);
}

void dos_reset(DosBuf *db) {
  printf("dosbuf size: %ld\n", (sizeof db->buf));
  memset(db->buf, 0, sizeof db->buf);
  db->len = 0;
  db->p = 0;
}

int dos_hasdata(DosBuf *db) {
  return db->len > 0;
}

int dos_read(DosBuf* db) {
  if (db->p < db->len) {
    return db->buf[db->p++] - '0';
  } else {
    return -1;
  }
}

word read_checksum(byte* buf, int nbytes) {
  word res = 0;
  for (int i = 0; i < nbytes; ++i) {
    buf++;  /* skip the first bit - always '1' */
    for (int k = 0; k < 8; ++k)
      res += (*buf++ - '0');
  }
  return res;
}

word calc_checksum(byte* buf, int nbytes) {
  word res = 0;
  for (int i = 0; i < nbytes; ++i) {
    for (int k = 0; k < 8; ++k)
      res += (*buf >> (7-k)) & 1 ? 1 : 0;
  }
  return res;

}

byte bit9ToByte(byte* buf) {
  byte res = 0;
  buf++;
  for (int i = 0; i < 8; ++i) {
    res += ((buf[i] - '0') << (7 - i));
  }
  return res;
}

byte get_command_code(byte* buf) {
  byte *p = buf + OFFSET_PREAMBLE;
  return bit9ToByte(p);
}

void build_list_fcb(DosBuf *db) {
  byte out[0x80 + 2]; // fcb + checksum
  byte *outp = out;
  glob_t globbuf;

  memset(out, 0, 0x80 + 2);
  dos_reset(db);

  int result = glob("*.TAP", 0, 0, &globbuf);
  if (result == 0) {
    byte nfiles = MIN(globbuf.gl_pathc, MAX_FILES);
    *outp++ = nfiles;
    for (int i = 0; i < nfiles; ++i) {
      char n[16];
      strcpy(n, globbuf.gl_pathv[i]);
      n[strlen(n)-4] = '\0'; /* remove .TAP */
      strncpy(outp, n, 7);
      outp += 8;
    }
    globfree(&globbuf);
  }
  word cs = calc_checksum(out, 0x80);
  outp = out + 0x80;
  *outp++ = cs % 256;
  *outp++ = cs / 256;

  for (int i = 0; i < 0x55f0; ++i) dos_putc(db, '0');
  for (int i = 0; i < 0x28; ++i) dos_putc(db, '1');
  for (int i = 0; i < 0x28; ++i) dos_putc(db, '0');
  dos_putc(db, '1');
  for (int i = 0; i < 0x80 + 2; ++i) dos_put_byte(db, out[i]);
  dos_putc(db, '1');
  for (int i = 0; i < 0x100; ++i) dos_putc(db, '0');
  for (int i = 0; i < 0x80 + 2; ++i) dos_put_byte(db, out[i]);
  dos_putc(db, '1');
  dos_rewind(db);
}

void set_filename(byte* buf, char* filename) {
  byte *p = buf + OFFSET_PREAMBLE + 9;
  // Max filename length = 7
  for (int i = 0; i < 7; ++i) {
    if ((filename[i] = bit9ToByte(p)) == 0) break;
    p += 9;
  }
  strcat(filename, ".TAP");
}

int exec_doscmd(DosBuf *db, Cassette *cas, Uint32 start_time) {
  byte filename[16+1];
  byte cmd = get_command_code(db->buf);
  int res = 0;

  switch (cmd) {
  case DOSCMD_VIEW:
    /* Prepare the response fcb in |db|. */
    build_list_fcb(db);

    cas->button = CAS_PLAY;
    cas->startTime = start_time;
    res = 1;
    break;
  case DOSCMD_LOAD:
    set_filename(db->buf, filename);
    dos_reset(db);
    if (cas->rfp) fclose(cas->rfp);
    cas->rfp = fopen(filename, "rb");

    /* File not found. Notify user with an error. */
    if (!cas->rfp)
      cas->rfp = fopen(FNF_TAP, "rb");

    cas->button = CAS_PLAY;
    cas->startTime = start_time;
    res = 1;

    break;
  case DOSCMD_SAVE:
    set_filename(db->buf, filename);
    dos_reset(db);
    if ((cas->wfp = fopen(filename, "wb")) != NULL) {
      cas->button = CAS_REC;
      res = 1;
    }
    break;
  case DOSCMD_DEL:
    set_filename(db->buf, filename);
    dos_reset(db);
    remove(filename);
    /* fall through */
  default:
    /* No cmd received. Emulate STOP button. */
    cas->button = CAS_STOP;
    dos_reset(db);
    break;
  }
  return res;
}

void info_dosv_fcb(byte* fcb) {
  byte fcb_name[16+1];
  word len;
  word start;
  word ipl;
  word checksum;

  byte *p = fcb + OFFSET_PREAMBLE;

  printf("attr: %02x\n", bit9ToByte(p));
  p += 9;
  memset(fcb_name, 0, 16+1);
  for (int k = 0; k < 16; ++k) {
    fcb_name[k] = bit9ToByte(p);
    p += 9;
  }
  printf("name: [%s]\n", fcb_name);
  len = bit9ToByte(p) + bit9ToByte(p+9)*256;
  printf("len: %d(0x%x)\n", (unsigned int)len, (unsigned int)len);
  p += 2 * 9;
  start = bit9ToByte(p) + bit9ToByte(p+9)*256;
  printf("start: 0x%x\n", (unsigned int)start);
  p += 2 * 9;
  ipl = bit9ToByte(p) + bit9ToByte(p+9)*256;
  printf("ipl: 0x%x\n", (unsigned int)ipl);
  p += 2 * 9;

  /* p == fcb + OFFSET_PREAMBLE + OFFSET_FCB_EXT */

  for (int i = 0; i < 15; ++i) {
    byte name[7+1];
    memset(name, 0, 7+1);
    for (int k = 0; k < 7; ++k) {
        name[k] = bit9ToByte(p);
        p += 9;
    }
    printf("[%s]\n", name);
  }

  checksum = bit9ToByte(p) + bit9ToByte(p+9)*256;
  printf("checksum: 0x%04x\n", (unsigned int)checksum);
}

/*
void main() {
  byte fcb[FIB_TAP_SIZE];
  build_list_fcb(DOS_LISTFILE);
  FILE *fp = fopen(DOS_LISTFILE, "rb");
  fread(fcb, 1, FIB_TAP_SIZE, fp);
  fclose(fp);
  info_dosv_fcb(fcb);
}

*/
