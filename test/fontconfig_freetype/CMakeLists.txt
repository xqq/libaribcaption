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

if(NOT ARIBCC_USE_FREETYPE)
    find_package(Freetype)
endif()

if(NOT ARIBCC_USE_FONTCONFIG)
    find_package(Fontconfig)
endif()

add_executable(test_fontconfig_freetype
    EXCLUDE_FROM_ALL
        test.cpp
)

target_compile_features(test_fontconfig_freetype
    PRIVATE
        cxx_std_17
)

target_include_directories(test_fontconfig_freetype
    PRIVATE
        ../../include
        ../../src
        ../png_writer/include
        ${FREETYPE_INCLUDE_DIRS}
        ${Fontconfig_INCLUDE_DIRS}
)

target_link_libraries(test_fontconfig_freetype
    PRIVATE
        aribcaption
        png_writer
        ${FREETYPE_LIBRARIES}
        ${Fontconfig_LIBRARIES}
)

set_target_properties(test_fontconfig_freetype
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
)
