#include <stdio.h>
#include <stdlib.h>

#include "main.hpp"
#include "stream.hpp"
#include "bswp.hpp"

using namespace std;

#define MAX_INT64_T 0x7FFFFFFFFFFFFFFFLL

#define SAMPLE_COUNT_PADDING 0x400
#define SAMPLE_RATE_MULTIPLE_CONSTANT 0x400E
#define FORM_HEADER_SIZE 0x0C
#define COMM_HEADER_SIZE 0x1A
#define MARK_HEADER_SIZE 0x20
#define INST_HEADER_SIZE 0x1C
#define SSND_PRE_HEADER_SIZE 0x10


// Override parameters
static int64_t ovrdSampleRate = -1;
static int64_t ovrdEnableLoop = -1;
static int64_t ovrdLoopStartSamples = MAX_INT64_T;
static int64_t ovrdLoopEndSamples = MAX_INT64_T;
static int64_t ovrdLoopStartMicro = MAX_INT64_T;
static int64_t ovrdLoopEndMicro = MAX_INT64_T;

static uint32_t gFileSize = 0;


void set_sample_rate(int64_t sampleRate) {
	if (sampleRate <= 0) {
		print_param_warning("sample rate");
		return;
	}

	ovrdSampleRate = sampleRate;
}

void set_enable_loop(int64_t isLoopingEnabled) {
	if (!(isLoopingEnabled == 0 || isLoopingEnabled == 1)) {
		print_param_warning("loop enable/disable");
		return;
	}

	ovrdEnableLoop = isLoopingEnabled;
}

void set_loop_start_samples(int64_t samples) {
	if (samples >= 0x100000000) {
		print_param_warning("loop start (samples)");
		return;
	}

	ovrdEnableLoop = 1;
	ovrdLoopStartSamples = samples;
	ovrdLoopStartMicro = MAX_INT64_T;
}

void set_loop_end_samples(int64_t samples) {
	if (samples >= 0x100000000) {
		print_param_warning("loop end (samples)");
		return;
	}

	ovrdLoopEndSamples = samples;
	ovrdLoopEndMicro = MAX_INT64_T;
}

void set_loop_start_microseconds(int64_t microseconds) {
	ovrdEnableLoop = 1;
	ovrdLoopStartMicro = microseconds;
	ovrdLoopStartSamples = MAX_INT64_T;
}

void set_loop_end_microseconds(int64_t microseconds) {
	ovrdLoopEndMicro = microseconds;
	ovrdLoopEndSamples = MAX_INT64_T;
}


char get_num_to_hex(uint8_t num) {
	char ret = (num & 0x0F) + 48;
	if (ret >= 58)
		ret += 7;

	return ret;
}

int64_t sample_to_us(int64_t sampleRate, int64_t sampleOffset) {
	return (int64_t) (((long double) sampleOffset / (long double) sampleRate) * 1000000.0 + 0.5);
}

