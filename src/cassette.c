#include "cassette.h"
#include "dos.h"

extern uint32 cas_start_time();
extern DosBuf dosbuf_;

/*************************************************************/
/** Cassette Tape Processing                                **/
/*************************************************************/
/**
 * Reset Cassette structure
 * @param cas cassette structure
 */
void ResetCassette(Cassette *cas) {
  cas->rdVal = -2;
  cas->cnt0 = cas->cnt1 = 0;

  cas->wrVal = 0; // correct location?
  cas->wrRisingT = 0;
}

/**
 * Initialize cassette structure
 * @param cas cassette structure
 */
void InitCassette(Cassette *cas) {
  cas->button = CAS_STOP;

  cas->wfp = NULL;
  cas->rfp = NULL;
  ResetCassette(cas);
}

int ReadVal(Cassette *cas) {
  int c = -1;

  if (dos_hasdata(&dosbuf_)) {
    return dos_read(&dosbuf_);
  } else  if (cas->rfp != NULL) {
    static int EOF_flag = 0;

    c = fgetc(cas->rfp);
    if (c == EOF) {
      if (!EOF_flag) {
        printf("EOF\n");
        EOF_flag = 1;
      }
      c = -1;
    } else {
      EOF_flag = 0;
      c -= '0';
    }
  }
  return c;
}

int CasRead(Cassette *cas) {
  uint32 curTime;
  int bitTime;

  curTime = cas_start_time() - cas->startTime;
  bitTime = (cas->rdVal == 0) ? 56 : (cas->rdVal == 1) ? 112 : 0;

  while (curTime >= (cas->cnt0 * 56 + cas->cnt1 * 112 + bitTime)) {
    if (cas->rdVal == 0)
      cas->cnt0++;
    if (cas->rdVal == 1)
      cas->cnt1++;

    cas->rdVal = ReadVal(cas);

    if (cas->rdVal == -1)
      break;

    bitTime = (cas->rdVal == 0) ? 56 : (cas->rdVal == 1) ? 112 : 0;
  }

  curTime -= (cas->cnt0 * 56 + cas->cnt1 * 112);

  switch (cas->rdVal) {
  case 0:
    if (curTime < 28)
      return 1; // high
    else
      return 0; // low

  case 1:
    if (curTime < 56)
      return 1; // high
    else
      return 0; // low
  }
  return 0; // low for other cases
}

void CasWrite(Cassette *cas, int val) {
  uint32 curTime;

  curTime = cas_start_time();
  if (!cas->wrVal & val) // rising edge
    cas->wrRisingT = curTime;
  if (cas->wrVal & !val) { // falling edge
    byte b = (curTime - cas->wrRisingT) < 32 ? '0' : '1';
    if (cas->wfp != NULL) {
      fputc(b, cas->wfp);
    } else if (cas->dos) {
      dos_putc(&dosbuf_, b);
    }
  }

  cas->wrVal = val;
}

extern int noname_load_;
