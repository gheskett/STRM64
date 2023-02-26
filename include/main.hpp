#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define WINDOWS
#endif

enum ReturnCodes {
    RETURN_SUCCESS,
    RETURN_NOT_ENOUGH_ARGS,

    RETURN_INVALID_ARGS,
    RETURN_CANNOT_FIND_INPUT_FILE,
    RETURN_INVALID_INPUT_FILE,
    RETURN_NOT_ENOUGH_CHANNELS,
    RETURN_TOO_MANY_CHANNELS,

    RETURN_STREAM_NULL_VGMSTREAM,
    RETURN_STREAM_FAILED_RESAMPLING,
    RETURN_STREAM_INVALID_PARAMETERS,
    RETURN_STREAM_CANNOT_CREATE_FILE,

    RETURN_SEQUENCE_NO_CHANNELS,
    RETURN_SEQUENCE_CANNOT_CREATE_FILE,

    RETURN_SOUNDBANK_NO_CHANNELS,
    RETURN_SOUNDBANK_CANNOT_CREATE_FILE
};

#define NUM_CHANNELS_MAX (sizeof(uint16_t) * 8)

bool is_mono();
void set_filename_duplicate(std::string duplicate);
std::string get_filename_duplicate();

void print_param_warning(std::string param);
void print_header_info(bool isStreamGeneration, uint32_t fileSize);

#endif
