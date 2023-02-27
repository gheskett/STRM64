# STRM64

## About

This is a command line tool designed to generate files required for emulation of streamed audio within Super Mario 64. STRM64 is compatible with any multichannel audio file up to 16 channels.

## How to Use

Usage: `STRM64 <input audio file> [optional arguments]`

OPTIONAL ARGUMENTS
```
-o [output filenames]                (default: same as input, not including extension)
-r [sample rate]                     (default: same as source file (affects playback speed))
-R [resample rate]                   (default: same as source file (affects internal resolution))
-l [enable/disable loop]             (default: either value in source audio or false)
-s [loop start sample]               (default: either value in source audio or 0)
-t [loop start timestamp]            (default: either value in source audio or 0)
-e [loop end sample / total samples] (default: number of samples in source file)
-f [loop end timestamp / total time] (default: length of source audio)
-v [master volume of sequence]       (default: 127)
-c [mute scale of sequence]          (default: 63)
-m                                   (set all sequence channels to mono)
-x                                   (don't generate stream files)
-y                                   (don't generate sequence file)
-z                                   (don't generate soundbank file)
-h                                   (show help text)
```

USAGE EXAMPLES
```
STRM64 inputfile.wav -o custom_outfiles -s 158462 -e 7485124
STRM64 "spaces not recommended.wav" -l 1 -f 1:35.23
STRM64 inputfile.brstm -l false -e 0x10000
STRM64 custom_soundeffect.wav -y -z
```

Note: STRM64 uses vgmstream to parse audio. You may need to install ffmpeg for certain conversions to be supported. For the Windows build of this application, the bundled dlls are mandatory for this program to run. You may need also to find additional dlls and add them to the folder (Windows) or install additional libraries such as FFmpeg to run the build (Linux).

## Building

### Windows

#### Windows Setup

- Install MSYS2 with MinGW (specifically 32 bit; this does not currently compile with 64-bit)

- Add `C:\msys64\mingw32\bin` to your Path (if also installing 64-bit shell, make sure mingw32 gets priority over mingw64)

- Open MSYS2 MinGW 32-bit and install the following libraries as such:
```
# update package information
pacman -Syu

# install essential MinGW 32-bit tools
pacman -Sy mingw-w64-i686-toolchain

# install cmake stuff
pacman -Sy mingw-w64-i686-cmake mingw-w64-i686-ninja
```

- Close out of the MinGW shell and open up the command prompt. Check your version of gcc with `gcc -v`
  - If gcc is referencing use with mingw32, you're good to go. If not, make sure your Path is given enough priority.

#### Windows Building

- Navigate to the root directory of STRM64 and run `cmake -S . -Bbuild` to set up build files

- Run `cmake --build build` to compile

- Output executable will be in the `build` folder, simply named STRM64.exe

- To clean the repo of all build/untracked files, run `git clean -dxf`

Note (almost) all of the dll files that appear in the build directory are mandatory for the Windows build to run with vgmstream. Please keep this in mind if moving the executable to a new location.

### Linux

- Start by installing the necessary libraries for use with vgmstream
```
# update package information to install up-to-date libraries
sudo apt update

# base deps
sudo apt install -y gcc g++ make build-essential git cmake

# vorbis deps
sudo apt install -y libvorbis-dev

# mpeg deps
sudo apt install -y libmpg123-dev

# speex deps
sudo apt install -y libspeex-dev

# ffmpeg deps
sudo apt install -y libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
```

- Navigate to the root directory of STRM64 and run `cmake -S . -Bbuild` to set up build files

- Run `cmake --build build` to compile

- Output executable will be in the `build` folder, simply named STRM64

- To clean the repo of all build/untracked files, run `git clean -dxf`

- NOTE: You may also need to install Ninja for use with cmake

If you are unable to make conversions that require libraries such as ffmpeg (e.g. mp3 files), you may need to upgrade to a newer Unix distro to install libraries that are up to date. If after doing this you are still unable to make conversions, it may be worth making a separate conversion to WAV using a separate software, and then trying again.
