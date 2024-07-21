#!/bin/bash

BIN=build/bin/spc1000

z80asm -i src/dos.asm -o spcdos.rom
python tool/gen_romc.py ./spcdos.rom > src/rom.c

rm spcdos.rom

mkdir -p build/bin
cmake -Bbuild/bin -H.
make -C build/bin

if [ -f ${BIN} ]; then
  echo
  ls -al ${BIN}
  echo
  echo ${BIN} ready.
fi
