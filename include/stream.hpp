#ifndef STREAM_HPP
#define STREAM_HPP

#include <string>
#include <stdint.h>

extern "C" {
#include "vgmstream.h"
#include "libswresample/swresample.h"
}

#define SAMPLE_COUNT_PADDING 0x10
#define MIN_PRINT_BUFFER_SIZE 0x1000

class AudioOutData {
    bool resample;
    bool vgmstreamLoopPointMismatch;
    int32_t sampleRate;
    int enableLoop;
    int32_t loopStartSamples;
    int32_t loopEndSamples;
    int32_t numSamples;
    int32_t resampledSampleRate;
    int32_t resampledLoopStartSamples;
    int32_t resampledLoopEndSamples;
    int32_t resampledNumSamples;
    int numChannels;
    struct SwrContext *resampleContext;

public:
	AudioOutData(VGMSTREAM *inFileProperties);
	~AudioOutData();
    void print_header_info();
    void set_sequence_duration_120bpm();
    int check_properties(VGMSTREAM *inFileProperties, std::string newFilename);
    void calculate_aiff_file_size();
    void write_form_header(FILE *streamFile);
    void write_comm_header(FILE *streamFile);
    void write_mark_header(FILE *streamFile);
    void write_inst_header(FILE *streamFile);
    void write_ssnd_header(FILE *streamFile);
    void write_stream_headers(FILE **streamFiles);
    int init_audio_resampling(VGMSTREAM *inFileProperties, int inputBufferSize);
    void cleanup_resample_context();
    int resample_audio_data(const sample_t *inputAudioBuffer, sample_t *audioOutBuffer, sample_t *printBuffer,
     FILE **streamFiles, int inputBufferSize, int outputBufferSamples, uint32_t samplesPadded, uint32_t *totalSamplesProcessed);
    int write_resampled_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles);
    void write_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles);
    int write_streams(VGMSTREAM *inFileProperties, std::string newFilename, std::string oldFilename);
};

int generate_new_streams(VGMSTREAM *inFileProperties, std::string newFilename, std::string oldFilename, bool shouldGenerateFiles);
void set_sample_rate(int64_t sampleRate);
void set_resample_rate(int64_t resampleRate);
void set_enable_loop(int64_t isLoopingEnabled);
void set_loop_start_samples(int64_t samples);
void set_loop_end_samples(int64_t samples);
void set_loop_start_timestamp(std::string arg);
void set_loop_end_timestamp(std::string arg);
std::string print_timestamp(uint64_t microseconds);
int64_t samples_to_us(uint64_t sampleOffset, uint64_t sampleRate);
int64_t timestamp_to_us(std::string dur);

#endif
