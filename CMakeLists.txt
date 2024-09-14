cmake_minimum_required(VERSION 3.4.1)

project(spc1000)
set(SOURCE_FILES \\
    src/AY8910.c \\
    src/MC6847.c \\
    src/spcmain.c \\
    src/Debug.c \\
    src/Z80SPC.c \\
    src/cgbuf.c \\
    src/cassette.c \\
    src/dos.c \\
    src/osd.c \\
    src/rom.c)
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIRS})
add_executable(spc1000 ${SOURCE_FILES} )
target_link_libraries(spc1000 ${SDL2_LIBRARIES})
