#include <stdio.h>
#include <stdlib.h>

#include "main.hpp"

using namespace std;

string generate_bank_start() {
	return
		"{\n"
		"    \"date\": \"1996-03-19\",\n"
		"    \"sample_bank\": \"streamed_audio\",\n" // TODO: Custom folder names
		"    \"envelopes\": {\n"
		"        \"envelope0\": [\n"
		"            [1, 32700],\n"
		"            \"hang\"\n"
		"        ]\n"
		"    },\n"
		"    \"instruments\": {\n";
}

string generate_instrument_strings(string bankStr, string filename, uint16_t instFlags, uint8_t numChannels) {
	string instruments = "";
	string instList = "    \"instrument_list\": [\n";

	for (uint8_t i = 0, j = 0; j < numChannels; i++) {
		if (!((1 << i) & instFlags)) {
			instList += "        null";
			if (j != numChannels)
				instList += ",";
			instList += "\n";
			continue;
		}

		string tmp = "        \"inst";
		tmp += to_string(i);
		tmp += "\"";

		instruments += tmp;
		instList += tmp;

		instruments += ": {\n"
			"            \"release_rate\": 10,\n"
			"            \"envelope\": \"envelope0\",\n"
			"            \"sound\": \"";

		string newFilename = filename;
		
		if (numChannels == 2 && !is_mono()) {
			if (j == 0) {
				newFilename += "_L";
			} else {
				newFilename += "_R";
			}
		} else if (numChannels != 1) {
			newFilename += '_';

			char index = (j & 0x0F) + 48;
			if (index >= 58)
				index += 7;
			newFilename += index;
		}

		if (newFilename.compare(get_filename_duplicate()) == 0) {
			newFilename += "_0";
		}

		instruments += newFilename + "\"\n"
			"        }";

		j++;

		if (j != numChannels) {
			instruments += ",";
			instList += ",";
		}

		instruments += "\n";
		instList += "\n";
	}

	instruments += "    },\n";
	instList += "    ]\n"
		"}\n";

	return instruments + instList;
}

int write_to_soundbank(string filename, uint16_t instFlags, uint8_t numChannels) {
	FILE *seqBank;

	printf("Generating soundbank file...");
	fflush(stdout);

	string shortFilename;
	string tmpFilename = filename;
	size_t slash = tmpFilename.find_last_of("/\\");
	if (slash == string::npos) {
		shortFilename = tmpFilename;
		tmpFilename = "XX_" + shortFilename + ".json";
	}
	else {
		shortFilename = tmpFilename.substr(slash+1);
		tmpFilename = tmpFilename.substr(0, slash+1) + "XX_" + shortFilename + ".json";
	}

	seqBank = fopen(tmpFilename.c_str(), "wb");
	if (seqBank == NULL) {
		printf("...FAILED!\nERROR: Could not open %s for writing!\n", filename.c_str());
		return RETURN_SOUNDBANK_CANNOT_CREATE_FILE;
	}

	string bankStr = generate_bank_start();
	bankStr += generate_instrument_strings(bankStr, shortFilename, instFlags, numChannels);

	fwrite(bankStr.c_str(), 1, bankStr.length(), seqBank); // Not using fprintf here to avoid carriage returns on Windows

	fclose(seqBank);

	printf("...DONE!\n");

	return RETURN_SUCCESS;
}

int generate_new_soundbank(string filename, uint16_t instFlags) {
	uint8_t numChannels = 0;

	for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) {
		if (!((1 << i) & instFlags))
			continue;

		numChannels++;
	}
	if (numChannels == 0)
		return RETURN_SOUNDBANK_NO_CHANNELS;

	return write_to_soundbank(filename, instFlags, numChannels);
}
