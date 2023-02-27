#include <stdio.h>
#include <stdlib.h>

#include "main.hpp"
#include "stream.hpp"
#include "bswp.hpp"

using namespace std;

#define SAMPLE_RATE_MULTIPLE_CONSTANT 0x400E
#define FORM_HEADER_SIZE 0x0C
#define COMM_HEADER_SIZE 0x1A
#define MARK_HEADER_SIZE 0x20
#define INST_HEADER_SIZE 0x1C
#define SSND_PRE_HEADER_SIZE 0x10

#define MICROSECOND_DECIMALS 6
#define TIME_SECOND          1000000LL // microseconds
#define TIME_MINUTE          (TIME_SECOND * 60)
#define TIME_HOUR            (TIME_MINUTE * 60)
#define TIME_DAY             (TIME_HOUR * 24)


// Override parameters
static bool ovrdResample = false;
static int64_t ovrdSampleRate = -1;
static int64_t ovrdEnableLoop = -1;
static int64_t ovrdLoopStartSamples = INT64_MAX;
static int64_t ovrdLoopEndSamples = INT64_MAX;
static int64_t ovrdLoopStartMicro = INT64_MAX;
static int64_t ovrdLoopEndMicro = INT64_MAX;

static uint32_t gFileSize = 0;


AudioOutData::AudioOutData(VGMSTREAM *inFileProperties) {
	resample = ovrdResample;
	vgmstreamLoopPointMismatch = false; // Only used with data resampling to enforce a manual stream seek
	sampleRate = inFileProperties->sample_rate;
	enableLoop = inFileProperties->loop_flag;
	numSamples = inFileProperties->num_samples;
	numChannels = inFileProperties->channels;
	
	if (enableLoop) {
		loopStartSamples = inFileProperties->loop_start_sample;
		loopEndSamples = inFileProperties->loop_end_sample;
		if (loopEndSamples < numSamples)
			numSamples = loopEndSamples;
	}
	else {
		loopStartSamples = 0;
		loopEndSamples = numSamples;
	}

	resampledSampleRate = sampleRate;
	resampledLoopStartSamples = loopStartSamples;
	resampledLoopEndSamples = loopEndSamples;
	resampledNumSamples = numSamples;

	resampleContext = NULL;
}
AudioOutData::~AudioOutData() {

}


