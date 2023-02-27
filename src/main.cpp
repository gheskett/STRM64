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
 *	-o [output filenames]                (default: same as input, not including extension)
 *	-r [sample rate]                     (default: same as source file (affects playback speed))
 *	-R [resample rate]                   (default: same as source file (affects internal resolution))
 *	-l [enable/disable loop]             (default: either value in source audio or false)
 *	-s [loop start sample]               (default: either value in source audio or 0)
 *	-t [loop start timestamp]            (default: either value in source audio or 0)
 *	-e [loop end sample / total samples] (default: number of samples in source file)
 *	-f [loop end timestamp / total time] (default: length of source audio)
 *	-v [master volume of sequence]       (default: 127)
 *	-c [mute scale of sequence]          (default: 63)
 *	-m                                   (set all sequence channels to mono)
 *	-x                                   (don't generate stream files)
 *	-y                                   (don't generate sequence file)
 *	-z                                   (don't generate soundbank file)
 *	-h                                   (show help text)
 *
 * USAGE EXAMPLES
 *	STRM64 inputfile.wav -o outfiles -s 158462 -e 7485124
 *	STRM64 "spaces not recommended.wav" -l 1 -f 1:35.23
 *	STRM64 inputfile.brstm -l false -e 0x10000
 *	STRM64 custom_soundeffect.wav -y -z
 *
 * Note: STRM64 uses vgmstream to parse audio. You may need to install ffmpeg for certain conversions to be supported.
 * For the Windows build of this application, the bundled dlls are mandatory for this program to run.
 * You may need also to find additional dlls and add them to the folder (Windows) or install additional libraries such as FFmpeg to run the build (Linux).
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>

extern "C" {
#include "vgmstream.h"
}
		
#include "main.hpp"
#include "stream.hpp"
#include "sequence.hpp"
#include "soundbank.hpp"

using namespace std;

vector <string> cmdArgs;

string newFilename;
string parsedExeName;
bool customNewFilename = false;

bool generateStreams = true;
bool generateSequence = true;
bool generateSoundbank = true;
bool printedHelp = false;

static bool forcedMono = false;
static string duplicateStringName = "";

uint16_t gInstFlags = 0x0000;

VGMSTREAM *inFileProperties;

void printHelp() {
	if (printedHelp)
		return;
	printedHelp = true;

	string print = "\n"
		"Usage: " + parsedExeName + " <input audio file> [optional arguments]\n"
		"\n"
		"OPTIONAL ARGUMENTS\n"
		"    -o [output filenames]                (default: same as input, not including extension)\n"
		"    -r [sample rate]                     (default: same as source file (affects playback speed))\n"
		"    -R [resample rate]                   (default: same as source file (affects internal resolution))\n"
		"    -l [enable/disable loop]             (default: value in source audio or false)\n"
		"    -s [loop start sample]               (default: value in source audio or 0)\n"
		"    -t [loop start timestamp]            (default: value in source audio or 0)\n"
		"    -e [loop end sample / total samples] (default: number of samples in source file)\n"
		"    -f [loop end timestamp / total time] (default: length of source audio)\n"
		"    -v [master volume of sequence]       (default: 127)\n"
		"    -c [mute scale of sequence]          (default: 63)\n"
		"    -m                                   (set all sequence channels to mono)\n"
        "    -x                                   (don't generate stream files)\n"
        "    -y                                   (don't generate sequence file)\n"
        "    -z                                   (don't generate soundbank file)\n"
		"    -h                                   (show help text)\n"
		"\n"
		"USAGE EXAMPLES\n"
		"    " + parsedExeName + " inputfile.wav -o custom_outfiles -s 158462 -e 7485124\n"
		"    " + parsedExeName + " \"spaces not recommended.wav\" -l 1 -f 1:35.23\n"
		"    " + parsedExeName + " inputfile.brstm -l false -e 0x10000\n"
		"    " + parsedExeName + " custom_soundeffect.wav -y -z\n"
		"\n"
		"Note: " + parsedExeName + " uses vgmstream to parse audio. You may need to install ffmpeg for certain conversions to be supported.\n\n";

	printf("%s", print.c_str());
}

bool is_mono() {
	return forcedMono;
}

string get_filename_duplicate() {
	return duplicateStringName;
}

void set_filename_duplicate(string duplicate) {
	duplicateStringName = duplicate;
	size_t slash = duplicate.find_last_of("/\\");
	if (slash != string::npos) {
		duplicateStringName = duplicate.substr(slash+1);
	}
}

void print_param_warning(string param) {
	printf("WARNING: Invalid value used for %s parameter, skipping...\n", param.c_str());
}

int64_t parse_string_to_number(string input) {
	transform(input.begin(), input.end(), input.begin(), ::tolower);

	// Is this a hex number?
	if (input.length() > 2 && input[1] == 'x')
		return (int64_t) strtoll(input.substr(2).c_str(), NULL, 16);

	// Is this a boolean?
	if (input.compare("true") == 0)
		return 1;
	if (input.compare("false") == 0)
		return 0;

	// Nope, just a standard number or potentially invalid.
	return (int64_t) strtoll(input.c_str(), NULL, 10);
}

string strip_extension(string inStr) {
	size_t offsetPeriod = inStr.find_last_of(".");
	if (offsetPeriod == string::npos)
		return inStr;

	size_t offsetSlash = inStr.find_last_of("/\\");
	if (offsetSlash != string::npos && offsetSlash > offsetPeriod)
		return inStr;

	return inStr.substr(0, offsetPeriod);
}

string replace_spaces(string inStr) {
	size_t offsetSlash = inStr.find_last_of("/\\");
	if (offsetSlash == string::npos)
		offsetSlash = 0;

	for (; offsetSlash < inStr.length(); offsetSlash++) {
		if (inStr[offsetSlash] == ' ')
			inStr[offsetSlash] = '_';
	}

	return inStr;
}


int parse_input_arguments() {
	bool isPrintHelp = false;
	int32_t slash, colon;

	for (size_t i = 0; i < cmdArgs.size(); i++) {
		string arg = cmdArgs.at(i);
		if (arg.length() != 2 || arg[0] != '-')
			return RETURN_INVALID_ARGS;

		char argVal = (char) tolower(arg[1]);
		char argValNoCase = (char) arg[1];

		// Standalone arguments
		switch (argVal) {
		case 'm':
			forcedMono = true;
			continue;
		case 'x':
			generateStreams = false;
			continue;
		case 'y':
			generateSequence = false;
			continue;
		case 'z':
			generateSoundbank = false;
			continue;
		case 'h':
			isPrintHelp = true;
			continue;
		}

		i++;
		if (i == cmdArgs.size())
			return RETURN_INVALID_ARGS;

		arg = cmdArgs.at(i);

		// Value arguments
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
			} else {
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
			if (argValNoCase == 'R')
				set_resample_rate(parse_string_to_number(arg));
			else
				set_sample_rate(parse_string_to_number(arg));
			break;
		case 'l':
			set_enable_loop(parse_string_to_number(arg));
			break;
		case 's':
			set_loop_start_samples(parse_string_to_number(arg));
			break;
		case 't':
			set_loop_start_timestamp(arg);
			break;
		case 'e':
			set_loop_end_samples(parse_string_to_number(arg));
			break;
		case 'f':
			set_loop_end_timestamp(arg);
			break;
		case 'v':
			seq_set_master_volume(parse_string_to_number(arg));
			break;
		case 'c':
			seq_set_mute_scale(parse_string_to_number(arg));
			break;
		default:
			return RETURN_INVALID_ARGS;
		}
	}

	if (isPrintHelp)
		printHelp();

	return RETURN_SUCCESS;
}

int get_vgmstream_properties(const char *inFilename) {
	inFileProperties = init_vgmstream(inFilename);
	printf("Opening %s for reading...", inFilename);
	fflush(stdout);

	if (!inFileProperties) {
		FILE *invalidFile = fopen(inFilename, "r");
		if (invalidFile == NULL) {
			printf("...FAILED!\nERROR: Input file cannot be found or opened!\n");
			return RETURN_CANNOT_FIND_INPUT_FILE;
		}
		fclose(invalidFile);

		printf("...FAILED!\nERROR: Input file is not a valid audio file!\nIf you believe this is a fluke, please make sure you have the proper audio libraries installed.\n");
		printf("Alternatively, you can convert the input file to WAV separately and try again.\n");
		return RETURN_INVALID_INPUT_FILE;
	}

	if (inFileProperties->channels <= 0) {
		printf("...FAILED!\nERROR: Audio must have at least 1 channel!\nCONTAINS: %d channels\n", inFileProperties->channels);
		close_vgmstream(inFileProperties);
		return RETURN_NOT_ENOUGH_CHANNELS;
	}

	if (inFileProperties->channels > (int) NUM_CHANNELS_MAX) {
		printf("...FAILED!\nERROR: Audio file exceeds maximum of %d channels!\nCONTAINS: %d channels\n", (int) NUM_CHANNELS_MAX, inFileProperties->channels);
		close_vgmstream(inFileProperties);
		return RETURN_TOO_MANY_CHANNELS;
	}

	// Currently using bitflags to represent channels serves no purpose, but it may be easier to automate for decomp soundbank optimization in the future.
	for (int i = 0; i < inFileProperties->channels; i++)
		gInstFlags |= (1 << i);

	printf("...SUCCESS!\n");

	return RETURN_SUCCESS;
}


void print_seq_channels(uint16_t instFlags) {
	uint8_t numChannels = 0;

	for (uint8_t i = 0; i < (uint8_t) NUM_CHANNELS_MAX; i++)
		if (instFlags & (1 << i))
			numChannels++;

	printf("\n");

	printf("    Number of Channels: %d", numChannels);
	if (!forcedMono) {
		if (numChannels == 1)
			printf(" (mono)");
		else if (numChannels == 2)
			printf(" (stereo)");

	}
	printf("\n");

	printf("\n");
}

int main(int argc, char **argv) {
	if (argc == 0) {
		parsedExeName = "STRM64";
		printHelp();
		return RETURN_NOT_ENOUGH_ARGS;
	}

	parsedExeName = argv[0];
	size_t slash = parsedExeName.find_last_of("/\\");
	if (slash != string::npos)
		parsedExeName = parsedExeName.substr(slash+1);

	if (argc < 2) {
		printHelp();
		return RETURN_NOT_ENOUGH_ARGS;
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

	ret = get_vgmstream_properties(argv[1]);
	if (ret) {
		printHelp();
		return ret;
	}

	if (generateStreams)
		ret = generate_new_streams(inFileProperties, newFilename, argv[1]);
	else
		print_seq_channels(gInstFlags);

	if (generateSequence) {
		if (!ret)
			ret = generate_new_sequence(newFilename, gInstFlags);
		else
			generate_new_sequence(newFilename, gInstFlags);
	}

	if (generateSoundbank) {
		if (!ret)
			ret = generate_new_soundbank(newFilename, gInstFlags);
		else
			generate_new_soundbank(newFilename, gInstFlags);
	}

	close_vgmstream(inFileProperties);

	if (!generateStreams && !generateSequence && !generateSoundbank)
		printf("No files to generate!\n");

	return ret;
}
