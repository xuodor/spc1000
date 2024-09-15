#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dos.h"
#include "osd.h"
#include "sysdep.h"
#include "cassette.h"

/*
 * Handles disk-like operations with cassette interface.
 */

typedef struct {
  Cassette *cas;
  DosBuf *dos_buf;
} LoadData;

DosBuf *dosbuf_;
LoadData load_params_;

byte _9bits_to_byte(byte* buf) {
  byte res = 0;
  buf++;
  for (int i = 0; i < 8; ++i) {
    res += ((buf[i] - '0') << (7 - i));
  }
  return res;
}

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

byte dos_get_command(DosBuf *db) {
  return dos_hasdata(db) ? _9bits_to_byte(db->buf+OFFSET_PREAMBLE) : 0;
}

uint16_t read_checksum(byte* buf, int nbytes) {
  uint16_t res = 0;
  for (int i = 0; i < nbytes; ++i) {
    buf++;  /* skip the first bit - always '1' */
    for (int k = 0; k < 8; ++k)
      res += (*buf++ - '0');
  }
  return res;
}

uint16_t calc_checksum(byte* buf, int nbytes) {
  uint16_t res = 0;
  for (int i = 0; i < nbytes; ++i) {
    for (int k = 0; k < 8; ++k)
      if ((buf[i] >> k) & 0x1) ++res;
  }
  return res;

}

byte get_command_code(byte* buf) {
  byte *p = buf + OFFSET_PREAMBLE;
  return _9bits_to_byte(p);
}

