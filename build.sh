#!/bin/bash

BIN=build/bin/spc1000

z80asm -i src/dos.asm

echo "$(stat -c%s "a.bin") - 8"  | awk '{printf("expr %s", $0); }' | sh  | awk '{printf("tool/build_img.py spcdos-base.rom spcdos.rom 69b2 a.bin %d\n", $0); }' | sh

python tool/gen_romc.py ./spcdos.rom > src/rom.c

rm spcdos.rom
rm a.bin

mkdir -p build/bin
cmake -Bbuild/bin -H.
make -C build/bin

if [ -f ${BIN} ]; then
  echo
  ls -al ${BIN}
  echo
  echo ${BIN} ready.
fi
