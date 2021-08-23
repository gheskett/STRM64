#ifndef MAIN_HPP
#define MAIN_HPP

#include <string>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
#define WINDOWS
#endif

#define NUM_CHANNELS_MAX (sizeof(uint16_t) * 8)

void print_param_warning(std::string param);
void print_header_info(bool isStreamGeneration, uint32_t fileSize);

#endif
