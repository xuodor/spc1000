#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Z80.h"
#include "dos.h"

#define OFFSET_PREAMBLE (0x55f0 + 0x28 + 0x28 + 1)
#define OFFSET_FCB_EXT ((1 + 17 + 2 + 2 + 2) * 9)
#define MIN(x,y) (((x) <= (y)) ? (x) : (y))
#define MAX_FILES 15
#define FIB_TAP_SIZE 24679
#define FNF_TAP "fnf.bin"

/*
 * Handles disk-like operations with cassette interface.
 *
 * TODO:
 *  - Do not create .dosfile but handle it in memory
 *  - Support more files (15 for now)
 */

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

void fput_byte2bit9(byte b, FILE* fp) {
  fputc('1', fp);
  for (int i = 7; i >= 0; --i)
    fputc((b >> i) & 1 ? '1' : '0', fp);
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

void build_list_fcb(char* filename) {
  byte out[0x80 + 2]; // fcb + checksum
  byte *outp = out;
  glob_t globbuf;

  memset(out, 0, 0x80 + 2);

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

  // Now create a temporary TAP file.
  FILE *fp = fopen(filename, "wb");
  for (int i = 0; i < 0x55f0; ++i) fputc('0', fp);
  for (int i = 0; i < 0x28; ++i) fputc('1', fp);
  for (int i = 0; i < 0x28; ++i) fputc('0', fp);
  fputc('1', fp);
  for (int i = 0; i < 0x80 + 2; ++i) fput_byte2bit9(out[i], fp);
  fputc('1', fp);
  for (int i = 0; i < 0x100; ++i) fputc('0', fp);
  for (int i = 0; i < 0x80 + 2; ++i) fput_byte2bit9(out[i], fp);
  fputc('1', fp);
  fclose(fp);
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

int exec_doscmd(byte *buf, Cassette *cas, Uint32 start_time) {
  byte filename[16+1];
  byte cmd = get_command_code(buf);
  int res = 0;

  switch (cmd) {
  case DOSCMD_VIEW:
    build_list_fcb(DOS_LISTFILE);
    if (cas->rfp) fclose(cas->rfp);
    if ((cas->rfp = fopen(DOS_LISTFILE, "rb")) != NULL) {
      cas->button = CAS_PLAY;
      cas->startTime = start_time;
      res = 1;
    }
    break;
  case DOSCMD_LOAD:
    set_filename(buf, filename);
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
    set_filename(buf, filename);
    if ((cas->wfp = fopen(filename, "wb")) != NULL) {
      cas->button = CAS_REC;
      res = 1;
    }
    break;
  case DOSCMD_DEL:
    set_filename(buf, filename);
    remove(filename);
    /* fall through */
  default:
    /* No cmd received. Emulate STOP button. */
    cas->button = CAS_STOP;
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
