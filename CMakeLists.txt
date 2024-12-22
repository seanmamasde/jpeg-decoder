cmake_minimum_required(VERSION 3.20)
project(jpeg-decoder)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17 -Ofast")

set(SOURCE_FILES    ../src/jpegDecoder.cpp
                    ../src/jpegDecoder.h
)
add_executable(jpegDecoder ${SOURCE_FILES})
