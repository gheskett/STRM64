#include <stdio.h>
#include <stdlib.h>

#include "main.hpp"
#include "stream.hpp"
#include "bswp.hpp"

using namespace std;

#define MAX_INT64_T 0x7FFFFFFFFFFFFFFFLL

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

static bool forcedMono = false;

static uint32_t gFileSize = 0;


AudioOutData::AudioOutData(VGMSTREAM *inFileProperties) {
    sampleRate = inFileProperties->sample_rate;
    enableLoop = inFileProperties->loop_flag;
	if (enableLoop) {
		loopStartSamples = inFileProperties->loop_start_sample;
		loopEndSamples = inFileProperties->loop_end_sample;
	}
	else {
		loopStartSamples = 0;
		loopEndSamples = numSamples;
	}
    numSamples = inFileProperties->num_samples;
	numChannels = inFileProperties->channels;
}
AudioOutData::~AudioOutData() {

}


string AudioOutData::samples_to_us_print(uint64_t sampleOffset) {
	uint64_t convTime = (uint64_t) (((long double) sampleOffset / (long double) sampleRate) * 1000000.0 + 0.5);

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

void AudioOutData::print_header_info() {
	printf("\n");

	if (numChannels == 1)
		printf("    File Size of AIFF: %u bytes\n", gFileSize * (uint32_t) numChannels);
	else
		printf("    Cumulative File Size of AIFFs: %u bytes\n", gFileSize * (uint32_t) numChannels);

	printf("    Sample Rate: %d Hz", sampleRate);
	if (sampleRate > 32000)
		printf(" (Downsampling recommended!)");
	printf("\n");

	printf("    Is Looped: ");
	if (enableLoop) {
		printf("true\n");

		printf("    Starting Loop Point: %d Samples (Time: %s)\n", loopStartSamples,
			samples_to_us_print(loopStartSamples).c_str());

		printf("    Ending Loop Point: %d Samples (Time: %s)\n", loopEndSamples,
			samples_to_us_print(loopEndSamples).c_str());
	}
	else {
		printf("false\n");

		int32_t samplesPadded = numSamples;
		if (samplesPadded % SAMPLE_COUNT_PADDING)
			samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);
		printf("    End of Stream: %d Samples (Time: %s)\n", samplesPadded,
			samples_to_us_print(samplesPadded).c_str());
	}

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

void set_strm_force_mono() {
	forcedMono = true;
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

int AudioOutData::check_properties(string newFilename) {
	if (sampleRate <= 0) {
		printf("ERROR: Input file has invalid sample rate value!\n");
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (numSamples <= 0) {
		printf("ERROR: Input file has no sample data!\n");
		return RETURN_STREAM_INVALID_PARAMETERS;
	}

	// Overridden sample rate
	if (ovrdSampleRate > 0) {
		sampleRate = (int32_t) ovrdSampleRate;
	}

	// Overridden loop flag
	if (ovrdEnableLoop >= 0) {
		if (ovrdEnableLoop && !enableLoop) {
			loopStartSamples = 0;
			loopEndSamples = numSamples;
		}
		enableLoop = (int) ovrdEnableLoop;
	}

	// Overridden total sample count / end loop point
	if (ovrdLoopEndSamples != MAX_INT64_T) {
		if (ovrdLoopEndSamples > 0) {
			if (numSamples > ovrdLoopEndSamples)
				numSamples = (int32_t) ovrdLoopEndSamples;
		}
		else {
			numSamples = (int32_t) ovrdLoopEndSamples + numSamples;
		}
		if (enableLoop)
			loopEndSamples = numSamples;
	}
	// Overridden total sample count / end loop point, represented in microseconds
	else if (ovrdLoopEndMicro != MAX_INT64_T) {
		int64_t tmpNumSamples = sample_to_us(sampleRate, ovrdLoopEndMicro);
		if (tmpNumSamples > 0) {
			if (numSamples > tmpNumSamples)
				numSamples = (int32_t) tmpNumSamples;
		}
		else {
			numSamples = (int32_t) tmpNumSamples + numSamples;
		}
		if (enableLoop)
			loopEndSamples = numSamples;
	}

	// Loop end / stream length validation checks
	if (enableLoop && loopEndSamples != numSamples) {
		if (loopEndSamples > numSamples)
			loopEndSamples = numSamples;
		else
			numSamples = loopEndSamples;
	}

	// Loop Start, only handled if looping is enabled
	if (enableLoop) {
		// Overridden start loop point
		if (ovrdLoopStartSamples != MAX_INT64_T) {
			if (ovrdLoopStartSamples >= 0)
				loopStartSamples = (int32_t) ovrdLoopStartSamples;
			else
				loopStartSamples = (int32_t) ovrdLoopStartSamples + numSamples;
		}
		// Overridden start loop point, represented in microseconds
		else if (ovrdLoopStartMicro != MAX_INT64_T) {
			int64_t tmpNumSamples = sample_to_us(sampleRate, ovrdLoopStartMicro);
			if (tmpNumSamples >= 0)
				loopStartSamples = (int32_t) tmpNumSamples;
			else
				loopStartSamples = (int32_t) tmpNumSamples + numSamples;
		}
	}

	if (numSamples <= 0) {
		printf("ERROR: Negative stream length value extends beyond the original stream length!\n");
		printf("ATTEMPTED VALUE: %d\n", numSamples);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (numSamples && loopEndSamples <= loopStartSamples) {
		printf("ERROR: Starting loop point must be smaller than ending loop point!\n");
		printf("LOOP_START: %d, LOOP_END: %d\n", loopStartSamples, loopEndSamples);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (enableLoop && loopStartSamples < 0) {
		printf("ERROR: Negative starting loop point value extends beyond the total stream length!\n");
		printf("ATTEMPTED VALUE: %d\n", loopStartSamples);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}

	return RETURN_SUCCESS;
}


void AudioOutData::calculate_aiff_file_size() {
	gFileSize = 0;

	gFileSize += FORM_HEADER_SIZE;
	gFileSize += COMM_HEADER_SIZE;

	if (enableLoop) {
		gFileSize += MARK_HEADER_SIZE;
		gFileSize += INST_HEADER_SIZE;
	}

	gFileSize += SSND_PRE_HEADER_SIZE;

	int32_t samplesPadded = numSamples;
	if (samplesPadded % SAMPLE_COUNT_PADDING)
		samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);

	gFileSize += samplesPadded * sizeof(sample_t);
}


void AudioOutData::write_form_header(FILE *streamFile) {
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

void AudioOutData::write_comm_header(FILE *streamFile) {
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
	tmp32BitValue = (uint32_t) numSamples;
	if (tmp32BitValue % SAMPLE_COUNT_PADDING)
		tmp32BitValue += SAMPLE_COUNT_PADDING - (tmp32BitValue % SAMPLE_COUNT_PADDING);
	tmp32BitValue = bswap_32(tmp32BitValue);
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Bit Depth (always 16) [0x0E]
	tmp16BitValue = bswap_16((uint16_t) 16);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Calculate sample rate stuffs, weirdly complicated to calculate manually
	uint16_t sampleRateMultiple = SAMPLE_RATE_MULTIPLE_CONSTANT;
	uint32_t sampleRateCurrent = (uint32_t) sampleRate;
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

void AudioOutData::write_mark_header(FILE *streamFile) {
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
	tmp32BitValue = bswap_32((uint32_t) (loopStartSamples));
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
	tmp32BitValue = bswap_32((uint32_t) (loopEndSamples));
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Marker Id ("end" is 3 characters) [0x1C]
	tmp8BitValue = 3;
	fwrite(&tmp8BitValue, 1, 1, streamFile);

	// "end" (Loop End Marker) [0x1D]
	fwrite(endMarker, 1, 3, streamFile);
}

// May not even be needed, but here just in case
void AudioOutData::write_inst_header(FILE *streamFile) {
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

void AudioOutData::write_ssnd_header(FILE *streamFile) {
	const char ssndHeader[] = "SSND";
	uint32_t tmp32BitValue;

	// SSND [0x00]
	fwrite(ssndHeader, 1, 4, streamFile);

	// SSND Size - 8 [0x04]
	uint32_t samplesPadded = (uint32_t) numSamples;
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

void AudioOutData::write_stream_headers(FILE **streamFiles) {
	for (int i = 0; i < numChannels; i++) {
		write_form_header(streamFiles[i]);
		write_comm_header(streamFiles[i]);

		if (enableLoop) {
			write_mark_header(streamFiles[i]);
			write_inst_header(streamFiles[i]);
		}

		write_ssnd_header(streamFiles[i]);
	}
}

void AudioOutData::write_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles) {
	uint32_t samplesPadded = (uint32_t) numSamples;
	if (samplesPadded % SAMPLE_COUNT_PADDING)
		samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);

	uint32_t bufferSize = MIN_PRINT_BUFFER_SIZE;
	if (MIN_PRINT_BUFFER_SIZE < SAMPLE_COUNT_PADDING)
		bufferSize = SAMPLE_COUNT_PADDING;

	sample_t *audioBuffer = new sample_t[bufferSize * (size_t) inFileProperties->channels];

	sample_t **printBuffer = new sample_t*[(size_t) inFileProperties->channels];
	for (int i = 0; i < inFileProperties->channels; i++)
		printBuffer[i] = new sample_t[bufferSize];

	for (uint32_t samplesProcessed = 0; samplesProcessed < samplesPadded; samplesProcessed += bufferSize) {
		render_vgmstream(audioBuffer, bufferSize, inFileProperties);

		// Not using inFileProperties->num_samples here is by intention, so padding is composed of zeros rather than unwanted audio data.
		for (uint64_t j = (((uint64_t) numSamples - samplesProcessed) * (uint64_t) inFileProperties->channels);
		 j < (uint64_t) bufferSize * (uint64_t) inFileProperties->channels; j++)
			audioBuffer[j] = 0;

		for (uint32_t j = 0; j < bufferSize * (uint32_t) inFileProperties->channels; j++)
			printBuffer[j % inFileProperties->channels][j / inFileProperties->channels] = (sample_t) bswap_16((uint16_t) audioBuffer[j]);

		for (int32_t j = 0; j < numChannels; j++)
			if (samplesProcessed + bufferSize > (uint32_t) samplesPadded)
				fwrite(printBuffer[j], sizeof(sample_t), samplesPadded - samplesProcessed, streamFiles[j]);
			else
				fwrite(printBuffer[j], sizeof(sample_t), bufferSize, streamFiles[j]);
	}

	for (int i = 0; i < inFileProperties->channels; i++)
		delete[] printBuffer[i];
	delete[] printBuffer;
	delete[] audioBuffer;
}

int AudioOutData::write_streams(VGMSTREAM *inFileProperties, string newFilename) {
	FILE **streamFiles = new FILE*[(size_t) numChannels];

	calculate_aiff_file_size();
	print_header_info();

	printf("Generating streamed file(s)...");
	fflush(stdout);
	for (int i = 0; i < numChannels; i++) {
		streamFiles[i] = fopen((newFilename + "_" + get_num_to_hex((uint8_t) i) + ".aiff").c_str(), "wb");
		if (!streamFiles[i]) {
			printf("...FAILED!\nERROR: Could not open %s for writing!\n", (newFilename + "_" + get_num_to_hex((uint8_t) i) + ".aiff").c_str());

			for (int j = i - 1; j >= 0; j--)
				fclose(streamFiles[j]);

			delete[] streamFiles;
			return RETURN_STREAM_CANNOT_CREATE_FILE;
		}
	}

	write_stream_headers(streamFiles);

	write_audio_data(inFileProperties, streamFiles);

	for (int i = 0; i < numChannels; i++)
		fclose(streamFiles[i]);

	delete[] streamFiles;

	printf("...DONE!\n");

	return RETURN_SUCCESS;
}

int generate_new_streams(VGMSTREAM *inFileProperties, string newFilename) {
	if (!inFileProperties)
		return 1;

	AudioOutData *audioData = new AudioOutData(inFileProperties);

	int ret = audioData->check_properties(newFilename);
	if (ret) {
		delete audioData;
		return ret;
	}
	ret = audioData->write_streams(inFileProperties, newFilename);

	delete audioData;
	return ret;
}
