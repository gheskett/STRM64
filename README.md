# STRM64

## About

This is a command line tool designed to generate files required for emulation of streamed audio within Super Mario 64. STRM64 is compatible with any multichannel audio file up to 16 channels.

Development on a new GUI for STRM64 has been started: [github.com/thecozies/STRM64-GUI](https://github.com/thecozies/STRM64-GUI)

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
-c [number of channels in sequence]  (default: same as source file)
-u [mute scale of sequence]          (default: 63)
-v [master volume of sequence]       (default: 127)
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
STRM64 inputfile.mp3 -R 32000 -t 0
STRM64 custom_soundeffect.wav -y -z
```

Note: STRM64 uses [vgmstream](https://github.com/vgmstream/vgmstream) to parse audio. You may need to install [ffmpeg](https://ffmpeg.org/) for certain conversions to be supported, or for the build to run at all. For the Windows build of this application, the bundled DLLs are mandatory for this program to run.

## Optional Argument Descriptions

- `-o [output filenames]`
  - This can be used to override the exported filenames of all of the files. File extension is determined automatically.
  - Example: Passing in any stereo audio file with `-o my_custom_audio` will produce the files `my_custom_audio_L.aiff`, `my_custom_audio_R.aiff`, `XX_my_custom_audio.m64`, and `XX_my_custom_audio.json`.
  - By default, this pulls from the name of the input audio file.
- `-r [sample rate]`
  - This can be used to change the sample rate of the exported audio, effectively altering its playback speed. Speed multiplier is calculated by `new sample rate / original sample rate`.
  - Any looping timestamp arguments passed in with this will be applied _before_ the speed change. Looping sample point arguments are unaffected.
  - Example: Passing in an audio file with a sample rate of 32000 Hz and then appending `-r 16000` will change the playback speed to 0.5x. In addition, passing in `-t 1:00` will automatically alter the starting loop point from 1 minute to 2 minutes.
  - Passing this argument along with resample rate will change the speed of the audio first before it gets resampled. When combining both arguments, all loop point computation will still be handled automatically.
- `-R [resample rate]`
  - This can be used to change the resample rate of the exported audio, effectively altering its resolution (and thus, file size). Unlike the sample rate argument, this does not impact playback speed.
  - Use of this command is highly encouraged if the sample rate of the source audio is greater than 32000 Hz, as this is the maximum audio fidelity produced by the game. Anything more is purely a waste of space.
  - Any looping sample point arguments passed in with this will be applied _before_ the speed change. Looping timestamp arguments are unaffected.
  - Example: Passing in an audio file with a sample rate of 48000 Hz and then appending `-R 32000` will change the resolution (and file size) to 66.7% that of the input. In addition, passing in `-s 48000` will automatically alter the starting loop point from 48000 samples to 32000 samples.
  - Passing this argument along with sample rate will process resample rate _after_ the speed change from sample rate. When combining both arguments, all loop point computation will still be handled automatically.
- `-l [enable/disable loop]`
  - Forcefully enables or disables looping.
  - Example: Passing in an audio file with no loop data followed with `-l true` will force the audio file to loop. If the audio file contained no loop information beforehand or no looping information is provided as arguments, the starting loop point will be set to the very beginning of the audio stream.
  - By default, this is autodetected by whether the input audio file contains any loop data. Most common audio files do not contain any loop data, and will not loop when passed into STRM64 standalone.
- `-s [loop start sample]`
  - Sets the starting loop point based on number of samples from the beginning of the audio file.
  - Passing this argument will forcefully enable looping, eliminating the need to use `-l`.
  - Example: Passing in an audio file with a sample rate of 32000 Hz and then appending `-s 80000` will set the starting loop point to exactly 2.5 seconds, or 80000 samples into the audio stream.
  - By default, this is determined automatically if the source audio file contains looping data. Otherwise, the default value is 0.
- `-t [loop start timestamp]`
  - Sets the starting loop point based on the timestamp passed in. Timestamp format is `dd:HH:mm:ss.SSSSSS`.
    - The timestamp format is not strict. There is no requirement to include days/microseconds or to use a two-digit input to represent seconds for example.
  - Passing this argument will forcefully enable looping, eliminating the need to use `-l`.
  - Example: Passing in an audio file followed by `-t 1:45` will set the starting loop point to exactly 1 minute and 45 seconds into the audio stream.
  - By default, this is determined automatically if the source audio file contains looping data. Otherwise, the default value is 0.
- `-e [loop end sample / total samples]`
  - Sets the ending loop point or end of stream based on number of samples from the beginning of the audio file.
  - If this value is larger than what the source audio contains, it will the full length of the source audio instead.
  - Example: Passing in an audio file with a sample rate of 32000 Hz and then appending `-e 320000` will either terminate or loop the audio after 10 seconds, or 320000 samples into the audio stream.
  - By default, this is determined automatically and uses the end loop point if it exists, otherwise it defaults to the very end of the audio stream.
- `-f [loop end timestamp / total time]`
  - Sets the ending loop point or end of stream based on the timestamp passed in. Timestamp format is `dd:HH:mm:ss.SSSSSS`.
    - The timestamp format is not strict. There is no requirement to include days/microseconds or to use a two-digit input to represent seconds for example.
  - If this value is larger than what the source audio contains, it will the full length of the source audio instead.
  - Example: Passing in an audio file followed by `-f 30.5` will either terminate or loop the audio after 30.5 seconds.
  - By default, this is determined automatically and uses the end loop point if it exists, otherwise it defaults to the very end of the audio stream.
- `-c [number of channels in sequence]`
  - Forcefully overrides the number of exported channels in the output m64 file to a specified value. Must be a number between 1 and 16 inclusive.
  - This is mostly useful for example if you want a multitracked audio stream but don't know how to obtain/produce a singular input audio file with all of the necessary channels. The soundbank will still need to be configured manually to accomodate for this.
  - By default, this uses the number of channels contained in the source audio file.
- `-u [mute scale of sequence]`
  - This can be used to set the mute scale of the sequence file. The mute scale affects the playback volume during stuff like cutscenes or when the game is paused.
  - It is not recommended to set this value above 127, but STRM64 will not prevent you from doing so.
  - By default, this is set to 63, or about half the maximum master volume.
- `-v [master volume of sequence]`
  - This can be used to set the master volume of the sequence file.
  - It is not recommended to set this value above 127, but STRM64 will not prevent you from doing so.
  - By default, this is set to 127, which is the maximum value with defined behavior.
- `-m`
  - Sets all sequence channels to mono. Useful for multitracked audio consisting of mono channels rather than stereo.
- `-x`
  - Skips generation of .aiff audio files.
- `-y`
  - Skips generation of .m64 sequence file.
- `-z`
  - Skips generation of .json soundbank file.
- `-h`
  - Forcefully displays help text. This can also be accomplished by running STRM64 with no or invalid arguments.

## Importing Generated Files Into the Game

This process is explained on the [STRM64 Wiki](https://github.com/gheskett/STRM64/wiki). Please read through those resources first if you need help importing your music into the game. Note these guides will not cover how to use streamed audio with binary hacks.

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

Note (almost) all of the DLL files that appear in the build directory are mandatory for the Windows build to run with vgmstream. Please keep this in mind if moving the executable to a new location.

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
