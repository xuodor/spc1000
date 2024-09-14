#pragma once

#include <stdio.h>

#include "common.h"

#define CAS_STOP 0
#define CAS_PLAY 1
#define CAS_REC 2


/**
 * Cassette structure for tape processing, included in the SPCIO
 */
typedef struct {
  int motor; // Motor Status
  int pulse; // Motor Pulse (0->1->0) causes motor state to flip
  int button;
  int rdVal;
  uint32 startTime;
  uint32 cnt0, cnt1;

  int wrVal;
  uint32 wrRisingT; // rising time

  FILE *wfp;
  FILE *rfp;
  int dos;  // DOS command signal
} Cassette;

void ResetCassette(Cassette *cas);
void InitCassette(Cassette *cas);
int ReadVal(Cassette *cas);
int CasRead(Cassette *cas);
void CasWrite(Cassette *cas, int val);
