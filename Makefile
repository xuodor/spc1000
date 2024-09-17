all: spcemul-v1 spcmon spcdos

build/bin/Makefile: CMakeLists.txt
	mkdir -p build/bin
	cmake -Bbuild/bin -H.

spcdos: src/spcmain.c src/dos.asm src/dos.c src/cassette.h src/cassette.c src/dos.h src/osd.h src/osd.c src/sysdep.h src/sysdep.c
	z80asm -i src/dos.asm -o dos
	python tool/gen_romc.py ./dos > src/rom.c
	rm dos
	make -C build/bin
	cp build/bin/spc1000 spcdos

spcmon: example/mload.asm
	z80asm -i example/mload.asm -o mon
	python tool/gen_romc.py ./mon > src/rom.c
	rm mon
	make -C build/bin
	cp build/bin/spc1000 spcmon

spcemul-v1: src/spcmain.c
	git checkout org
	make -C build/bin
	cp build/bin/spc1000 spcemul-v1

clean:
	rm -f spcmon spcdos spcemul-v1 src/*.o *.*~ src/*.*~
