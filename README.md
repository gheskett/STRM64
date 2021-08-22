# STRM64

## About

This is a command line tool designed to generate files required for emulation of streamed audio with Super Mario 64. STRM64 is compatible with any multichannel audio file up to 16 channels.

## How to Use

Usage: `STRM64 <input audio file> [optional arguments]`

OPTIONAL ARGUMENTS
```
	-o [output filenames]                      (default: same as input, not including extension)
	-c                                         (force vgmstream conversion)
	-r [sample rate]                           (default: same as source file (this does NOT resample the audio!))
	-l [enable/disable loop]                   (default: either value in source audio or false)
	-s [loop start sample]                     (default: either value in source audio or 0)
	-t [loop start in microseconds]            (default: either value in source audio or 0)
	-e [loop end sample / total samples]       (default: number of samples in source file)
	-f [loop end in microseconds / total time] (default: length of source audio)
	-v [master volume of sequence]             (default: 127)
	-m [mute scale of sequence]                (default: 63)
	-x                                         (don't generate sequence file)
	-y                                         (don't generate soundbank file)
	-h                                         (show help text)
```

USAGE EXAMPLES
```
STRM64 inputfile.wav -o outfiles -s 158462 -e 7485124
STRM64 "spaces not recommended.wav" -l true -f 95000000
```

Note: This program works with WAV files (.wav) encoded with 16-bit PCM. If the source file is anything other than a WAV file, STRM64 will attempt to make a separate conversion with the vgmstream library, if it can.