int check_properties(VGMSTREAM *inFileProperties, string newFilename) {
	if (inFileProperties->sample_rate <= 0) {
		printf("ERROR: Input file has invalid sample rate value!\n");
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (inFileProperties->num_samples <= 0) {
		printf("ERROR: Input file has no sample data!\n");
		return RETURN_STREAM_INVALID_PARAMETERS;
	}

	// Overridden sample rate
	if (ovrdSampleRate >= 0) {
		inFileProperties->sample_rate = ovrdSampleRate;
	}

	// Overridden loop flag
	if (ovrdEnableLoop >= 0) {
		if (ovrdEnableLoop && !inFileProperties->loop_flag) {
			inFileProperties->loop_start_sample = 0;
			inFileProperties->loop_end_sample = inFileProperties->num_samples;
		}
		inFileProperties->loop_flag = (int) ovrdEnableLoop;
	}

	// Overridden total sample count / end loop point
	if (ovrdLoopEndSamples != MAX_INT64_T) {
		if (ovrdLoopEndSamples > 0) {
			if (inFileProperties->num_samples < ovrdLoopEndSamples) {
				ovrdLoopEndSamples = inFileProperties->num_samples;
			}
			else {
				inFileProperties->num_samples = ovrdLoopEndSamples;
				if (ovrdEnableLoop)
					inFileProperties->loop_end_sample = ovrdLoopEndSamples;
			}
		}
		else {
			inFileProperties->num_samples = ovrdLoopEndSamples + inFileProperties->num_samples;
			if (ovrdEnableLoop)
				inFileProperties->loop_end_sample = ovrdLoopEndSamples + inFileProperties->num_samples;
		}
	}
	// Overridden total sample count / end loop point, represented in microseconds
	else if (ovrdLoopEndMicro != MAX_INT64_T) {
		int64_t tmpNumSamples = sample_to_us(inFileProperties->sample_rate, ovrdLoopEndMicro);
		if (tmpNumSamples > 0) {
			if (inFileProperties->num_samples < tmpNumSamples) {
				tmpNumSamples = inFileProperties->num_samples;
			}
			else {
				inFileProperties->num_samples = tmpNumSamples;
				if (ovrdEnableLoop)
					inFileProperties->loop_end_sample = tmpNumSamples;
			}
		}
		else {
			inFileProperties->num_samples = tmpNumSamples + inFileProperties->num_samples;
			if (ovrdEnableLoop)
				inFileProperties->loop_end_sample = tmpNumSamples + inFileProperties->num_samples;
		}
	}

	// Loop end / stream length validation checks
	if (inFileProperties->loop_flag && inFileProperties->loop_end_sample != inFileProperties->num_samples) {
		if (inFileProperties->loop_end_sample > inFileProperties->num_samples)
			inFileProperties->loop_end_sample = inFileProperties->num_samples;
		else
			inFileProperties->num_samples = inFileProperties->loop_end_sample;
	}

	// Loop Start, only handled if looping is enabled
	if (inFileProperties->loop_flag) {
		// Overridden start loop point
		if (ovrdLoopStartSamples != MAX_INT64_T) {
			if (ovrdLoopStartSamples >= 0) {
				inFileProperties->loop_start_sample = ovrdLoopStartSamples;
			}
			else {
				inFileProperties->loop_start_sample = ovrdLoopStartSamples + inFileProperties->num_samples;
			}
		}
		// Overridden start loop point, represented in microseconds
		else if (ovrdLoopStartMicro != MAX_INT64_T) {
			int64_t tmpNumSamples = sample_to_us(inFileProperties->sample_rate, ovrdLoopStartMicro);
			if (tmpNumSamples >= 0) {
				inFileProperties->loop_start_sample = tmpNumSamples;
			}
			else {
				inFileProperties->loop_start_sample = tmpNumSamples + inFileProperties->num_samples;
			}
		}
	}

	if (inFileProperties->num_samples <= 0) {
		printf("ERROR: Negative stream length value extends beyond the original stream length!\n");
		printf("ATTEMPTED VALUE: %d\n", inFileProperties->num_samples);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (inFileProperties->loop_flag && inFileProperties->loop_end_sample <= inFileProperties->loop_start_sample) {
		printf("ERROR: Starting loop point must be smaller than ending loop point!\n");
		printf("LOOP_START: %d, LOOP_END: %d\n", inFileProperties->loop_start_sample, inFileProperties->loop_end_sample);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (inFileProperties->loop_flag && inFileProperties->loop_start_sample < 0) {
		printf("ERROR: Negative starting loop point value extends beyond the total stream length!\n");
		printf("ATTEMPTED VALUE: %d\n", inFileProperties->loop_start_sample);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}

	return RETURN_SUCCESS;
}


void calculate_aiff_file_size(VGMSTREAM *inFileProperties) {
	gFileSize = 0;

	gFileSize += FORM_HEADER_SIZE;
	gFileSize += COMM_HEADER_SIZE;

	if (inFileProperties->loop_flag) {
		gFileSize += MARK_HEADER_SIZE;
		gFileSize += INST_HEADER_SIZE;
	}

	gFileSize += SSND_PRE_HEADER_SIZE;

	int32_t samplesPadded = inFileProperties->num_samples;
	if (samplesPadded % SAMPLE_COUNT_PADDING)
		samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);

	gFileSize += samplesPadded * sizeof(sample_t);
}


void write_form_header(VGMSTREAM *inFileProperties, FILE *streamFile) {
	const char formHeader[] = "FORM";
	const char aiffHeader[] = "AIFF";
	uint32_t bswpFileSize = bswap_32(gFileSize - 8);

	// FORM [0x00]
	fwrite(formHeader, 1, 4, streamFile);

	// File Size - 8 [0x04]
	fwrite(&bswpFileSize, 4, 1, streamFile);

	// AIFF [0x08]
	fwrite(aiffHeader, 1, 4, streamFile);
}

void write_comm_header(VGMSTREAM *inFileProperties, FILE *streamFile) {
	const char commHeader[] = "COMM";
	uint16_t tmp16BitValue;
	uint32_t tmp32BitValue;

	// COMM [0x00]
	fwrite(commHeader, 1, 4, streamFile);

	// COMM Size - 8 [0x04]
	tmp32BitValue = bswap_32((uint32_t) (COMM_HEADER_SIZE - 8));
	fwrite(&tmp32BitValue, 4, 1, streamFile);
	
	// Channel Count (always 1 in this case) [0x08]
	tmp16BitValue = bswap_16((uint16_t) 1);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Number of Samples, padded to SAMPLE_COUNT_PADDING [0x0A]
	tmp32BitValue = inFileProperties->num_samples;
	if (tmp32BitValue % SAMPLE_COUNT_PADDING)
		tmp32BitValue += SAMPLE_COUNT_PADDING - (tmp32BitValue % SAMPLE_COUNT_PADDING);
	tmp32BitValue = bswap_32(tmp32BitValue);
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Bit Depth (always 16) [0x0E]
	tmp16BitValue = bswap_16((uint16_t) 16);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Calculate sample rate stuffs, weirdly complicated to calculate manually
	uint16_t sampleRateMultiple = SAMPLE_RATE_MULTIPLE_CONSTANT;
	uint32_t sampleRateCurrent = (uint32_t) inFileProperties->sample_rate;
	uint64_t sampleRateRemainder = 0;
	while (sampleRateCurrent < 0x8000) {
		sampleRateMultiple--;
		sampleRateCurrent <<= 1;
	}
	while (sampleRateCurrent > 0xFFFF) {
		sampleRateMultiple++;
		sampleRateRemainder >>= 1;
		sampleRateRemainder |= ((sampleRateCurrent & 1) << (sizeof(sampleRateRemainder) - 1));
		sampleRateCurrent >>= 1;
	}

	// Sample Rate Exponential Multiple [0x10]
	tmp16BitValue = bswap_16(sampleRateMultiple);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Modified Sample Rate [0x12]
	tmp16BitValue = bswap_16((uint16_t) sampleRateCurrent);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Modified Sample Rate Remainder (6 bytes) [0x14]
	fwrite(&sampleRateRemainder, 1, 6, streamFile); // Proper endianness check not used here, should be addressed later if it ever matters
}

void write_mark_header(VGMSTREAM *inFileProperties, FILE *streamFile) {
	const char markHeader[] = "MARK";
	const char startMarker[] = "start";
	const char endMarker[] = "end";
	uint8_t tmp8BitValue;
	uint16_t tmp16BitValue;
	uint32_t tmp32BitValue;

	// MARK [0x00]
	fwrite(markHeader, 1, 4, streamFile);

	// MARK Size - 8 [0x04]
	tmp32BitValue = bswap_32((uint32_t) (MARK_HEADER_SIZE - 8));
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Marker Count (Always 2 in this case) [0x08]
	tmp16BitValue = bswap_16((uint16_t) 2);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// First Marker ID (Indexed at 1) [0x0A]
	tmp16BitValue = bswap_16((uint16_t) 1);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Sample Offset (Loop Start Value) [0x0C]
	tmp32BitValue = bswap_32((uint32_t) (inFileProperties->loop_start_sample));
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Marker Id ("start" is 5 characters) [0x10]
	tmp8BitValue = 5;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// "start" (Loop Start Marker) [0x11]
	fwrite(startMarker, 1, 5, streamFile);

	// Second Marker ID (Indexed at 1) [0x16]
	tmp16BitValue = bswap_16((uint16_t) 2);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Sample Offset (Loop End Value) [0x18]
	tmp32BitValue = bswap_32((uint32_t) (inFileProperties->loop_end_sample));
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Marker Id ("end" is 3 characters) [0x1C]
	tmp8BitValue = 3;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// "end" (Loop End Marker) [0x1D]
	fwrite(endMarker, 1, 3, streamFile);
}

// May not even be needed, but here just in case
void write_inst_header(VGMSTREAM *inFileProperties, FILE *streamFile) {
	const char instHeader[] = "INST";
	uint8_t tmp8BitValue;
	uint16_t tmp16BitValue;
	uint32_t tmp32BitValue;

	// INST [0x00]
	fwrite(instHeader, 1, 4, streamFile);

	// INST Size - 8 [0x04]
	tmp32BitValue = bswap_32((uint32_t) (INST_HEADER_SIZE - 8));
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Base Note (0) [0x08]
	tmp8BitValue = 0;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// Detune (0) [0x09]
	tmp8BitValue = 0;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// Low Note (0) [0x0A]
	tmp8BitValue = 0;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// High Note (0) [0x0B]
	tmp8BitValue = 0;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// Low Velocity (0) [0x0C]
	tmp8BitValue = 0;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// High Velocity (0) [0x0D]
	tmp8BitValue = 0;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// Gain (0) [0x0E]
	tmp16BitValue = bswap_16((uint16_t) 0);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Sustain Loop? (0) [0x10]
	tmp32BitValue = bswap_32((uint32_t) 0x10001);
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Release Loop? (0) [0x14]
	tmp32BitValue = bswap_32((uint32_t) 0x20000);
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Padding? [0x18]
	tmp32BitValue = bswap_32((uint32_t) 0);
	fwrite(&tmp32BitValue, 4, 1, streamFile);
}

void write_ssnd_header(VGMSTREAM *inFileProperties, FILE *streamFile) {
	const char ssndHeader[] = "SSND";
	uint32_t tmp32BitValue;

	// SSND [0x00]
	fwrite(ssndHeader, 1, 4, streamFile);

	// SSND Size - 8 [0x04]
	uint32_t samplesPadded = (uint32_t) inFileProperties->num_samples;
	if (samplesPadded % SAMPLE_COUNT_PADDING)
		samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);
	tmp32BitValue = bswap_32((uint32_t) (SSND_PRE_HEADER_SIZE + samplesPadded * sizeof(sample_t) - 8));
	fwrite(&tmp32BitValue, 4, 1, streamFile);
	
	// Offset (always 0 in this case) [0x08]
	tmp32BitValue = bswap_32((uint32_t) 0);
	fwrite(&tmp32BitValue, 4, 1, streamFile);
	
	// Block Size (always 0 in this case) [0x0C]
	tmp32BitValue = bswap_32((uint32_t) 0);
	fwrite(&tmp32BitValue, 4, 1, streamFile);
}

void write_stream_headers(VGMSTREAM *inFileProperties, FILE **streamFiles) {
	for (int i = 0; i < inFileProperties->channels; i++) {
		write_form_header(inFileProperties, streamFiles[i]);
		write_comm_header(inFileProperties, streamFiles[i]);

		if (inFileProperties->loop_flag) {
			write_mark_header(inFileProperties, streamFiles[i]);
			write_inst_header(inFileProperties, streamFiles[i]);
		}

		write_ssnd_header(inFileProperties, streamFiles[i]);
	}
}

void write_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles) {
	uint32_t samplesPadded = (uint32_t) inFileProperties->num_samples;
	if (samplesPadded % SAMPLE_COUNT_PADDING)
		samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);

	uint32_t loopCount = (uint32_t) (samplesPadded / SAMPLE_COUNT_PADDING);

	sample_t *audioBuffer = new sample_t[SAMPLE_COUNT_PADDING * inFileProperties->channels];

	sample_t **printBuffer = new sample_t*[inFileProperties->channels];
	for (int i = 0; i < inFileProperties->channels; i++)
		printBuffer[i] = new sample_t[SAMPLE_COUNT_PADDING];

	for (uint32_t i = 0; i < loopCount; i++) {
		int samplesRendered = render_vgmstream(audioBuffer, SAMPLE_COUNT_PADDING, inFileProperties);

		for (uint32_t j = samplesRendered * inFileProperties->channels; j < SAMPLE_COUNT_PADDING * (uint32_t) inFileProperties->channels; j++)
			audioBuffer[j] = 0;

		for (uint32_t j = 0; j < SAMPLE_COUNT_PADDING * (uint32_t) inFileProperties->channels; j++)
			printBuffer[j % inFileProperties->channels][j / inFileProperties->channels] = (sample_t) bswap_16((uint16_t) audioBuffer[j]);

		// printf("%d\n", samplesRendered);

		for (int32_t j = 0; j < inFileProperties->channels; j++)
			fwrite(printBuffer[j], sizeof(sample_t), SAMPLE_COUNT_PADDING, streamFiles[j]);
	}

	for (int i = 0; i < inFileProperties->channels; i++)
		delete[] printBuffer[i];
	delete[] printBuffer;
	delete[] audioBuffer;
}