string AudioOutData::samples_to_us_print(uint64_t sampleOffset) {
	uint64_t convTime = (uint64_t) (((long double) sampleOffset / (long double) resampledSampleRate) * 1000000.0 + 0.5);

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

// Calculates time duration to use in microseconds (string to int64_t)
int64_t timestamp_to_us(string timestamp) {
	int64_t days = 0;
	int64_t hours = 0;
	int64_t minutes = 0;
	int64_t seconds = 0;
	int64_t microseconds = 0;

	if (timestamp.find("::") != string::npos || timestamp.find(":.") != string::npos)
		return INT64_MIN;

	size_t period = timestamp.find(".");
	if (period != string::npos) {
		for (size_t i = timestamp.length(); i < period + 1 + MICROSECOND_DECIMALS; ++i)
			timestamp += "0"; // Tack on zeros to make the decimal an artificially large number. Does not respect invalid character inputs.

		microseconds = strtoll(timestamp.substr(period + 1, MICROSECOND_DECIMALS).c_str(), NULL, 10);
		timestamp = timestamp.substr(0, period);
	}

	int colonCount = 0;
	size_t index = 0;
	while (true) {
		index = timestamp.find(":", index);
		if (index == string::npos)
			break;

		colonCount++;
		index += 1;
	}
	if (colonCount > 3)
		return INT64_MIN;

	index = 0;
	if (colonCount == 3) {
		size_t indLast = index;
		index = timestamp.find(":", index) + 1;
		days = strtoll(timestamp.substr(indLast, index - indLast - 1).c_str(), NULL, 10);
		days = days * TIME_DAY;
		colonCount--;
	}
	if (colonCount == 2) {
		size_t indLast = index;
		index = timestamp.find(":", index) + 1;
		hours = strtoll(timestamp.substr(indLast, index - indLast - 1).c_str(), NULL, 10);
		hours = hours * TIME_HOUR;
		colonCount--;
	}
	if (colonCount == 1) {
		size_t indLast = index;
		index = timestamp.find(":", index) + 1;
		minutes = strtoll(timestamp.substr(indLast, index - indLast - 1).c_str(), NULL, 10);
		minutes = minutes * TIME_MINUTE;
		colonCount--;
	}
	seconds = strtoll(timestamp.substr(index).c_str(), NULL, 10);
	seconds *= TIME_SECOND;

	return days + hours + minutes + seconds + microseconds;
}

void AudioOutData::print_header_info() {
	printf("\n");

	if (numChannels == 1)
		printf("    File Size of AIFF: %u bytes\n", gFileSize * (uint32_t) numChannels);
	else
		printf("    Cumulative File Size of AIFFs: %u bytes\n", gFileSize * (uint32_t) numChannels);

	printf("    Sample Rate: %d Hz", resampledSampleRate);
	if (ovrdSampleRate <= 0 && resampledSampleRate > 32000)
		printf(" (Downsampling recommended! [-R 32000])");
	printf("\n");

	printf("    Is Looped: ");
	if (enableLoop) {
		printf("true\n");

		printf("    Starting Loop Point: %d Samples (Time: %s)\n", resampledLoopStartSamples,
			samples_to_us_print(resampledLoopStartSamples).c_str());

		printf("    Ending Loop Point: %d Samples (Time: %s)\n", resampledLoopEndSamples,
			samples_to_us_print(resampledLoopEndSamples).c_str());
	}
	else {
		printf("false\n");

		int32_t samplesPadded = resampledNumSamples;
		if (samplesPadded % SAMPLE_COUNT_PADDING)
			samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);
		printf("    End of Stream: %d Samples (Time: %s)\n", samplesPadded,
			samples_to_us_print(samplesPadded).c_str());
	}

	printf("    Number of Channels: %d", numChannels);
	if (!is_mono()) {
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

	ovrdResample = false;
	ovrdSampleRate = sampleRate;
}

void set_resample_rate(int64_t resampleRate) {
	if (resampleRate <= 0) {
		print_param_warning("resample rate");
		return;
	}

	ovrdResample = true;
	ovrdSampleRate = resampleRate;
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
	ovrdLoopStartMicro = INT64_MAX;
}

void set_loop_end_samples(int64_t samples) {
	if (samples >= 0x100000000) {
		print_param_warning("loop end (samples)");
		return;
	}

	ovrdLoopEndSamples = samples;
	ovrdLoopEndMicro = INT64_MAX;
}

void set_loop_start_microseconds(int64_t microseconds) {
	ovrdEnableLoop = 1;
	ovrdLoopStartMicro = microseconds;
	ovrdLoopStartSamples = INT64_MAX;
}

void set_loop_end_microseconds(int64_t microseconds) {
	ovrdLoopEndMicro = microseconds;
	ovrdLoopEndSamples = INT64_MAX;
}

char get_num_to_hex(uint8_t num) {
	char ret = (num & 0x0F) + 48;
	if (ret >= 58)
		ret += 7;

	return ret;
}

int64_t us_to_samples(int64_t sampleRate, int64_t timeOffset) {
	return (int64_t) ((((long double) timeOffset / 1000000.0) * (long double) sampleRate) + 0.5);
}

int AudioOutData::check_properties(VGMSTREAM *inFileProperties, string newFilename) {
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
		resampledSampleRate = (int32_t) ovrdSampleRate;
		if (!resample)
			sampleRate = resampledSampleRate;
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
	if (ovrdLoopEndSamples != INT64_MAX) {
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
	else if (ovrdLoopEndMicro != INT64_MAX) {
		int64_t tmpNumSamples = us_to_samples(sampleRate, ovrdLoopEndMicro);
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
		if (ovrdLoopStartSamples != INT64_MAX) {
			if (ovrdLoopStartSamples >= 0)
				loopStartSamples = (int32_t) ovrdLoopStartSamples;
			else
				loopStartSamples = (int32_t) ovrdLoopStartSamples + numSamples;
		}
		// Overridden start loop point, represented in microseconds
		else if (ovrdLoopStartMicro != INT64_MAX) {
			int64_t tmpNumSamples = us_to_samples(sampleRate, ovrdLoopStartMicro);
			if (tmpNumSamples >= 0)
				loopStartSamples = (int32_t) tmpNumSamples;
			else
				loopStartSamples = (int32_t) tmpNumSamples + numSamples;
		}
	}

	// Calculate new metadata for use if resampling audio
	if (resample) {
		double ratio = (double) resampledSampleRate / (double) sampleRate;
		resampledNumSamples = (numSamples * ratio) + 0.95; // round up most of the time, but not always
		resampledLoopEndSamples = (loopEndSamples * ratio) + 0.95; // round up most of the time, but not always

		if (enableLoop) {
			resampledLoopStartSamples = resampledLoopEndSamples - (int32_t) (((double) (loopEndSamples - loopStartSamples) * ratio) + 0.5); // calculate based on difference rather than absolute
			if (resampledLoopStartSamples < 0)
				resampledLoopStartSamples = 0;

			if (resampledLoopEndSamples <= resampledLoopStartSamples) {
				resampledLoopEndSamples = resampledLoopStartSamples + 1;
				resampledNumSamples = resampledLoopEndSamples; // numSamples and loopEndSamples are presumed matching by this point, so no additional check needed
			}

			if (
				!inFileProperties->loop_flag ||
				loopStartSamples != inFileProperties->loop_start_sample ||
				loopEndSamples != inFileProperties->loop_end_sample
			) {
				vgmstreamLoopPointMismatch = true; // Force manual seeking in the stream once the loop end is reached
			}
		}
	} else {
		resampledNumSamples = numSamples;
		resampledLoopEndSamples = loopEndSamples;
		resampledLoopStartSamples = loopStartSamples;
		vgmstreamLoopPointMismatch = false;
	}

	if (numSamples <= 0) {
		printf("ERROR: Negative stream length value extends beyond the original stream length!\n");
		printf("ATTEMPTED VALUE: %d\n", numSamples);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (resampledNumSamples <= 0) {
		printf("ERROR: Output audio file size is too large after resampling!\n");
		printf("ATTEMPTED VALUE: %d\n", resampledNumSamples);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (enableLoop && resampledLoopEndSamples <= resampledLoopStartSamples) {
		printf("ERROR: Starting loop point must be smaller than ending loop point!\n");
		printf("LOOP_START: %d, LOOP_END: %d\n", resampledLoopStartSamples, resampledLoopEndSamples);
		return RETURN_STREAM_INVALID_PARAMETERS;
	}
	if (enableLoop && resampledLoopStartSamples < 0) {
		printf("ERROR: Negative starting loop point value extends beyond the total stream length!\n");
		printf("ATTEMPTED VALUE: %d\n", resampledLoopStartSamples);
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

	int32_t samplesPadded = resampledNumSamples;
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
	tmp32BitValue = (uint32_t) resampledNumSamples;
	if (tmp32BitValue % SAMPLE_COUNT_PADDING)
		tmp32BitValue += SAMPLE_COUNT_PADDING - (tmp32BitValue % SAMPLE_COUNT_PADDING);
	tmp32BitValue = bswap_32(tmp32BitValue);
	fwrite(&tmp32BitValue, 4, 1, streamFile);

	// Bit Depth (always 16) [0x0E]
	tmp16BitValue = bswap_16((uint16_t) 16);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Calculate sample rate stuffs manually; uses an 80-bit extended float value in the AIFF header
	uint16_t sampleRateMultiple = SAMPLE_RATE_MULTIPLE_CONSTANT;
	uint32_t sampleRateCurrent = (uint32_t) resampledSampleRate;
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

	// NOTE: The endianness checks here are funky, since this "weird structure" has now been recognized to essentially be an 80-bit float.
	// All 10 bytes of the data here probably need to be swapped as one, if there's ever any reason to implement little-endian exports.

	// Sample Rate Exponential Multiple [0x10]
	tmp16BitValue = bswap_16(sampleRateMultiple);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Modified Sample Rate [0x12]
	tmp16BitValue = bswap_16((uint16_t) sampleRateCurrent);
	fwrite(&tmp16BitValue, 2, 1, streamFile);

	// Modified Sample Rate Remainder (6 bytes) [0x14]
	fwrite(&sampleRateRemainder, 1, 6, streamFile); // FIXME: Missing endianness check. Will break if exporting as little-endian.
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
	tmp32BitValue = bswap_32((uint32_t) (resampledLoopStartSamples));
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
	tmp32BitValue = bswap_32((uint32_t) (resampledLoopEndSamples));
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
	uint32_t samplesPadded = (uint32_t) resampledNumSamples;
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

void AudioOutData::cleanup_resample_context() {
	if (resampleContext != NULL)
		swr_free(&resampleContext);
}

int AudioOutData::init_audio_resampling(VGMSTREAM *inFileProperties, int inputBufferSize) {
	uint64_t channelLayout = (1ULL << numChannels) - 1;

	// Works with 18 channels maximum probably, TODO: research whether different bitflags affect how a thing is resampled if it matters for some reason
	resampleContext = swr_alloc_set_opts(NULL, (int64_t) channelLayout, AV_SAMPLE_FMT_S16, resampledSampleRate,
		(int64_t) channelLayout, AV_SAMPLE_FMT_S16, sampleRate, 0, NULL);
	
	if (resampleContext == NULL) {
		printf("...FAILED!\nERROR: Could not allocate resampling context!\n");
		swr_free(&resampleContext);
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	if (swr_init(resampleContext) != 0) {
		printf("...FAILED!\nERROR: Could not initialize resampling context!\n");
		swr_free(&resampleContext);
		return RETURN_STREAM_FAILED_RESAMPLING;
	}
	
	if (swr_is_initialized(resampleContext) == 0) {
		printf("...FAILED!\nERROR: Resample context has not been properly initialized!\n");
		cleanup_resample_context();
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	return RETURN_SUCCESS;
}

int AudioOutData::resample_audio_data(const sample_t *inputAudioBuffer, sample_t *audioOutBuffer, sample_t *printBuffer,
 FILE **streamFiles, int inputBufferSize, int outputBufferSamples, uint32_t samplesPadded, uint32_t *totalSamplesProcessed) {
	if (swr_is_initialized(resampleContext) == 0) {
		printf("...FAILED!\nERROR: Resample context has not been properly initialized!\n");
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	int result = swr_convert(resampleContext, (uint8_t**) &audioOutBuffer, outputBufferSamples, (const uint8_t**) &inputAudioBuffer, inputBufferSize);

	if (result < 0) {
		printf("...FAILED!\nERROR: Unable to resample given input buffer!\n");
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	if (result > outputBufferSamples) {
		printf("...FAILED!\nERROR: Memory overflow!\n");
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	outputBufferSamples = result;

	// Eliminate any unwanted data for padding
	int64_t samplesToPadStart = ((int64_t) resampledNumSamples - *totalSamplesProcessed) * (int64_t) numChannels;
	if (samplesToPadStart < 0)
		samplesToPadStart = 0;
	for (int64_t j = samplesToPadStart; j < (int64_t) outputBufferSamples * (int64_t) numChannels; j++)
		audioOutBuffer[j] = 0;

	for (int32_t i = 0; i < numChannels; i++) {
		for (uint64_t j = 0; j < (uint64_t) outputBufferSamples; j++)
			printBuffer[j] = (sample_t) bswap_16((uint16_t) audioOutBuffer[j * numChannels + i]);

		if (*totalSamplesProcessed + outputBufferSamples > (uint32_t) samplesPadded)
			fwrite(printBuffer, sizeof(sample_t), samplesPadded - *totalSamplesProcessed, streamFiles[i]);
		else
			fwrite(printBuffer, sizeof(sample_t), outputBufferSamples, streamFiles[i]);
	}

	*totalSamplesProcessed += outputBufferSamples;

	return RETURN_SUCCESS;
}

int AudioOutData::write_resampled_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles) {
	uint32_t resampledSamplesPadded = (uint32_t) resampledNumSamples;
	if (resampledSamplesPadded % SAMPLE_COUNT_PADDING)
		resampledSamplesPadded += SAMPLE_COUNT_PADDING - (resampledSamplesPadded % SAMPLE_COUNT_PADDING);

	uint32_t bufferSize = MIN_PRINT_BUFFER_SIZE;
	if (MIN_PRINT_BUFFER_SIZE < SAMPLE_COUNT_PADDING)
		bufferSize = SAMPLE_COUNT_PADDING;

	sample_t *audioBuffer = new (nothrow) sample_t[bufferSize * (size_t) numChannels];
	if (audioBuffer == nullptr) {
		printf("...FAILED!\nERROR: Out of memory!\n");
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	int retCode = init_audio_resampling(inFileProperties, bufferSize);
	if (retCode != RETURN_SUCCESS) {
		delete[] audioBuffer;
		return retCode;
	}

	int outputBufferSamples = swr_get_out_samples(resampleContext, bufferSize);

	sample_t *audioOutBuffer = new (nothrow) sample_t[(size_t) outputBufferSamples * (size_t) numChannels];
	if (audioOutBuffer == nullptr) {
		printf("...FAILED!\nERROR: Out of memory!\n");
		cleanup_resample_context();
		delete[] audioBuffer;
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	sample_t *printBuffer = new (nothrow) sample_t[(size_t) outputBufferSamples];
	if (printBuffer == nullptr) {
		printf("...FAILED!\nERROR: Out of memory!\n");
		cleanup_resample_context();
		delete[] audioBuffer;
		delete[] audioOutBuffer;
		return RETURN_STREAM_FAILED_RESAMPLING;
	}

	uint32_t samplesProcessed = 0;
	uint32_t resampledSamplesProcessed = 0;

	while (true) {
		int64_t samplesRemaining = numSamples - (int64_t) samplesProcessed;
		render_vgmstream(audioBuffer, bufferSize, inFileProperties);

		if (enableLoop && vgmstreamLoopPointMismatch && samplesRemaining < bufferSize) {
			int64_t loopSampleDifference = loopEndSamples - loopStartSamples;

			while (samplesRemaining < bufferSize) {
				sample_t *startOffset = &(audioBuffer[((int64_t) bufferSize - samplesRemaining) * numChannels]);
				seek_vgmstream(inFileProperties, loopStartSamples);
				render_vgmstream(startOffset, samplesRemaining, inFileProperties);
				samplesProcessed = loopStartSamples + (bufferSize - samplesRemaining);
				samplesRemaining += loopSampleDifference;
			}
		} else {
			samplesProcessed += bufferSize;
		}

		int retCode = resample_audio_data((const sample_t*) audioBuffer, audioOutBuffer, printBuffer, streamFiles,
		 (int) bufferSize, outputBufferSamples, resampledSamplesPadded, &resampledSamplesProcessed);

		if (retCode != RETURN_SUCCESS) {
			cleanup_resample_context();
			delete[] audioBuffer;
			delete[] audioOutBuffer;
			delete[] printBuffer;

			return retCode;
		}

		if (resampledSamplesProcessed >= resampledSamplesPadded)
			break;
	}

	cleanup_resample_context();
	delete[] printBuffer;
	delete[] audioOutBuffer;
	delete[] audioBuffer;

	return RETURN_SUCCESS;
}

void AudioOutData::write_audio_data(VGMSTREAM *inFileProperties, FILE **streamFiles) {
	uint32_t samplesPadded = (uint32_t) numSamples;
	if (samplesPadded % SAMPLE_COUNT_PADDING)
		samplesPadded += SAMPLE_COUNT_PADDING - (samplesPadded % SAMPLE_COUNT_PADDING);

	uint32_t bufferSize = MIN_PRINT_BUFFER_SIZE;
	if (MIN_PRINT_BUFFER_SIZE < SAMPLE_COUNT_PADDING)
		bufferSize = SAMPLE_COUNT_PADDING;

	sample_t *audioBuffer = new sample_t[bufferSize * (size_t) numChannels];

	sample_t **printBuffer = new sample_t*[(size_t) numChannels];
	for (int i = 0; i < numChannels; i++)
		printBuffer[i] = new sample_t[bufferSize];

	for (uint32_t samplesProcessed = 0; samplesProcessed < samplesPadded; samplesProcessed += bufferSize) {
		render_vgmstream(audioBuffer, bufferSize, inFileProperties);

		// Not using inFileProperties->num_samples here is by intention, so padding is composed of zeros rather than unwanted audio data.
		int64_t samplesToPadStart = ((int64_t) numSamples - samplesProcessed) * (int64_t) numChannels;
		if (samplesToPadStart < 0)
			samplesToPadStart = 0;
		for (int64_t j = samplesToPadStart; j < (int64_t) bufferSize * (int64_t) numChannels; j++)
			audioBuffer[j] = 0;

		for (uint32_t j = 0; j < bufferSize * (uint32_t) numChannels; j++)
			printBuffer[j % numChannels][j / numChannels] = (sample_t) bswap_16((uint16_t) audioBuffer[j]);

		for (int32_t j = 0; j < numChannels; j++)
			if (samplesProcessed + bufferSize > (uint32_t) samplesPadded)
				fwrite(printBuffer[j], sizeof(sample_t), samplesPadded - samplesProcessed, streamFiles[j]);
			else
				fwrite(printBuffer[j], sizeof(sample_t), bufferSize, streamFiles[j]);
	}

	for (int i = 0; i < numChannels; i++)
		delete[] printBuffer[i];
	delete[] printBuffer;
	delete[] audioBuffer;
}

int AudioOutData::write_streams(VGMSTREAM *inFileProperties, string newFilename, string oldFilename) {
	FILE **streamFiles = new FILE*[(size_t) numChannels];

	calculate_aiff_file_size();
	print_header_info();

	printf("Generating streamed file(s)...");
	fflush(stdout);
	for (int i = 0; i < numChannels; i++) {
		string suffix = "";
		if (numChannels == 2 && !is_mono()) {
			if (i == 0) {
				suffix += "_L";
			} else {
				suffix += "_R";
			}
		} else if (numChannels != 1) {
			suffix += '_' + get_num_to_hex((uint8_t) i);
		}

		string finalFilename = newFilename + suffix + ".aiff";

		// Only check for duplicates here; if exporting only the soundbank but not the streams, the soundbank should ignore duplicate filenames.
		// This is necessary as to not overwrite the source file being read by vgmstream, without having to terminate the entire application.
		// Even if we were to just rely on fopen failing, this doesn't always work as expected.
		if (finalFilename.compare(oldFilename) == 0) {
			set_filename_duplicate(newFilename + suffix);
			finalFilename = newFilename + suffix + "_0" + ".aiff";
		}

		streamFiles[i] = fopen((finalFilename).c_str(), "wb");
		if (!streamFiles[i]) {
			printf("...FAILED!\nERROR: Could not open %s for writing!\n", (finalFilename).c_str());

			for (int j = i - 1; j >= 0; j--)
				fclose(streamFiles[j]);

			delete[] streamFiles;
			return RETURN_STREAM_CANNOT_CREATE_FILE;
		}
	}

	write_stream_headers(streamFiles);

	int retCode = RETURN_SUCCESS;
	if (resample)
		retCode = write_resampled_audio_data(inFileProperties, streamFiles);
	else
		write_audio_data(inFileProperties, streamFiles);

	for (int i = 0; i < numChannels; i++)
		fclose(streamFiles[i]);

	delete[] streamFiles;

	if (retCode != RETURN_SUCCESS) {
		return retCode;
	}

	printf("...DONE!\n");

	return RETURN_SUCCESS;
}

int generate_new_streams(VGMSTREAM *inFileProperties, string newFilename, string oldFilename) {
	if (!inFileProperties)
		return 1;

	AudioOutData *audioData = new AudioOutData(inFileProperties);

	int ret = audioData->check_properties(inFileProperties, newFilename);
	if (ret) {
		delete audioData;
		return ret;
	}
	ret = audioData->write_streams(inFileProperties, newFilename, oldFilename);

	delete audioData;
	return ret;
}
