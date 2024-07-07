#ifndef SPC1000_H
#define SPC1000_H

#include "AY8910.h"
#include "MC6847.h"
#include "Z80.h"

/**
 * Cassette structure for tape processing, included in the SPCIO
 */
typedef struct {
  int motor; // Motor Status
  int pulse; // Motor Pulse (0->1->0) causes motor state to flip
  int button;
  int rdVal;
  Uint32 startTime;
  Uint32 cnt0, cnt1;

  int wrVal;
  Uint32 wrRisingT; // rising time

  FILE *wfp;
  FILE *rfp;
  int dos;  // DOS command signal
} Cassette;

/**
 * SPC IO structure, registers and memories mappted to IO space
 */
typedef struct {
  int IPLK;          // IPLK switch for memory selection
  byte VRAM[0x2000]; // Video Memory (6KB)
  byte GMODE;        // GMODE (for video)

  byte psgRegNum; // Keep PSG (AY-3-8910) register number for next turn of I/O

  // int intrState;

  /* SPC-1000 keyboard matrix. Initially turned off. (high) */
  byte keyMatrix[10];

  Cassette cas;

  AY8910 ay8910;
} SPCIO;

/**
 * SPC system structure, Z-80, RAM, ROM, IO(VRAM), etc
 */
typedef struct {
  Z80 Z80R;        // Z-80 register
  byte RAM[65536]; // RAM area
  byte ROM[32768]; // ROM area
  SPCIO IO;
  Uint32 tick;
  int turbo;
  int refrTimer;   // timer for screen refresh
  int refrSet;     // init value for screen refresh timer
  double intrTime; // variable for interrupt timing
} SPCSystem;

#define CAS_STOP 0
#define CAS_PLAY 1
#define CAS_REC 2

#endif /* SPC1000_H */
