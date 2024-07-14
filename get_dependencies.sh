#!/bin/bash -x

SDL=external/SDL2/SDL2
if [ ! -d "$SDL" ]; then
  pushd external/SDL2
  wget https://www.libsdl.org/release/SDL2-2.0.8.zip
  unzip -q SDL2-2.0.8.zip
  mv SDL2-2.0.8 SDL2
  rm SDL2-2.0.8.zip
  popd
fi

TFD=external/tfd
if [ ! -d "$TFD" ]; then
  pushd external
  wget https://github.com/native-toolkit/tinyfiledialogs/archive/master.zip
  unzip -q master.zip
  mv libtinyfiledialogs-master tfd
  rm master.zip
  popd
fi

# No need to copy SDLActivity.java et al, repo contains those from 2.0.8
# cp external/SDL2/SDL2/android-project/app/src/main/java/org/libsdl/app/*.java android/app/src/main/java/org/libsdl/app/
