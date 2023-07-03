#ifndef SEQUENCE_HPP
#define SEQUENCE_HPP

#include <string>

enum SEQCommands {
	SEQ_CHANNEL_POINTER = 0x90, // 0x90-0x9F
	SEQ_MUTE_BEHAVIOR = 0xD3,
	SEQ_MUTE_SCALE = 0xD5,
	SEQ_CHANNEL_DISABLE = 0xD6,
	SEQ_CHANNEL_ENABLE = 0xD7,
	SEQ_VOLUME = 0xDB,
	SEQ_TEMPO = 0xDD,
	SEQ_BRANCH_ABS_ALWAYS = 0xFB,
	SEQ_TIMESTAMP = 0xFD, // 0xFDXXXX or 0xFDXX, dependent on the MSB of first byte
	SEQ_END_OF_DATA = 0xFF
};

enum ChannelCommands {
	CHN_PRIORITY_US = 0x60, // 0x60-0x6F
	CHN_PRIORITY_US_MAX = 0x6F,
	CHN_TRACK_POINTER = 0x90,
	CHN_INSTRUMENT = 0xC1,
	CHN_START = 0xC4,
	CHN_PITCH_BEND = 0xD3,
	CHN_EFFECT = 0xD4,
	CHN_PAN = 0xDD,
	CHN_VOLUME = 0xDF,
	CHN_TIMESTAMP = 0xFD, // 0xFDXXXX or 0xFDXX, dependent on the MSB of first byte
	CHN_END_OF_DATA = 0xFF
};

enum TrackCommands {
	TRK_NOTE_TVG = 0x00, // 0x00 + Note Value, Timestamp, Velocity, Gate Time
	TRK_NOTE_TV = 0x40, // 0x40 + Note Value, Timestamp, Velocity
	TRK_NOTE_VG = 0x80, // 0x80 + Note Value, Velocity, Gate Time
	TRK_TIMESTAMP = 0xC0, // 0xC0XXXX or 0xC0XX, determined by the MSB of first byte
	TRK_TRANSPOSE = 0xC2,
	TRK_END_OF_DATA = 0xFF
};

class SEQHeader {
	uint16_t channelFlags;
	uint8_t channelCount;
	int8_t muteScale;
	uint8_t volume;

public:
	SEQHeader(uint16_t instFlags, uint8_t numChannels);
	~SEQHeader();

	void write_seq_header(FILE *seqFile);
};

class CHNHeader {
	uint8_t channelId;
	uint8_t instrument;
	uint8_t pan;

public:
	CHNHeader(uint8_t channelIndex, uint8_t instId, uint8_t numChannels);
	~CHNHeader();

	void write_chn_header(FILE *seqFile, uint8_t channelCount);
};

class SEQFile {
	std::string filename;
	uint8_t channelCount;
	uint16_t channelFlags;
	SEQHeader *seqhead;
	CHNHeader **chnHeader;

public:
	SEQFile(std::string fname, uint16_t instFlags, uint8_t numChannels);
	~SEQFile();

	int write_sequence();
	void write_trk_header(FILE *seqFile);
};

uint8_t seq_get_num_channels();
bool seq_set_num_channels(int64_t numChannels);
void seq_set_mute_scale(int64_t muteScale);
void seq_set_master_volume(int64_t volume);

int generate_new_sequence(std::string filename, uint16_t instFlags);

#endif
