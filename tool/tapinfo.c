#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define OFFSET_PREAMBLE (0x55f0 + 0x28 + 0x28 + 1)
#define OFFSET_FCB_EXT ((1 + 17 + 2 + 2 + 2) * 9)
#define MAX_FILES 15
#define FIB_TAP_SIZE 24679
#define MAX_BODY_TAP (0x2af8+0x14+0x14+(1+256+2)*9+1)
#define FNF_TAP "fnf.bin"
#define CANCEL_TAP "cancel.bin"

typedef unsigned long uint32;
typedef unsigned char byte;
typedef unsigned short word;

byte _9bits_to_byte(byte* buf) {
  byte res = 0;
  buf++;
  for (int i = 0; i < 8; ++i) {
    res += ((buf[i] - '0') << (7 - i));
  }
  return res;
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
  word exec;
  word checksum;
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
  printf("exec: 0x%x\n", (unsigned int)exec);
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

void test_read_file_body(char *filename) {
  byte fcb[FIB_TAP_SIZE];
  byte buf[256*9+1];
  word bl;

  FILE *fp = fopen(filename, "rb");
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

void main(int argc, char *argv[]) {
  test_read_file_body(argv[1]);
}
