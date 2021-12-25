#
# Copyright (C) 2021 magicxqq <xqq@xqq.im>. All rights reserved.
#
# This file is part of libaribcaption.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

cmake_minimum_required(VERSION 3.1)

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

add_executable(test_ffmpeg
    EXCLUDE_FROM_ALL
        stopwatch.hpp
        test.cpp
)

target_compile_features(test_ffmpeg
    PRIVATE
        cxx_std_17
)

target_include_directories(test_ffmpeg
    PRIVATE
        ../../include
        ../../src
        ../png_writer/include
        ${AVCODEC_INCLUDE_DIR}
        ${AVFORMAT_INCLUDE_DIR}
)

target_link_libraries(test_ffmpeg
    PRIVATE
        aribcaption
        png_writer
        ${AVCODEC_LIBRARY}
        ${AVFORMAT_LIBRARY}
)

set_target_properties(test_ffmpeg
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
