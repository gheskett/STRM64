#include <stdio.h>
#include <stdlib.h>

#include "main.hpp"
#include "sequence.hpp"

using namespace std;


#define MUTE_SCALE_DEFAULT 0x3F
#define MASTER_VOLUME_DEFAULT 0x7F

// These must be changed when manually adding/removing fields
#define SEQ_HEADER_SIZE 0x19 // Exclusive of Channel Pointer commands
#define CHN_HEADER_SIZE 0x13
#define TRK_HEADER_SIZE 0x09
#define ABS_PTR_SIZE 0x03


static int8_t gMuteScale = MUTE_SCALE_DEFAULT;
static uint8_t gMasterVolume = MASTER_VOLUME_DEFAULT;


SEQHeader::SEQHeader(uint16_t instFlags, uint8_t numChannels) {
	channelFlags = instFlags;
	channelCount = numChannels;
	muteScale = gMuteScale;
	volume = gMasterVolume;
}
SEQHeader::~SEQHeader() {

}

CHNHeader::CHNHeader(uint8_t channelIndex, uint8_t instId, uint8_t numChannels) {
	channelId = NUM_CHANNELS_MAX - numChannels + channelIndex;
	instrument = instId;
	
	// TODO: make channel panning overrideable
	if (channelIndex % 2) { // right channel
		pan = 0x7F;
	}
	else { // left/mono channel
		if (channelIndex + 1 == numChannels) // mono channel
			pan = 0x3F;
		else // left channel
			pan = 0x00;
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
	gMasterVolume = (uint8_t) volume;
}

void SEQHeader::write_seq_header(FILE *seqFile) {
	uint16_t seqHeaderSize = (uint16_t) (SEQ_HEADER_SIZE + this->channelCount * ABS_PTR_SIZE); // Size of SEQ header
	uint8_t *header = new uint8_t[seqHeaderSize]; // Data buffer for temporary storage before printing
	size_t headerPtr = 0; // Initialize data pointer to 0

	// Calculate channels being used with sequence. This intentionally generates channels from MAX - n to MAX, rather than from 0 to n.
	uint16_t enabledChannels = 0;
	for (uint8_t i = NUM_CHANNELS_MAX - this->channelCount; i < NUM_CHANNELS_MAX; i++) {
		enabledChannels |= (1 << i);
	}

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
	for (uint8_t i = NUM_CHANNELS_MAX - this->channelCount; i < NUM_CHANNELS_MAX; i++) {
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

	// Set tempo to 0x30
	header[headerPtr++] = SEQ_TEMPO;
	header[headerPtr++] = 0x30;

	// Wait for 6 audio ticks to pass
	header[headerPtr++] = SEQ_TIMESTAMP;
	header[headerPtr++] = 0x06;

	// Set tempo to 0x00 (SM64 only allows a minimum tempo of 1 in vanilla, but this value will still be compatible. Modding it to support a tempo of 0 is very easy and recommended, but not that important.)
	header[headerPtr++] = SEQ_TEMPO;
	header[headerPtr++] = 0x00;

	// Wait for ideally an indefinite amount of time (or at least as indefinite as possible)
	header[headerPtr++] = SEQ_TIMESTAMP;
	header[headerPtr++] = 0xFF;
	header[headerPtr++] = 0xF9;

	/* Almost everything past this point is unnecessary given the above timestamp, but still here just in case. */

	// Loop back to channel pointers. If adding/removing anything from this header, the following value should be updated accordingly.
	header[headerPtr++] = SEQ_BRANCH_ABS_ALWAYS;
	header[headerPtr++] = 0x00;
	header[headerPtr++] = 0x09;

	// The sequence has ended, disable all channels that were being used. This will in theory never get called unless removing the branch command.
	header[headerPtr++] = SEQ_CHANNEL_DISABLE;
	header[headerPtr++] = (uint8_t) (enabledChannels >> 8);
	header[headerPtr++] = (uint8_t) enabledChannels;

	// End of sequence header
	header[headerPtr++] = SEQ_END_OF_DATA;

	// If these values don't match, then something is wrong!
	if (seqHeaderSize != headerPtr) {
		printf("FATAL WARNING! Precalculated sequence header size does not match output! Your output sequence may not work!\n");
		printf("EXPECTED: %u bytes, ACTUAL: %zu bytes\n", seqHeaderSize, headerPtr);
	}

	// Write sequence header to file
	fwrite(header, 1, headerPtr, seqFile);

	// Free header buffer from memory
	delete[] header;
}

void CHNHeader::write_chn_header(FILE *seqFile, uint8_t channelCount) {
	uint8_t *header = new uint8_t[CHN_HEADER_SIZE]; // Data buffer for temporary storage before printing
	size_t headerPtr = 0; // Initialize data pointer to 0
	uint16_t seqHeaderSize = (uint16_t) (SEQ_HEADER_SIZE + channelCount * ABS_PTR_SIZE); // Size of SEQ header

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
	header[headerPtr++] = 0xFF;
	header[headerPtr++] = 0xFF;

	// End of channel header
	header[headerPtr++] = CHN_END_OF_DATA;

	// If these values don't match, then something is wrong!
	if (CHN_HEADER_SIZE != headerPtr) {
		printf("FATAL WARNING! Precalculated channel header size does not match output! Your output sequence may not work!\n");
		printf("EXPECTED: %u bytes, ACTUAL: %zu bytes\n", CHN_HEADER_SIZE, headerPtr);
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
	data[dataPtr++] = 0x05;

	// Play note with timestamp and velocity 
	data[dataPtr++] = TRK_NOTE_TV + 0x27; // Middle C
	data[dataPtr++] = 0xFF; // Timestamp first byte
	data[dataPtr++] = 0xFA; // Timestamp second byte
	data[dataPtr++] = 0x7F; // Velocity

	// End of track data
	data[dataPtr++] = TRK_END_OF_DATA;

	// If these values don't match, then something is wrong!
	if (TRK_HEADER_SIZE != dataPtr) {
		printf("FATAL WARNING! Precalculated track data size does not match output! Your output sequence may not work!\n");
		printf("EXPECTED: %u bytes, ACTUAL: %zu bytes\n", TRK_HEADER_SIZE, dataPtr);
	}

	// Write track data to file
	fwrite(data, 1, dataPtr, seqFile);

	// Free data buffer from memory
	delete[] data;
}

int SEQFile::write_sequence() {
	FILE *seqFile;

	seqFile = fopen(("XX_" + this->filename + ".m64").c_str(), "wb");
	if (seqFile == NULL) {
		printf("ERROR: Could not open %s for writing!\n", this->filename.c_str());
		return 2;
	}

	this->seqhead->write_seq_header(seqFile);

	for (size_t i = 0; i < this->channelCount; i++) {
		this->chnHeader[i]->write_chn_header(seqFile, this->channelCount);
	}

	write_trk_header(seqFile);

	fclose(seqFile);

	return 0;
}

int generate_new_sequence(string filename, uint16_t instFlags) {
	uint8_t numChannels = 0;

	for (uint8_t i = 0; i < NUM_CHANNELS_MAX; i++) {
		if (!((1 << i) & instFlags))
			continue;

		numChannels++;
	}
	if (numChannels == 0)
		return 1;

	SEQFile sequence(filename, instFlags, numChannels);

	return sequence.write_sequence();
}
