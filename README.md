# SPC-1000

## Introduction

This is an emulator for SPC-1000 ([https://en.wikipedia.org/wiki/SPC-1000](https://en.wikipedia.org/wiki/SPC-1000)) ported on Android device. It was branched off of Windows-based one developed and published by Ionique at ([https://blog.naver.com/ionique/10015262896](https://blog.naver.com/ionique/10015262896)).

## Requirements
- JDK and JRE 8
- Android SDK and NDK (with Android Build-tools 28.0.3 and Android Platform API 28, though these are configurable)
- ANDROID_HOME and ANDROID_NDK_HOME environment variables set (I did this in /etc/environment)
- Android device (> SDK version 19, Lollipop)

## How to build (command line)

You can download the dependencies (SDL2), compile your program and then install it on a connected device with the following commands:
```
./get_dependencies
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
## Credits

- [https://blog.naver.com/ionique/10015262896](https://blog.naver.com/ionique/10015262896) Base emulator code. All the files are redistributed as-is, except some TXT files that were converted from EUC-KR to UTF-8 encoding
- [https://github.com/pvallet/hello-sdl2-android](https://github.com/pvallet/hello-sdl2-android) Android SDL2 template
