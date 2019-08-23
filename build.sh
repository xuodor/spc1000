#!/bin/bash

BIN=build/bin/spc1000

mkdir -p build/bin
cmake -Bbuild/bin -H.
make -C build/bin

if [ -f ${BIN} ]; then
  echo
  ls -al ${BIN}
  echo
  echo ${BIN} ready.
fi
