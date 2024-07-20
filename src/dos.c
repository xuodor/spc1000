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
      if ((buf[i] >> k) & 0x1) ++res;
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

void dos_build_list_resp(DosBuf *db) {
  /* fcb + checksum */
  byte fcb[0x80 + 2] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x21, 0x00, 0xFF, 0x7E, 0x23, 0xB7,
    0x28, 0x05, 0xCD, 0x64, 0x08, 0x18, 0xF6, 0xCD,
    0x02, 0x08, 0x7E, 0xB7, 0x20, 0xEF, 0xC9, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00
  };

  byte body[0x100 + 2]; /* body + checksum */
  byte *outp = body;
  glob_t globbuf;
  size_t maxbody = 0x100; /* ff00h - ffffh */
  word body_len;
  int nfiles = 0;

  memset(body, 0, 0x100 + 2);

  int result = glob("*.TAP", 0, 0, &globbuf);
  if (result == 0) {
    nfiles = globbuf.gl_pathc;
    for (int i = 0; i < nfiles; ++i) {
      char n[16];
      strcpy(n, globbuf.gl_pathv[i]);
      n[strlen(n)-4] = '\0'; /* remove .TAP */
      n[7] = '\0'; /* truncate up to 7 chars */

      /* +2 for extra null at the end of the list */
      if ((outp-body)+strlen(n)+2 <= maxbody) {
        strncpy(outp, n, 7);
        outp += strlen(n);
        *outp++ = 0;
      } else {
        nfiles = 0xff;  /* Not all files are returned */
        break;
      }
    }
    globfree(&globbuf);
  }

  body_len = outp - body + 1; /* extra zero at the end */
  word bcs = calc_checksum(body, body_len);
  body[body_len  ] = bcs / 256; /* maybe swap? */
  body[body_len+1] = bcs % 256;

  fcb[0] = 0x01; /* attr */
  fcb[1] = (byte) nfiles;
  fcb[18] = body_len % 256;
  fcb[19] = body_len / 256;
  fcb[20] = 0x00;  /* start addr */
  fcb[21] = 0xff;
  fcb[22] = 0xb0;  /* exec */
  fcb[23] = 0x13;
  fcb[24] = 0x3a;  /* auto */
  word fcs = calc_checksum(fcb, 0x80);
  fcb[0x80] = fcs / 256; /* maybe swap? */
  fcb[0x81] = fcs % 256;

  dos_reset(db);

  /* FCB */
  for (int i = 0; i < 0x55f0; ++i) dos_putc(db, '0');
  for (int i = 0; i < 0x28; ++i) dos_putc(db, '1');
  for (int i = 0; i < 0x28; ++i) dos_putc(db, '0');
  dos_putc(db, '1');
  for (int i = 0; i < 0x80 + 2; ++i) dos_put_byte(db, fcb[i]);
  dos_putc(db, '1');
  for (int i = 0; i < 0x100; ++i) dos_putc(db, '0');
  for (int i = 0; i < 0x80 + 2; ++i) dos_put_byte(db, fcb[i]);
  dos_putc(db, '1');

  /* Body */
  for (int i = 0; i < 0x2af8; ++i) dos_putc(db, '0');
  for (int i = 0; i < 0x14; ++i) dos_putc(db, '1');
  for (int i = 0; i < 0x14; ++i) dos_putc(db, '0');
  dos_putc(db, '1');
  for (int i = 0; i < body_len+2; ++i) dos_put_byte(db, body[i]);
  dos_putc(db, '1');
  for (int i = 0; i < 0x100; ++i) dos_putc(db, '0');
  for (int i = 0; i < body_len+2; ++i) dos_put_byte(db, body[i]);
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

int exec_doscmd(DosBuf *db, Cassette *cas, uint32 start_time) {
  byte filename[16+1];
  byte cmd = get_command_code(db->buf);
  int res = 0;

  switch (cmd) {
  case DOSCMD_VIEW:
    dos_reset(db);
    dos_build_list_resp(db);
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

void dos_dump(byte *p, size_t dl) {
  char s[32], bb[4];
  for (int i = 0; i < dl; ++i) {
    if ((i % 8) == 0) {
      if (i != 0) puts(s);
      sprintf(s, "%04x", i);
    }
    sprintf(bb, " %02X", p[i]);
    strcat(s, bb);
  }
  if (dl % 8) puts(s);
}

word info_dosv_fcb(byte* fcb) {
  byte fcb_name[17+1];
  word len;
  word start;
  word ipl;
  word checksum;
  byte res[0x80+2+1];

  byte *p = fcb + OFFSET_PREAMBLE;

  for (int i = 0; i < 0x80+2; ++i, p += 9) {
    res[i] = bit9ToByte(p);
  }

  dos_dump(res, 0x80+2);
  memcpy(fcb_name, res+1, 17);
  len = res[18] + res[19]*256;
  start = res[20] + res[21]*256;
  ipl = res[22] + res[23]*256;
  checksum = res[0x80]*256 + res[0x80+1];

  printf("attr: %02x\n", res[0]);
  printf("len: %d(0x%x)\n", (unsigned int)len, (unsigned int)len);
  printf("name: [%s]\n", fcb_name);
  printf("start: 0x%x\n", (unsigned int)start);
  printf("exec: 0x%x\n", (unsigned int)ipl);
  printf("checksum: read: 0x%04x calc: 0x%04x\n",
         checksum,
         calc_checksum(res, 0x80));
  return len;
}

void info_file_body(byte* buf, word len) {
  word checksum;
  byte *res;

  res = (byte *)malloc(len+2);
  for (int i = 0; i < len+2; ++i, buf += 9) {
    res[i] = bit9ToByte(buf);
  }
  puts("body:");
  dos_dump(res, len+2);
  checksum = res[len]*256 + res[len+1];
  printf("checksum: read: 0x%04x calc: 0x%04x\n",
         checksum,
         calc_checksum(res, len));
  free(res);
}

//#define TEST

#ifdef TEST
void test_read_file_body() {
  byte fcb[FIB_TAP_SIZE];
  byte buf[256*9+1];
  word bl;

  FILE *fp = fopen("load.tap", "rb");
  fread(fcb, 1, FIB_TAP_SIZE, fp);
  bl = info_dosv_fcb(fcb);

  fread(buf, 1, 0x2AF8, fp);  /* 0 * 2AF8h */
  fread(buf, 1, 0x14, fp);      /* 1 * 14 */
  fread(buf, 1, 0x14, fp);      /* 0 * 14 */
  fread(buf, 1, 1, fp);

  fread(buf, 1, (bl+2)*9, fp);
  info_file_body(buf, bl);

  fclose(fp);
}

void main() {
  test_read_file_body();
}
#endif
