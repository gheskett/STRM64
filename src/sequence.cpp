#include <stdio.h>
#include <stdlib.h>

#include "main.hpp"
#include "sequence.hpp"
#include "stream.hpp"

using namespace std;


#define TIMESTAMP_DELAY 6 // NOTE: Cannot be less than 1
#define MAX_DURATION (0x7FFF - TIMESTAMP_DELAY) // NOTE: Must be a bit less than max int64_t value, as additional timestamps are tacked on to the end of this.

#define MUTE_SCALE_DEFAULT 0x3F
#define MASTER_VOLUME_DEFAULT 0x7F

// These must be changed when manually adding/removing fields
#define SEQ_HEADER_SIZE 0x17 // Exclusive of looping branch and Channel Pointer commands
#define CHN_HEADER_SIZE 0x13
#define TRK_HEADER_SIZE 0x0A
#define ABS_PTR_SIZE 0x03


static uint8_t gNumChannels = 0;
static int8_t gMuteScale = MUTE_SCALE_DEFAULT;
static uint8_t gMasterVolume = MASTER_VOLUME_DEFAULT;
static uint8_t gTempo = 0;
static int16_t gTimestamp = -1;

static string warnings = "";


SEQHeader::SEQHeader(uint16_t instFlags, uint8_t numChannels) {
	channelFlags = instFlags;
	channelCount = numChannels;
	muteScale = gMuteScale;
	volume = gMasterVolume;
}
SEQHeader::~SEQHeader() {

}

CHNHeader::CHNHeader(uint8_t channelIndex, uint8_t instId, uint8_t numChannels) {
	channelId = channelIndex;
	instrument = instId;
	
	// TODO: make channel panning overrideable
	if (is_mono()) {
		pan = 0x3F;
	} else {
		if (channelIndex % 2) { // right channel
			pan = 0x7F;
		} else { // left/mono channel
			if (channelIndex + 1 == numChannels) // mono channel
				pan = 0x3F;
			else // left channel
				pan = 0x00;
		}
	}
}
CHNHeader::~CHNHeader() {

}

SEQFile::SEQFile(string fname, uint16_t instFlags, uint8_t numChannels) {
	filename = fname;
	channelCount = numChannels;
	channelFlags = instFlags;

	chnHeader = new CHNHeader*[numChannels];
	for (uint8_t i = 0, j = 0; j < numChannels; i++) {
		if (!((1 << i) & instFlags))
			continue;

		chnHeader[j] = new CHNHeader(j, i, numChannels);
		j++;
	}

	seqhead = new SEQHeader(instFlags, numChannels);
}
SEQFile::~SEQFile() {
	delete seqhead;
	for (size_t i = 0; i < channelCount; i++)
		delete chnHeader[i];
	delete[] chnHeader;
}

string seq_get_duration_print() {
	if (gTimestamp < 0)
		return "";

	if (gTempo == 0)
		gTempo = 1;

	// Timestamp * 60 seconds / (BPM * tatums per beat), result in microseconds
	return print_timestamp(1000000 * (uint64_t) gTimestamp * 60 / (48 * (uint64_t) gTempo));
}

void seq_set_timestamp_duration(long double duration120BPM) {
	gTempo = 120;
	if (ceil(duration120BPM) <= MAX_DURATION) {
		// No tempo change needed from 120 BPM, grab value ceiling and return
		gTimestamp = (int16_t) ceil(duration120BPM);
		return;
	}

	gTempo = (120 * MAX_DURATION) / duration120BPM;

	if (gTempo < 1) {
		// Too long, just play sequence file forever (tbf the music needs to be over 11 hours long for this to happen)
		gTempo = 0;
		gTimestamp = -1;
		return;
	}

	int64_t newDuration = ceil(duration120BPM * gTempo / 120.0);
	if (newDuration > MAX_DURATION) {
		printf("FATAL WARNING: Miscalculation in seq_set_timestamp_duration function!\n");
		newDuration = MAX_DURATION;
	}

	gTimestamp = newDuration;
}

uint8_t seq_get_num_channels() {
	return gNumChannels;
}

bool seq_set_num_channels(int64_t numChannels) {
	if (numChannels <= 0 || numChannels > (int64_t) NUM_CHANNELS_MAX) {
		print_param_warning("sequence channel count");
		return false;
	}

	gNumChannels = (uint8_t) numChannels;

	return true;
}

