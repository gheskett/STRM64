cmake_minimum_required(VERSION 3.10)

project(STRM64)

add_compile_options(-Wall -g -O3 -march=native -DNDEBUG)

include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/library/vgmstream)

add_subdirectory("library/vgmstream")

set (CMAKE_CXX_STANDARD 17)

list(APPEND SRC_FILES
src/main.cpp
src/sequence.cpp
src/soundbank.cpp
src/stream.cpp
)

add_executable(STRM64
${SRC_FILES})
