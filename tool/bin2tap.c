#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char byte;
typedef unsigned short word;

void dos_putc(FILE *fp, byte b) {
  fputc(b, fp);
}

void dos_put_byte(FILE *fp, byte b) {
  dos_putc(fp, '1');
  for (int i = 7; i >= 0; --i)
    dos_putc(fp, (b >> i) & 1 ? '1' : '0');
}

word calc_checksum(byte* buf, int nbytes) {
  word res = 0;
  for (int i = 0; i < nbytes; ++i) {
    for (int k = 0; k < 8; ++k)
      if ((buf[i] >> k) & 0x1) ++res;
  }
  return res;

}
void dos_build_tap(byte *title, word start, word exec, byte *body, word size) {
  byte fcb[0x80 + 2];

  memset(fcb, 0, 0x80+2);

  FILE *fp = fopen("a.tap", "wb");

  fcb[0] = 0x01; /* attr (FILMOD) */
  memcpy(&fcb[1], title, strlen(title));
  fcb[18] = size % 256; /* length (MTBYTE) */
  fcb[19] = size / 256;
  fcb[20] = start % 256;  /* start addr (MTADRS) */
  fcb[21] = start / 256;
  fcb[22] = exec % 256;  /* exec point (MTEXEC) */
  fcb[23] = exec / 256;
  word fcs = calc_checksum(fcb, 0x80);
  /* WHY FCB checksum bytes are reversed? */
  fcb[0x80] = fcs / 256;
  fcb[0x81] = fcs % 256;

  word bcs = calc_checksum(body, size);
  printf("size: %d checksum: %x\n", size, bcs);
  body[size] = bcs / 256;
  body[size+1] = bcs % 256;

  /* FCB */
  for (int i = 0; i < 0x55f0; ++i) dos_putc(fp, '0');
  for (int i = 0; i < 0x28; ++i) dos_putc(fp, '1');
  for (int i = 0; i < 0x28; ++i) dos_putc(fp, '0');
  dos_putc(fp, '1');
  for (int i = 0; i < 0x80 + 2; ++i) dos_put_byte(fp, fcb[i]);
  dos_putc(fp, '1');
  for (int i = 0; i < 0x100; ++i) dos_putc(fp, '0');
  for (int i = 0; i < 0x80 + 2; ++i) dos_put_byte(fp, fcb[i]);
  dos_putc(fp, '1');


  /* Body */
  for (int i = 0; i < 0x2af8; ++i) dos_putc(fp, '0');
  for (int i = 0; i < 0x14; ++i) dos_putc(fp, '1');
  for (int i = 0; i < 0x14; ++i) dos_putc(fp, '0');
  dos_putc(fp, '1');
  for (int i = 0; i < size+2; ++i) dos_put_byte(fp, body[i]);
  dos_putc(fp, '1');
  for (int i = 0; i < 0x100; ++i) dos_putc(fp, '0');
  for (int i = 0; i < size+2; ++i) dos_put_byte(fp, body[i]);
  dos_putc(fp, '1');

  fclose(fp);
}

word word_from_argv(char *arg) {
  int v;
  sscanf(arg, "%x", &v);
  return (word)v;
}

int main(int argc, char *argv[]) {
  word exec = 0;

  if (argc < 4 || argc > 5) {
    puts("bin2tap <input-bin> <fib-name> <start> [exec]");
    exit(-1);
  }
  FILE *fp = fopen(argv[1], "rb");
  fseek(fp, 0, SEEK_END);
  long sz = ftell(fp);
  byte *buf = (byte *)malloc(sz + 16);
  fseek(fp, 0, SEEK_SET);
  fread(buf, 1, sz, fp);
  fclose(fp);

  word start = word_from_argv(argv[3]);
  if (argc == 5) {
    exec = word_from_argv(argv[4]);
  }
  dos_build_tap(argv[2], /* name */
                start,
                exec,
                buf, (word) sz);
  free(buf);
}