void dos_generate_tap_bits(DosBuf *db, byte *fcb, byte *body, size_t body_len) {
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

void dos_build_list_resp(DosBuf *db) {
  /* fcb + checksum. The code in the extension area can be */
  /* found in fcb.asm. */
  byte fcb[0x80 + 2] = {
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x3a, 0x97, 0x13, 0xb7, 0xc8, 0x2a,
    0xaa, 0x13, 0xc5, 0x06, 0x08, 0x7e, 0x23, 0xb7,
    0x28, 0x06, 0xcd, 0x64, 0x08, 0x05, 0x18, 0xf5,
    0xcd, 0x62, 0x08, 0x10, 0xfb, 0x7e, 0xb7, 0x20,
    0xea, 0x3a, 0x97, 0x13, 0xfe, 0xff, 0x20, 0x06,
    0x28, 0x2e, 0x2e, 0x2e, 0x29, 0x00, 0x00, 0x00,
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
  uint16_t body_len;
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
        strncpy((char*)outp, n, 7);
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
  uint16_t bcs = calc_checksum(body, body_len);
  body[body_len  ] = bcs / 256; /* maybe swap? */
  body[body_len+1] = bcs % 256;

  fcb[0] = 0x01; /* attr (FILMOD) */
  fcb[1] = (byte) nfiles; /* file # (FILNAM) */
  fcb[18] = body_len % 256; /* length (MTBYTE) */
  fcb[19] = body_len / 256;
  fcb[20] = 0x00;  /* start addr (MTADRS) */
  fcb[21] = 0xff;
  fcb[22] = 0xb0;  /* exec point (MTEXEC) */
  fcb[23] = 0x13;
  uint16_t fcs = calc_checksum(fcb, 0x80);
  fcb[0x80] = fcs / 256; /* maybe swap? */
  fcb[0x81] = fcs % 256;

  dos_generate_tap_bits(db, fcb, body, body_len);
}

void dos_build_load_resp(DosBuf *db, char *msg, char *data, size_t body_len) {
  byte *fcb = (byte *)malloc(0x80 + 2);
  byte *body = (byte *)malloc(body_len + 2);
  memset(fcb, 0, 0x80 + 2);
  memcpy(body, data, body_len);

  fcb[0] = 0x01; /* attr (FILMOD) */
  strncpy((char*)fcb+1, msg, 16);
  fcb[18] = body_len % 256; /* length (MTBYTE) */
  fcb[19] = body_len / 256;
  uint16_t fcs = calc_checksum(fcb, 0x80);
  fcb[0x80] = fcs / 256; /* maybe swap? */
  fcb[0x81] = fcs % 256;

  uint16_t bcs = calc_checksum(body, body_len);
  body[body_len  ] = bcs / 256; /* maybe swap? */
  body[body_len+1] = bcs % 256;

  dos_generate_tap_bits(db, fcb, body, body_len);

  free(body);
  free(fcb);
}

void osd_set_filename_(byte* buf, char* filename) {
  byte *p = buf + OFFSET_PREAMBLE + 9;
  for (int i = 0; i < 7; ++i) {
    if ((filename[i] = _9bits_to_byte(p)) == 0) break;
    p += 9;
  }
  if (filename[0] != '\0') strcat(filename, ".TAP");
}

int dos_max_reached() {
  glob_t globbuf;
  int result = glob("*.TAP", 0, 0, &globbuf);
  return result == 0 && globbuf.gl_pathc >= 31;
}

void dos_load(char *filename, char *errmsg) {
  Cassette *cas = load_params_.cas;
  if (cas->rfp) FCLOSE(cas->rfp);
  if (filename) {
    cas->rfp = fopen(filename, "rb");
    /* File open error. Notify user with an error. */
    if (!cas->rfp) {
      dos_build_load_resp(load_params_.dos_buf, "FILE ERROR", "\0\0", 2);
    }
  } else {
    dos_build_load_resp(load_params_.dos_buf, errmsg, "\0\0", 2);
  }
  cas->button = CAS_PLAY;
  cas->startTime = cas_start_time();
  ResetCassette(cas);
}

/**
 * Executes the received dos command.
 * @return 1 if a command was recognized and executed.
 */
int dos_exec(DosBuf *db, Cassette *cas, uint32 start_time) {
  char filename[16+1];
  byte cmd = dos_get_command(db);
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
    osd_set_filename_(db->buf, filename);
    dos_reset(db);
    load_params_.cas = cas;
    load_params_.dos_buf = db;
    if (filename[0] == '\0') {
      if (!cas->rfp) {
        if (!osd_dialog_on()) {
          osd_open_dialog("LOAD", "*.TAP", dos_load);
        }
      } else {
        if (feof(cas->rfp)) {
          dos_build_load_resp(load_params_.dos_buf, "READ ERROR", "\0\0", 2);
          // TODO: After this, force the stop button to not repeat this message
          // upon subsequent LOAD command.
        }
        cas->button = CAS_PLAY;
      }
    } else {
      if (cas->rfp) FCLOSE(cas->rfp);
      dos_load(filename, NULL);
    }
    break;
  case DOSCMD_SAVE:
    osd_set_filename_(db->buf, filename);
    if (filename[0] == '\0') strcpy(filename, "NONAME.TAP");
    if (access(filename, F_OK) == 0) {
      /* TODO: Prevent overwrite. For now we always allow it */
    }
    if (dos_max_reached()) {
      /* Just a warning. */
      /* osd_toast("MAX FILES REACHED", 0, 0); */
    }
    dos_reset(db);
    if ((cas->wfp = fopen(filename, "wb")) == NULL) {
      osd_toast("SAVE ERROR", 0, 0);
    }
    cas->button = CAS_REC;
    res = 1;
    break;
  case DOSCMD_DEL:
    osd_set_filename_(db->buf, filename);
    dos_reset(db);
    remove(filename);
    /* fall through */
  default:
    /* No cmd received. Emulate STOP button. */
    cas->button = CAS_STOP;
    if (cas->rfp) FCLOSE(cas->rfp);
    break;
  }
  return res;
}

//#define TEST

#ifdef TEST
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

uint16_t info_dosv_fcb(byte* fcb) {
  byte fcb_name[17+1];
  uint16_t len;
  uint16_t start;
  uint16_t exec;
  uint16_t checksum;
  byte res[0x80+2+1];

  byte *p = fcb + OFFSET_PREAMBLE;

  for (int i = 0; i < 0x80+2; ++i, p += 9) {
    res[i] = _9bits_to_byte(p);
  }

  dos_dump(res, 0x80+2);
  memcpy(fcb_name, res+1, 17);
  len =   res[18] + res[19]*256;
  start = res[20] + res[21]*256;
  exec =  res[22] + res[23]*256;
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

void info_file_body(byte* buf, uint16_t len) {
  uint16_t checksum;
  byte *res;

  res = (byte *)malloc(len+2);
  for (int i = 0; i < len+2; ++i, buf += 9) {
    res[i] = _9bits_to_byte(buf);
  }
  puts("body:");
  dos_dump(res, len+2);
  checksum = res[len]*256 + res[len+1];
  printf("checksum: read: 0x%04x calc: 0x%04x\n",
         checksum,
         calc_checksum(res, len));
  free(res);
}

void test_read_file_body() {
  byte fcb[FIB_TAP_SIZE];
  byte buf[256*9+1];
  uint16_t bl;

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
