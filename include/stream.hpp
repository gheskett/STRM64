#ifndef STREAM_HPP
#define STREAM_HPP

#include <string>

extern "C" {
#include "vgmstream.h"
#include "libswresample/swresample.h"
}

#define SAMPLE_COUNT_PADDING 0x10
#define MIN_PRINT_BUFFER_SIZE 0x1000

class AudioOutData {
    int32_t sampleRate;
    int enableLoop;
    int32_t loopStartSamples;
    int32_t loopEndSamples;
    int32_t numSamples;
    int numChannels;

public:
	AudioOutData(VGMSTREAM *inFileProperties);
	~AudioOutData();
    std::string samples_to_us_print(uint64_t sampleOffset);
    void print_header_info();
    int check_properties(std::string newFilename);
    void calculate_aiff_file_size();
    void write_form_header(FILE *streamFile);
    void write_comm_header(FILE *streamFile);
    void write_mark_header(FILE *streamFile);
    void write_inst_header(FILE *streamFile);
    void write_ssnd_header(FILE *streamFile);
    void write_stream_headers(FILE **streamFiles);
    void write_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles);
    void resample_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles);
    int write_streams(VGMSTREAM *inFileProperties, std::string newFilename, std::string oldFilename);
};
		
int generate_new_streams(VGMSTREAM *inFileProperties, std::string newFilename, std::string oldFilename);
void set_sample_rate(int64_t sampleRate);
void set_enable_loop(int64_t isLoopingEnabled);
void set_loop_start_samples(int64_t samples);
void set_loop_end_samples(int64_t samples);
void set_loop_start_microseconds(int64_t microseconds);
void set_loop_end_microseconds(int64_t microseconds);

#endif
