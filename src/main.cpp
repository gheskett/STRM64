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
 *	-r [sample rate]                           (default: same as source file (this does NOT resample the audio!))
 *	-l [enable/disable loop]                   (default: either value in source audio or false)
 *	-s [loop start sample]                     (default: either value in source audio or 0)
 *	-t [loop start in microseconds]            (default: either value in source audio or 0)
 *	-e [loop end sample / total samples]       (default: number of samples in source file)
 *	-f [loop end in microseconds / total time] (default: length of source audio)
 *	-v [master volume of sequence]             (default: 127)
 *	-c [mute scale of sequence]                (default: 63)
 *	-m                                         (set all sequence channels to mono)
 *	-x                                         (don't generate stream files)
 *	-y                                         (don't generate sequence file)
 *	-z                                         (don't generate soundbank file)
 *	-h                                         (show help text)
 *
 * USAGE EXAMPLES
 *	STRM64 inputfile.wav -o outfiles -s 158462 -e 7485124
 *	STRM64 "spaces not recommended.wav" -l 1 -f 95000000
 *  STRM64 inputfile.brstm -l false -e 0x10000
 *
 * Note: STRM64 uses vgmstream to parse audio. You may need to install additional libraries for certain conversions to be supported.
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
bool customNewFilename = false;
bool forcedMono = false;

bool generateStreams = true;
bool generateSequence = true;
bool generateSoundbank = true;

uint16_t gInstFlags = 0x0000;

VGMSTREAM *inFileProperties;
VGMSTREAM *inFilePropertiess;

void printHelp() {
	string print = "\n"
		"Usage: STRM64 <input audio file> [optional arguments]\n"
		"\n"
		"OPTIONAL ARGUMENTS\n"
		"    -o [output file]                           (default: same as input, not including extension)\n"
		"    -r [sample rate]                           (default: same as source file (this does NOT resample the audio!))\n"
		"    -l [enable/disable loop]                   (default: value in source audio or false)\n"
		"    -s [loop start sample]                     (default: value in source audio or 0)\n"
		"    -t [loop start in microseconds]            (default: value in source audio or 0)\n"
		"    -e [loop end sample / total samples]       (default: number of samples in source file)\n"
		"    -f [loop end in microseconds / total time] (default: length of source audio)\n"
		"    -v [master volume of sequence]             (default: 127)\n"
		"    -c [mute scale of sequence]                (default: 63)\n"
		"    -m                                         (set all sequence channels to mono)\n"
        "    -x                                         (don't generate stream files)\n"
        "    -y                                         (don't generate sequence file)\n"
        "    -z                                         (don't generate soundbank file)\n"
		"    -h                                         (show help text)\n"
		"\n"
		"USAGE EXAMPLES\n"
		"    STRM64 inputfile.wav -o outfiles -s 158462 -e 7485124\n"
		"    STRM64 \"spaces not recommended.wav\" -l 1 -f 95000000\n"
		"    STRM64 inputfile.brstm -l false -e 0x10000\n"
		"\n"
		"Note: STRM64 uses vgmstream to parse audio. You may need to install additional libraries for certain conversions to be supported.\n";

	printf("%s", print.c_str());
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
		case 'm':
			seq_set_mono();
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
		case 'c':
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

int get_vgmstream_properties(const char *inFilename) {
	inFileProperties = init_vgmstream(inFilename);
	printf("Opening %s for reading...", inFilename);

	if (!inFileProperties) {
		FILE *invalidFile = fopen(inFilename, "r");
		if (invalidFile == NULL) {
			printf("...FAILED!\nERROR: Input file cannot be found or opened!\n");
			return 3;
		}
		fclose(invalidFile);

		printf("...FAILED!\nERROR: Input file is not a valid audio file!\nIf you believe this is a fluke, please make sure you have the proper audio libraries installed.\n");
		printf("Alternatively, you can convert the input file to WAV separately and try again.\n");
		return 4;
	}

	if (inFileProperties->channels <= 0) {
		printf("...FAILED!\nERROR: Audio cannot have less than 1 audio channel!\nCONTAINS: %d channels\n", inFileProperties->channels);
		close_vgmstream(inFileProperties);
		return 5;
	}

	if (inFileProperties->channels > VGMSTREAM_MAX_CHANNELS) {
		printf("...FAILED!\nERROR: Audio file exceeds maximum of %d channels!\nCONTAINS: %d channels\n", (int) NUM_CHANNELS_MAX, inFileProperties->channels);
		close_vgmstream(inFileProperties);
		return 6;
	}

	// Currently using bitflags to represent channels serves no purpose, but it may be easier to automate for decomp soundbank optimization in the future.
	for (int i = 0; i < inFileProperties->channels; i++)
		gInstFlags |= (1 << i);

	printf("...SUCCESS!\n");

	return 0;
}

string samples_to_us_print(uint64_t sample_offset) {
	uint64_t convTime = (uint64_t) (((long double) sample_offset / (long double) inFileProperties->sample_rate) * 1000000.0 + 0.5);

	char buf[64];
	if (convTime >= 3600000000) {
		sprintf(buf, "%d:%02d:%02d.%06d",
			(int) (convTime / 3600000000),
			(int) (convTime / 60000000) % 60,
			(int) (convTime / 1000000) % 60,
			(int) (convTime % 1000000));
	}
	else if (convTime >= 60000000) {
		sprintf(buf, "%d:%02d.%06d",
			(int) (convTime / 60000000),
			(int) (convTime / 1000000) % 60,
			(int) (convTime % 1000000));
	}
	else {
		sprintf(buf, "%d.%06d",
			(int) (convTime / 1000000),
			(int) (convTime % 1000000));
	}

	string ret = buf;
	return ret;
}

void print_header_info(bool isStreamGeneration, uint32_t fileSize) {
	printf("\n");

	if (isStreamGeneration) {
		printf("    Output Audio File Size(s): %u bytes\n", fileSize);

		printf("    Sample rate: %d Hz", inFileProperties->sample_rate);
		if (inFileProperties->sample_rate > 32000)
			printf(" (Downsampling recommended!)");
		printf("\n");

		printf("    Is Looped: ");
		if (inFileProperties->loop_flag) {
			printf("true\n");

			printf("    Starting Loop Point: %d Samples (Time: %s)\n", inFileProperties->loop_start_sample,
				samples_to_us_print(inFileProperties->loop_start_sample).c_str());
		}
		else {
			printf("false\n");
		}

		printf("    End of Stream: %d Samples (Time: %s)\n", inFileProperties->num_samples,
			samples_to_us_print(inFileProperties->num_samples).c_str());
	}

	printf("    Number of Channels: %d", inFileProperties->channels);
	if (!forcedMono) {
		if (inFileProperties->channels == 1)
			printf(" (mono)");
		else if (inFileProperties->channels == 2)
			printf(" (stereo)");

	}
	printf("\n");

	printf("\n");
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

	ret = get_vgmstream_properties(argv[1]);
	if (ret) {
		printHelp();
		return ret;
	}

	if (generateStreams && !ret) {
		ret = generate_new_streams(inFileProperties, newFilename);
	}
	else if (!ret) {
		print_header_info(false, 0);
	}

	if (generateSequence && !ret)
		ret = generate_new_sequence(newFilename, gInstFlags);

	if (generateSoundbank && !ret)
		ret = generate_new_soundbank(newFilename, gInstFlags);

	close_vgmstream(inFileProperties);

	return ret;
}
