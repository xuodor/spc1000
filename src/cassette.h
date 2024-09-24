#pragma once

#include <stdio.h>
#include <stdint.h>
#include "common.h"

#define CAS_STOP 0
#define CAS_PLAY 1
#define CAS_REC 2

#define FCLOSE(fp) fclose(fp), fp=NULL

/**
 * Cassette structure for tape processing, included in the SPCIO
 */
typedef struct {
  int motor; // Motor Status
  int pulse; // Motor Pulse (0->1->0) causes motor state to flip
  int button;

  FILE *wfp;
  FILE *rfp;
  int dos;  // DOS command signal
} Cassette;

#ifdef __cplusplus
extern "C" {
#endif

void ResetCassette(Cassette *cas);
void InitCassette(Cassette *cas);
int ReadVal(Cassette *cas);
int CasRead(Cassette *cas);
void CasWrite(Cassette *cas, int val);
void CasIOWrite(Cassette *cas, byte Value);
byte CasIORead(Cassette *cas);

#ifdef __cplusplus
}
#endif
