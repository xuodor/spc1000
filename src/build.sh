TFD=../external/tfd

gcc -c AY8910.c `sdl2-config --cflags`
gcc -c Debug.c `sdl2-config --cflags`
gcc -c MC6847.c `sdl2-config --cflags`
gcc -c spcmain.c `sdl2-config --cflags` -I ${TFD}
gcc -c Z80SPC.c `sdl2-config --cflags`
gcc -c ${TFD}/tinyfiledialogs.c -I ${TFD}

gcc AY8910.o  Debug.o  MC6847.o  spcmain.o  Z80SPC.o \
    tinyfiledialogs.o \
    `sdl2-config --libs` \
    -o spc