void seq_set_mute_scale(int64_t muteScale) {
	if (muteScale < -128 || muteScale > 255) {
		print_param_warning("sequence mute scale");
		return;
	}
	if (muteScale > 127)
		muteScale -= 256;

	gMuteScale = (int8_t) muteScale;
}

void seq_set_master_volume(int64_t volume) {
	if (volume < 0 || volume > 255) {
		print_param_warning("sequence master volume");
		return;
	}
	if (volume > 127) {
		printf("WARNING: It is not recommended to set a sequence channel volume greater than 127!\n");
	}

	gMasterVolume = (uint8_t) volume;
}

void SEQHeader::write_seq_header(FILE *seqFile, uint16_t seqHeaderSize) {
	uint8_t *header = new uint8_t[seqHeaderSize]; // Data buffer for temporary storage before printing
	size_t headerPtr = 0; // Initialize data pointer to 0

	// Calculate channels being used with sequence. This intentionally generates channels from MAX - n to MAX, rather than from 0 to n.
	// I still haven't established whether this actually matters, but it doesn't really hurt either, so I'm keeping it this way for now.
	uint16_t enabledChannels = 0;
	for (uint8_t i = 0; i < this->channelCount; i++)
		enabledChannels |= (1 << i);

	// How the sequence behaves when pausing the game (0x20 softens music on pause)
	header[headerPtr++] = SEQ_MUTE_BEHAVIOR;
	header[headerPtr++] = 0x20;

	// How much to soften music if above is set to 0x20
	header[headerPtr++] = SEQ_MUTE_SCALE;
	header[headerPtr++] = (uint8_t) this->muteScale;

	// Enable only channels being used with streamed sequence
	header[headerPtr++] = SEQ_CHANNEL_ENABLE;
	header[headerPtr++] = (uint8_t) (enabledChannels >> 8);
	header[headerPtr++] = (uint8_t) enabledChannels;

	// Volume of overall sequence
	header[headerPtr++] = SEQ_VOLUME;
	header[headerPtr++] = this->volume;

	// Absolute channel pointers
	uint16_t ptrOffset = seqHeaderSize;
	for (uint8_t i = 0; i < this->channelCount; i++) {
		header[headerPtr++] = (uint8_t) (SEQ_CHANNEL_POINTER + i);
		header[headerPtr++] = (uint8_t) (ptrOffset >> 8);
		header[headerPtr++] = (uint8_t) ptrOffset;

		ptrOffset += CHN_HEADER_SIZE;
	}

	/**
	 * Tempo needs to be set and reset for a few reasons.
	 *
	 * Firstly, if this gets queued and then replayed (e.g. metal cap gets picked up), the music will start again desynced.
	 * Adding a start delay to the seuqnece fixes this, but the tempo needs to be nonzero for this to work.
	 *
	 * Secondly, if the intent is to play dynamic sequences that start with some channels silenced, they will still play at sequence start briefly before silencing.
	 * Naturally, adding a delay here helps mitigate that effect.
	 *
	 * The chosen values are somewhat arbitrary, but they seem to work well enough without causing noticeable audio latency in the process.
	 */

	// Set tempo to 0x30 (arbitrary value, but it works well enough)
	header[headerPtr++] = SEQ_TEMPO;
	header[headerPtr++] = 0x30;

	// Wait for TIMESTAMP_DELAY audio ticks to pass
	header[headerPtr++] = SEQ_TIMESTAMP; // NOTE: This does not need to be 3 bytes, but is less likely to break if TIMESTAMP_DELAY is set to a huge value.
	header[headerPtr++] = (uint8_t) ((uint16_t) TIMESTAMP_DELAY >> 8) | 0x80;
	header[headerPtr++] = (uint8_t) ((uint16_t) TIMESTAMP_DELAY & 0xFF);

	// Set tempo (SM64 only allows a minimum tempo of 1 in vanilla, but this value will still be compatible. Modding it to support a tempo of 0 is very easy and recommended, but not that important.)
	header[headerPtr++] = SEQ_TEMPO;
	if (gTimestamp >= 0) // If not looping
		header[headerPtr++] = gTempo;
	else
		header[headerPtr++] = 0x00;

	// Wait for ideally an indefinite amount of time (or at least as indefinite as possible)
	header[headerPtr++] = SEQ_TIMESTAMP;
	if (gTimestamp >= 0) {
		header[headerPtr++] = (uint8_t) ((uint16_t) gTimestamp >> 8) | 0x80;
		header[headerPtr++] = (uint8_t) ((uint16_t) gTimestamp & 0xFF);
	} else {
		header[headerPtr++] = (uint8_t) ((uint16_t) MAX_DURATION >> 8) | 0x80;
		header[headerPtr++] = (uint8_t) ((uint16_t) MAX_DURATION & 0xFF);
	}

	/* Almost everything past this point is unnecessary if looping, but still here just in case. */

	// Loop back to channel pointers. If adding/removing anything from this header, the following value should be updated accordingly.
	if (gTimestamp < 0) {
		header[headerPtr++] = SEQ_BRANCH_ABS_ALWAYS; // Loop sequence to address of first channel pointer
		header[headerPtr++] = 0x00;
		header[headerPtr++] = 0x09;
	}

	// The sequence has ended, disable all channels that were being used. This will in theory never get called unless removing the branch command.
	header[headerPtr++] = SEQ_CHANNEL_DISABLE;
	header[headerPtr++] = (uint8_t) (enabledChannels >> 8);
	header[headerPtr++] = (uint8_t) enabledChannels;

	// End of sequence header
	header[headerPtr++] = SEQ_END_OF_DATA;

	// If these values don't match, then something is wrong!
	if (seqHeaderSize != headerPtr) {
		warnings += "FATAL WARNING! Precalculated sequence header size does not match output! Your output sequence may not work!\n";
		warnings += "EXPECTED: " + to_string(seqHeaderSize) + " bytes, ACTUAL: " + to_string(headerPtr) + " bytes\n";
	}

	// Write sequence header to file
	fwrite(header, 1, headerPtr, seqFile);

	// Free header buffer from memory
	delete[] header;
}

