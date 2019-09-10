# SPC-1000

## Introduction

This is an emulator for SPC-1000 ([https://en.wikipedia.org/wiki/SPC-1000](https://en.wikipedia.org/wiki/SPC-1000)) ported on Android device. It was branched off of Windows-based one developed and published by Ionique at ([https://blog.naver.com/ionique/10015262896](https://blog.naver.com/ionique/10015262896)).

## Snapshots

<img src="https://github.com/xuodor/spc1000/raw/master/snapshots/spc-android.png" />

You can build binaries for Linux and Mac as well(see below):

### Linux

<img src="https://github.com/xuodor/spc1000/raw/master/snapshots/spc-linux.png" />

### Mac
<img src="https://github.com/xuodor/spc1000/raw/master/snapshots/spc-mac.jpg" />

## Requirements

- JDK and JRE 8
- Android SDK and NDK (with Android Build-tools 28.0.3 and Android Platform API 28, though these are configurable)
- ANDROID_HOME and ANDROID_NDK_HOME environment variables set (or create `local.properties` as instructed by gradle when building)
- Android device (> SDK version 19, Lollipop)

## How to build (command line)

You can download the dependencies, compile your program and then install it on a connected device with the following commands:
```
./get_dependencies  # SDL
cd android
./gradlew assembleDebug
./gradlew installDebug
```
## How to build (as of Android Studio 3.2.1)

First download the dependencies (SDL2) as above or manually like below. Then open the ./android folder as an existing project in Android Studio.

## Downloading dependencies manually

Download the latest source release from SDL website:

https://www.libsdl.org/download-2.0.php

Unzip it, put the SDL2-x.x.x folder in `external/SDL2` and rename them to SDL2 so your project folder looks like this:
```
+ android
+ external
| + SDL2
| | + Android.mk
| | | SDL2
```

## How to build binaries for other platforms (Linux/Mac)
By-products of the project is that the emulator can be built for other platforms thanks to a cross-platform UI package that replaces the Windows-only UI support in V1.0. Currently verified to work on Linux and Mac.

```
./get_dependencies # tinyfiledialogs
sudo apt-get install libsdl2-dev
./build.sh
```

## TODO's
- Test builds on Windows
- Solve known issues on Android:
  - Emulator doesn't work after resuming from background/device rotation
  - Make the emulator .INI file configurable (hard-coded now)

## Credits

- [https://blog.naver.com/ionique/10015262896](https://blog.naver.com/ionique/10015262896) Base emulator code
- [https://github.com/pvallet/hello-sdl2-android](https://github.com/pvallet/hello-sdl2-android) Android SDL2 template
- [https://github.com/mlabbe/nativefiledialog](https://github.com/mlabbe/nativefiledialog) Cross-platform file open/save dialog library
- [https://github.com/RustamG/file-dialogs](https://github.com/RustamG/file-dialogs) Android file open/save dialog library
- [http://www.old-computers.com/museum/computer.asp?st=1&c=803](http://www.old-computers.com/museum/computer.asp?st=1&c=803) Image used for the launcher icon
