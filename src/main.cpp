/**
 * Written by gheskett (ArcticJaguar725)
 *
 * This is a command line tool designed to generate files required for emulation of streamed audio within Super Mario 64.
 * STRM64 is compatible with any multichannel audio file up to 16 channels.
 *
 */

 /**
 * Usage: STRM64 <input audio file> [optional arguments]
 *
 * OPTIONAL ARGUMENTS
 *	-o [output filenames]                      (default: same as input, not including extension)
 *	-c                                         (force vgmstream conversion)
 *	-r [sample rate]                           (default: same as source file (this does NOT resample the audio!))
 *	-l [enable/disable loop]                   (default: either value in source audio or false)
 *	-s [loop start sample]                     (default: either value in source audio or 0)
 *	-t [loop start in microseconds]            (default: either value in source audio or 0)
 *	-e [loop end sample / total samples]       (default: number of samples in source file)
 *	-f [loop end in microseconds / total time] (default: length of source audio)
 *	-v [master volume of sequence]             (default: 127)
 *	-m [mute scale of sequence]                (default: 63)
 *	-x                                         (don't generate sequence file)
 *	-y                                         (don't generate soundbank file)
 *	-h                                         (show help text)
 *
 * USAGE EXAMPLES
 *	STRM64 inputfile.wav -o outfiles -s 158462 -e 7485124
 *	STRM64 "spaces not recommended.wav" -l true -f 95000000
 *  STRM64 inputfile.brstm -l false -e 0x10000
 *
 * Note: This program works with WAV files (.wav) encoded with 16-bit PCM. If the source file is anything other than a WAV file, STRM64 will attempt to make a separate conversion with the vgmstream library, if it can.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

#include "main.hpp"
#include "stream.hpp"
#include "sequence.hpp"
#include "soundbank.hpp"
#include "bswp.hpp"

using namespace std;

vector <string> cmdArgs;

string newFilename;
bool customNewFilename = false;

bool generateSequence = true;
bool generateSoundbank = true;

uint16_t gInstFlags = 0x0000;

void printHelp() {
	string print = "\n"
		"Usage: STRM64 <input audio file> [optional arguments]\n"
		"\n"
		"OPTIONAL ARGUMENTS\n"
		"    -o [output file]                           (default: same as input, not including extension)\n"
		"    -c                                         (force vgmstream conversion)\n"
		"    -r [sample rate]                           (default: same as source file (this does NOT resample the audio!))\n"
		"    -l [enable/disable loop]                   (default: value in source audio or false)\n"
		"    -s [loop start sample]                     (default: value in source audio or 0)\n"
		"    -t [loop start in microseconds]            (default: value in source audio or 0)\n"
		"    -e [loop end sample / total samples]       (default: number of samples in source file)\n"
		"    -f [loop end in microseconds / total time] (default: length of source audio)\n"
		"    -v [master volume of sequence]             (default: 127)\n"
		"    -m [mute scale of sequence]                (default: 63)\n"
        "    -x                                         (don't generate sequence file)\n"
        "    -y                                         (don't generate soundbank file)\n"
		"    -h                                         (show help text)\n"
		"\n"
		"USAGE EXAMPLES\n"
		"    STRM64 inputfile.wav -o outfiles -s 158462 -e 7485124\n"
		"    STRM64 \"spaces not recommended.wav\" -l 1 -f 95000000\n"
		"    STRM64 inputfile.brstm -l false -e 0x10000\n"
		"\n"
		"Note: This program works with WAV files (.wav) encoded with 16-bit PCM. If the source file is anything other than a WAV file, STRM64 will attempt to make a separate conversion with the vgmstream library, if it can.\n";
}

void print_param_warning(string param) {
	printf("WARNING: Invalid value used for %s parameter, skipping...\n", param.c_str());
}

int64_t parse_string_to_number(string input) {
	transform(input.begin(), input.end(), input.begin(), ::tolower);

	// Is this a hex number?
	if (input.length() > 2) {
		char isHexNumber = input[1];
		if (isHexNumber == 'x') {
			return (int64_t) strtoll(input.substr(2).c_str(), NULL, 16);
		}
	}

	// Is this a boolean?
	if (input.compare("true"))
		return 1;
	if (input.compare("false"))
		return 0;

	// Nope, just a standard number or potentially invalid.
	return (int64_t) strtoll(input.c_str(), NULL, 10);
}

string strip_extension(string inStr) {
	size_t offsetPeriod = inStr.find_last_of(".");
	if (offsetPeriod == string::npos)
		return inStr;

	size_t offsetSlash = inStr.find_last_of("/\\");
	if (offsetSlash != string::npos) {
		if (offsetSlash > offsetPeriod)
			return inStr;
	}

	return inStr.substr(0, offsetPeriod);
}

string replace_spaces(string inStr) {
	for (size_t i = 0; i < inStr.length(); i++) {
		if (inStr[i] == ' ')
			inStr[i] = '_';
	}

	return inStr;
}


int parse_input_arguments() {
	bool isPrintHelp = false;
	int32_t slash, colon;

	for (size_t i = 0; i < cmdArgs.size(); i++) {
		string arg = cmdArgs.at(i);
		if (arg.length() != 2 || arg[0] != '-')
			return 2;

		char argVal = (char) tolower(arg[1]);

		// Single value arguments
		switch (argVal) {
		case 'c':
			set_force_vgmstream();
			continue;
		case 'x':
			generateSequence = false;
			continue;
		case 'y':
			generateSoundbank = false;
			continue;
		case 'h':
			isPrintHelp = true;
			continue;
		}

		i++;
		if (i == cmdArgs.size())
			return 2;

		arg = cmdArgs.at(i);

		// Multi value arguments
		switch (argVal) {
		case 'o':
			slash = (int32_t) arg.find_last_of("/\\");
			if (arg.find_last_of("/\\") == string::npos)
				slash = -1;
			colon = (int32_t) arg.find_last_of(":");
			if (arg.find_last_of(":") == string::npos)
				colon = -1;

			if (arg.find("*") != string::npos || arg.find("?") != string::npos || arg.find("\"") != string::npos || colon > slash
				|| arg.find("<") != string::npos || arg.find(">") != string::npos || arg.find("|") != string::npos) {
				printf("WARNING: Output filename \"%s\" contains illegal format/characters. Output argument will be ignored.\n", arg.c_str());
			}
			else {
				if (arg.find_last_of("/\\") + 1 == arg.length()) {
					if (newFilename.find_last_of("/\\") != string::npos)
						arg += newFilename.substr(newFilename.find_last_of("/\\") + 1, newFilename.length());
					else
						arg += newFilename;
				}
				newFilename = arg;
			}

			customNewFilename = true;
			break;
		case 'r':
			set_sample_rate(parse_string_to_number(arg));
			break;
		case 'l':
			set_enable_loop(parse_string_to_number(arg));
			break;
		case 's':
			set_loop_start_samples(parse_string_to_number(arg));
			break;
		case 't':
			set_loop_start_microseconds(parse_string_to_number(arg));
			break;
		case 'e':
			set_loop_end_samples(parse_string_to_number(arg));
			break;
		case 'f':
			set_loop_end_microseconds(parse_string_to_number(arg));
			break;
		case 'v':
			seq_set_master_volume(parse_string_to_number(arg));
			break;
		case 'm':
			seq_set_mute_scale(parse_string_to_number(arg));
			break;
		default:
			return 2;
		}
	}

	if (isPrintHelp)
		printHelp();

	return 0;
}

int main(int argc, char **argv) {
	if (argc == 0)
		return 1;

	if (argc < 2) {
		printHelp();
		return 1;
	}

	newFilename = argv[1];

	for (int i = 2; i < argc; i++)
		cmdArgs.emplace_back(argv[i]);

	int ret = parse_input_arguments();
	if (ret) {
		printHelp();
		return ret;
	}

	newFilename = replace_spaces(newFilename);

	if (!customNewFilename)
		newFilename = strip_extension(newFilename);

	// TODO: stream.cpp time!
	// TODO: set ret variable for generation calls; don't proceed if it's not set to 0

	if (generateSequence && gInstFlags)
		generate_new_sequence(newFilename, gInstFlags);

	if (generateSoundbank && gInstFlags)
		generate_new_soundbank(newFilename, gInstFlags);

	return 0;
}