void CHNHeader::write_chn_header(FILE *seqFile, uint8_t channelCount, uint16_t seqHeaderSize) {
	uint8_t *header = new uint8_t[CHN_HEADER_SIZE]; // Data buffer for temporary storage before printing
	size_t headerPtr = 0; // Initialize data pointer to 0

	// Calculate track pointer offset
	uint16_t trackPtr = (uint16_t) (seqHeaderSize + CHN_HEADER_SIZE * channelCount);

	// Start of channel header
	header[headerPtr++] = CHN_START;

	// How the sequence behaves when pausing the game (0x20 softens music on pause)
	header[headerPtr++] = CHN_TRACK_POINTER;
	header[headerPtr++] = (uint8_t) (trackPtr >> 8);
	header[headerPtr++] = (uint8_t) trackPtr;

	// Channel panning
	header[headerPtr++] = CHN_PAN;
	header[headerPtr++] = this->pan;

	// Set channel volume to max value
	header[headerPtr++] = CHN_VOLUME;
	header[headerPtr++] = 0x7F;

	// Set pitch bend to 0
	header[headerPtr++] = CHN_PITCH_BEND;
	header[headerPtr++] = 0x00;

	// Set reverb effect to 0
	header[headerPtr++] = CHN_EFFECT;
	header[headerPtr++] = 0x00;

	// Set channel's priority to maximum so it doesn't get overridden by any sound effects
	header[headerPtr++] = CHN_PRIORITY_US_MAX;

	// Set the channel instrument to use
	header[headerPtr++] = CHN_INSTRUMENT;
	header[headerPtr++] = this->instrument;

	// Set channel timestamp to ideally an indefinite amount of time (or at least as indefinite as possible)
	header[headerPtr++] = CHN_TIMESTAMP;
	if (gTimestamp >= 0) {
		header[headerPtr++] = (uint8_t) ((uint16_t) (gTimestamp + TIMESTAMP_DELAY) >> 8) | 0x80;
		header[headerPtr++] = (uint8_t) ((uint16_t) (gTimestamp + TIMESTAMP_DELAY) & 0xFF);
	} else {
		header[headerPtr++] = (uint8_t) ((uint16_t) (MAX_DURATION + TIMESTAMP_DELAY) >> 8) | 0x80;
		header[headerPtr++] = (uint8_t) ((uint16_t) (MAX_DURATION + TIMESTAMP_DELAY) & 0xFF);
	}

	// End of channel header
	header[headerPtr++] = CHN_END_OF_DATA;

	// If these values don't match, then something is wrong!
	if (CHN_HEADER_SIZE != headerPtr) {
		warnings += "FATAL WARNING! Precalculated channel header size does not match output! Your output sequence may not work!\n";
		warnings += "EXPECTED: " + to_string(CHN_HEADER_SIZE) + " bytes, ACTUAL: " + to_string(headerPtr) + " bytes\n";
	}

	// Write sequence header to file
	fwrite(header, 1, headerPtr, seqFile);

	// Free header buffer from memory
	delete[] header;
}

