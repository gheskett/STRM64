cmake_minimum_required(VERSION 3.10)

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

if(UNIX)
	target_link_libraries(STRM64 "${PROJECT_SOURCE_DIR}/library/vgmstream/lib/libvgmstream.a")
else()
	target_link_libraries(STRM64 "${PROJECT_SOURCE_DIR}/library/vgmstream/windows/libvgmstream.lib")
	add_custom_command(TARGET STRM64 POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_directory
		"${PROJECT_SOURCE_DIR}/library/vgmstream/windows"
		$<TARGET_FILE_DIR:STRM64>)
endif()
