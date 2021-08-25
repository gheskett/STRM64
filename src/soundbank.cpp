#include <stdio.h>
#include <stdlib.h>

#include "main.hpp"

using namespace std;

string generate_bank_start() {
	return
		"{\n"
		"    \"date\": \"1996-03-19\",\n"
		"    \"sample_bank\": \"streamed_audio\",\n"
		"    \"envelopes\": {\n"
		"        \"envelope0\": [\n"
		"            [1, 32700],\n"
		"            [1, 32700],\n"
		"            [32700, 29430],\n"
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
			"            \"sound\": \"" + filename + "_";

		char index = (j & 0x0F) + 48;
		if (index >= 58)
			index += 7;

		instruments += index;
		instruments += "\"\n"
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

	string tmpFilename = filename;
	size_t slash = tmpFilename.find_last_of("/\\");
	if (slash == string::npos)
		tmpFilename = "XX_" + tmpFilename + ".json";
	else
		tmpFilename = tmpFilename.substr(0, slash+1) + "XX_" + tmpFilename.substr(slash+1) + ".json";

	seqBank = fopen(tmpFilename.c_str(), "wb");
	if (seqBank == NULL) {
		printf("...FAILED!\nERROR: Could not open %s for writing!\n", filename.c_str());
		return 2;
	}

	string bankStr = generate_bank_start();
	bankStr += generate_instrument_strings(bankStr, filename, instFlags, numChannels);

	fwrite(bankStr.c_str(), 1, bankStr.length(), seqBank); // Not using fprintf here to avoid carriage returns on Windows

	fclose(seqBank);

	printf("...DONE!\n");

	return 0;
}

int generate_new_soundbank(string filename, uint16_t instFlags) {
	uint8_t numChannels = 0;

	for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) {
		if (!((1 << i) & instFlags))
			continue;

		numChannels++;
	}
	if (numChannels == 0)
		return 1;

	return write_to_soundbank(filename, instFlags, numChannels);
}