void SEQFile::write_trk_header(FILE *seqFile) {
	uint8_t *data = new uint8_t[TRK_HEADER_SIZE]; // Data buffer for temporary storage before printing
	size_t dataPtr = 0; // Initialize data pointer to 0

	// Layer transpose; this should be zeroed
	data[dataPtr++] = TRK_TRANSPOSE;
	data[dataPtr++] = 0x00;

	// Wait 5 game ticks to play note. See description in `write_seq_header` for more details.
	data[dataPtr++] = TRK_TIMESTAMP;
	data[dataPtr++] = (uint8_t) ((uint16_t) (TIMESTAMP_DELAY - 1) >> 8) | 0x80;
	data[dataPtr++] = (uint8_t) ((uint16_t) (TIMESTAMP_DELAY - 1) & 0xFF);

	// Play note with timestamp and velocity 
	data[dataPtr++] = TRK_NOTE_TV + 0x27; // Middle C
	if (gTimestamp >= 0) {
		data[dataPtr++] = (uint8_t) ((uint16_t) (gTimestamp + 1) >> 8) | 0x80;
		data[dataPtr++] = (uint8_t) ((uint16_t) (gTimestamp + 1) & 0xFF);
	} else {
		data[dataPtr++] = (uint8_t) ((uint16_t) (MAX_DURATION + 1) >> 8) | 0x80;
		data[dataPtr++] = (uint8_t) ((uint16_t) (MAX_DURATION + 1) & 0xFF);
	}
	data[dataPtr++] = 0x7F; // Velocity

	// End of track data
	data[dataPtr++] = TRK_END_OF_DATA;

	// If these values don't match, then something is wrong!
	if (TRK_HEADER_SIZE != dataPtr) {
		warnings += "FATAL WARNING! Precalculated track data size does not match output! Your output sequence may not work!\n";
		warnings += "EXPECTED: " + to_string(TRK_HEADER_SIZE) + " bytes, ACTUAL: " + to_string(dataPtr) + " bytes\n";
	}

	// Write track data to file
	fwrite(data, 1, dataPtr, seqFile);

	// Free data buffer from memory
	delete[] data;
}

int SEQFile::write_sequence() {
	FILE *seqFile;

	printf("Generating sequence file...");
	fflush(stdout);

	string tmpFilename = this->filename;
	size_t slash = tmpFilename.find_last_of("/\\");
	if (slash == string::npos)
		tmpFilename = "XX_" + tmpFilename + ".m64";
	else
		tmpFilename = tmpFilename.substr(0, slash+1) + "XX_" + tmpFilename.substr(slash+1) + ".m64";

	seqFile = fopen(tmpFilename.c_str(), "wb");
	if (seqFile == NULL) {
		printf("...FAILED!\nERROR: Could not open %s for writing!\n", this->filename.c_str());
		return RETURN_SEQUENCE_CANNOT_CREATE_FILE;
	}

	warnings = "";

	uint16_t seqHeaderSize = (uint16_t) (SEQ_HEADER_SIZE + channelCount * ABS_PTR_SIZE); // Size of SEQ header
	if (gTimestamp < 0) { // If looping
		seqHeaderSize += 3;
	}

	this->seqhead->write_seq_header(seqFile, seqHeaderSize);

	for (size_t i = 0; i < this->channelCount; i++) {
		this->chnHeader[i]->write_chn_header(seqFile, this->channelCount, seqHeaderSize);
	}

	write_trk_header(seqFile);

	fclose(seqFile);

	printf("...DONE!\n");
	printf("%s", warnings.c_str());

	return RETURN_SUCCESS;
}

int generate_new_sequence(string filename, uint16_t instFlags) {
	uint8_t numChannels = 0;

	if (gNumChannels == 0) {
		for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) {
			if (!((1 << i) & instFlags))
				continue;

			numChannels++;
		}
	} else {
		numChannels = gNumChannels;
		instFlags = (1ULL << numChannels) - 1ULL;
	}

	if (numChannels == 0)
		return RETURN_SEQUENCE_NO_CHANNELS;

	SEQFile sequence(filename, instFlags, numChannels);

	return sequence.write_sequence();
}
