# STRM64

## About

This is a command line tool designed to generate files required for emulation of streamed audio within Super Mario 64. STRM64 is compatible with any multichannel audio file up to 16 channels.

## How to Use

Usage: `STRM64 <input audio file> [optional arguments]`

OPTIONAL ARGUMENTS
```
-o [output filenames]                      (default: same as input, not including extension)
-r [sample rate]                           (default: same as source file (this does NOT resample the audio!))
-l [enable/disable loop]                   (default: either value in source audio or false)
-s [loop start sample]                     (default: either value in source audio or 0)
-t [loop start in microseconds]            (default: either value in source audio or 0)
-e [loop end sample / total samples]       (default: number of samples in source file)
-f [loop end in microseconds / total time] (default: length of source audio)
-v [master volume of sequence]             (default: 127)
-c [mute scale of sequence]                (default: 63)
-m                                         (set all sequence channels to mono)
-x                                         (don't generate stream files)
-y                                         (don't generate sequence file)
-z                                         (don't generate soundbank file)
-h                                         (show help text)
```

USAGE EXAMPLES
```
STRM64 inputfile.wav -o outfiles -s 158462 -e 7485124
STRM64 "spaces not recommended.wav" -l 1 -f 95000000
STRM64 inputfile.brstm -l false -e 0x10000
```

Note: STRM64 uses vgmstream to parse audio. You may need to install additional libraries for certain conversions to be supported.

## Building

### Windows

`TODO: get Windows vgmstream library to work`

### Linux

- Start by installing the necessary libraries for use with vgmstream
```
# base deps
sudo apt-get install -y gcc g++ make build-essential git

# vorbis deps
sudo apt-get install -y libvorbis-dev

# mpeg deps
sudo apt-get install -y libmpg123-dev

# speex deps
sudo apt-get install -y libspeex-dev

# ffmpeg deps
sudo apt-get install -y libavformat-dev libavcodec-dev libavutil-dev libswresample-dev
```

- Run `cmake -S . -B build` to set up the build files

- Run `cmake --build build` to compile

- Output executable will be in the `build` folder, simply named STRM64

- To clean the repo of all build/untracked files, run `git clean -dxf`
