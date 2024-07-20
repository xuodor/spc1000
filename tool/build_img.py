#!/usr/bin/python

import os
import sys

if __name__ == "__main__":
  if len(sys.argv) < 5:
    print("%s <base-image> <out-image> <offset(hex)> <binfile> [binsize(dec)]" % sys.argv[0])
    sys.exit(0)

  bufsize = os.path.getsize(sys.argv[1])
  fpi = open(sys.argv[1], 'rb')
  buf = fpi.read(bufsize)
  fpi.close()
  print("input: %s" % sys.argv[1])

  fpb = open(sys.argv[4], 'rb')
  binsize = int(sys.argv[5]) if len(sys.argv) == 6 else os.path.getsize(sys.argv[4])
  bin = fpb.read(binsize)
  fpb.close()

  print("binary size: %d" % binsize)

  offset = int(sys.argv[3], 16)
  print("offset: %x" % offset)

  newbuf = buf[:offset] + bin + buf[offset+binsize:]

  fpo = open(sys.argv[2], 'wb')
  fpo.write(newbuf)
  fpo.close()