int write_streams(VGMSTREAM *inFileProperties, string newFilename) {
	FILE **streamFiles = new FILE*[inFileProperties->channels];

	calculate_aiff_file_size(inFileProperties);
	print_header_info(true, gFileSize);

	printf("Generating streamed file(s)...");
	fflush(stdout);
	for (int i = 0; i < inFileProperties->channels; i++) {
		streamFiles[i] = fopen((newFilename + "_" + get_num_to_hex((uint8_t) i) + ".aiff").c_str(), "wb");
		if (!streamFiles[i]) {
			printf("...FAILED!\nERROR: Could not open %s for writing!\n", (newFilename + "_" + get_num_to_hex((uint8_t) i) + ".aiff").c_str());

			for (int j = i - 1; j >= 0; j--)
				fclose(streamFiles[j]);

			delete[] streamFiles;
			return RETURN_STREAM_CANNOT_CREATE_FILE;
		}
	}

	write_stream_headers(inFileProperties, streamFiles);

	write_audio_data(inFileProperties, streamFiles);

	for (int i = 0; i < inFileProperties->channels; i++)
		fclose(streamFiles[i]);

	delete[] streamFiles;

	printf("...DONE!\n");

	return RETURN_SUCCESS;
}

int generate_new_streams(VGMSTREAM *inFileProperties, string newFilename) {
	if (!inFileProperties)
		return 1;

	int ret = check_properties(inFileProperties, newFilename);
	if (ret)
		return ret;
	ret = write_streams(inFileProperties, newFilename);

	return ret;
}
