cmake_minimum_required(VERSION 3.16)

project(STRM64)

add_compile_options(-Wall -g -O3 -march=native -DNDEBUG)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(library/vgmstream/include)

set (CMAKE_CXX_STANDARD 17)

list(APPEND SRC_FILES
src/main.cpp
src/sequence.cpp
src/soundbank.cpp
src/stream.cpp
)

add_executable(STRM64
${SRC_FILES})

if(WIN32)
	target_link_libraries(STRM64 "${PROJECT_SOURCE_DIR}/library/vgmstream/windows/libvgmstream.lib")
	add_custom_command(TARGET STRM64 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${PROJECT_SOURCE_DIR}/library/vgmstream/windows/ext_libs"
		$<TARGET_FILE_DIR:STRM64>)
else()
	# FFmpeg
	find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
	find_library(AVCODEC avcodec)

	find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
	find_library(AVFORMAT avformat)

	find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
	find_library(AVUTIL avutil)

	find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h)
	find_library(SWRESAMPLE swresample)

	# Vorbis
	find_path(VORBIS_INCLUDE_DIR vorbis/codec.h)
	find_library(VORBIS vorbis)
	
	find_path(VORBISFILE_INCLUDE_DIR vorbis/vorbisfile.h)
	find_library(VORBISFILE vorbisfile)

	# MPG123
	find_path(MPG123_INCLUDE_DIR mpg123.h)
	find_library(MPG123 mpg123)

	# SPEEX
	find_path(SPEEX_INCLUDE_DIR speex/speex.h)
	find_library(SPEEX speex)

	target_include_directories(STRM64
		PRIVATE
		${AVCODEC_INCLUDE_DIR}
		${AVFORMAT_INCLUDE_DIR}
		${AVUTIL_INCLUDE_DIR}
		${SWRESAMPLE_INCLUDE_DIR}
		${VORBIS_INCLUDE_DIR}
		${VORBISFILE_INCLUDE_DIR}
		${MPG123_INCLUDE_DIR}
		${SPEEX_INCLUDE_DIR}
	)
	target_link_libraries(STRM64
		PRIVATE
		"${PROJECT_SOURCE_DIR}/library/vgmstream/lib/libvgmstream.a"
		${AVCODEC}
		${AVFORMAT}
		${AVUTIL}
		${SWRESAMPLE}
		${VORBIS}
		${VORBISFILE}
		${MPG123}
		${SPEEX}
	)
endif()
