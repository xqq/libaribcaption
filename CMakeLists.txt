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
project(aribcaption LANGUAGES C CXX)

add_library(aribcaption
    STATIC
        include/aligned_alloc.hpp
        include/aribcaption.h
        include/b24.hpp
        include/caption.hpp
        include/color.hpp
        include/context.hpp
        include/decoder.hpp
        include/drcs.hpp
        include/image.hpp
        src/base/aligned_alloc.cpp
        src/base/logger.hpp
        src/base/md5.c
        src/base/md5.h
        src/base/md5_helper.hpp
        src/base/result.hpp
        src/base/scoped_holder.hpp
        src/base/utf8_helper.hpp
        src/decoder/b24_codesets.hpp
        src/decoder/b24_colors.hpp
        src/decoder/b24_controlsets.hpp
        src/decoder/b24_conv_tables.hpp
        src/decoder/b24_drcs_conv.hpp
        src/decoder/b24_macros.hpp
        src/decoder/decoder.cpp
        src/decoder/decoder_impl.cpp
        src/decoder/decoder_impl.hpp
        src/renderer/bitmap.cpp
        src/renderer/bitmap.hpp
        src/renderer/font_provider.hpp
        src/renderer/font_provider_fontconfig.cpp
        src/renderer/font_provider_fontconfig.hpp
        src/renderer/text_renderer.hpp
        src/c_api.cpp
        src/context.cpp
)

target_compile_features(aribcaption PRIVATE cxx_std_17)

# Include directories
target_include_directories(aribcaption
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

add_subdirectory(test)
