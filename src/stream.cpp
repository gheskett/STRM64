#include <stdio.h>
#include <stdlib.h>

#include <vgmstream.h>

#include "main.hpp"
#include "stream.hpp"
#include "bswp.hpp"

using namespace std;


// Override parameters
static bool ovrdForceVgmstream = false;
static int64_t ovrdSampleRate = -1;
static int64_t ovrdEnableLoop = -1;
static int64_t ovrdLoopStartSamples = -1;
static int64_t ovrdLoopEndSamples = -1;
static int64_t ovrdLoopStartMicro = -1;
static int64_t ovrdLoopEndMicro = -1;


void set_force_vgmstream() {
	ovrdForceVgmstream = true;
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
	if (samples < 0 && samples >= 0x100000000) {
		print_param_warning("loop start (samples)");
		return;
	}

	ovrdLoopStartSamples = (uint32_t) samples;
	ovrdLoopStartMicro = -1;
}

void set_loop_end_samples(int64_t samples) {
	if (samples < 0 && samples >= 0x100000000) {
		print_param_warning("loop start (samples)");
		return;
	}

	ovrdLoopEndSamples = (uint32_t) samples;
	ovrdLoopEndMicro = -1;
}

void set_loop_start_microseconds(int64_t microseconds) {
	if (microseconds < 0) {
		print_param_warning("loop start (microseconds)");
		return;
	}

	ovrdLoopStartMicro = microseconds;
	ovrdLoopStartSamples = -1;
}

void set_loop_end_microseconds(int64_t microseconds) {
	if (microseconds < 0) {
		print_param_warning("loop end (microseconds)");
		return;
	}

	ovrdLoopEndMicro = microseconds;
	ovrdLoopEndSamples = -1;
}
