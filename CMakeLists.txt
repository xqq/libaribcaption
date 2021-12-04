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
project(libaribcaption VERSION 0.1.0 LANGUAGES C CXX)

# Release build by default
if(NOT CMAKE_BUILD_TYPE)
    message("Build type will be Release. Set CMAKE_BUILD_TYPE to indicate other types.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default to Release build" FORCE)
endif()

if(WIN32)
    # Windows, use DirectWrite
    option(LIBARIBCAPTION_USE_DIRECTWRITE "Enable DirectWrite text rendering backend" ON)
elseif(APPLE)
    # macOS or iOS, use CoreText by default
    option(LIBARIBCAPTION_USE_CORETEXT "Enable CoreText text rendering backend" ON)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    # Android, FreeType required
    set(LIBARIBCAPTION_IS_ANDROID TRUE CACHE BOOL "Specify target OS is Android")
    option(LIBARIBCAPTION_USE_FREETYPE "Enable FreeType text rendering backend" ON)
else()
    # Linux or other Unix systems, requires Fontconfig & FreeType
    option(LIBARIBCAPTION_USE_FONTCONFIG "Enable Fontconfig font provider" ON)
    option(LIBARIBCAPTION_USE_FREETYPE "Enable FreeType text rendering backend" ON)
endif()


if(LIBARIBCAPTION_USE_CORETEXT)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    find_library(CORETEXT_FRAMEWORK CoreText)
endif()

if(LIBARIBCAPTION_USE_FONTCONFIG)
    find_package(Fontconfig REQUIRED)
endif()

if(LIBARIBCAPTION_USE_FREETYPE)
    find_package(Freetype REQUIRED)
endif()

# Generate config header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
)

### Source files
add_library(aribcaption
        include/aribcaption/aligned_alloc.hpp
        include/aribcaption/aribcaption.h
        include/aribcaption/aribcaption.hpp
        include/aribcaption/b24.hpp
        include/aribcaption/caption.hpp
        include/aribcaption/color.hpp
        include/aribcaption/context.hpp
        include/aribcaption/decoder.hpp
        include/aribcaption/drcs.hpp
        include/aribcaption/image.hpp
        include/aribcaption/renderer.hpp
        src/base/aligned_alloc.cpp
        src/base/always_inline.hpp
        src/base/cfstr_helper.hpp
        src/base/language_code.hpp
        src/base/logger.hpp
        src/base/md5.c
        src/base/md5.h
        src/base/md5_helper.hpp
        src/base/result.hpp
        src/base/scoped_cfref.hpp
        src/base/scoped_com_initializer.hpp
        src/base/scoped_holder.hpp
        src/base/utf_helper.hpp
        src/base/wchar_helper.hpp
        src/decoder/b24_codesets.hpp
        src/decoder/b24_colors.hpp
        src/decoder/b24_controlsets.hpp
        src/decoder/b24_conv_tables.hpp
        src/decoder/b24_drcs_conv.hpp
        src/decoder/b24_macros.hpp
        src/decoder/decoder.cpp
        src/decoder/decoder_impl.cpp
        src/decoder/decoder_impl.hpp
        src/renderer/alpha_blend.hpp
        src/renderer/bitmap.cpp
        src/renderer/bitmap.hpp
        src/renderer/canvas.cpp
        src/renderer/canvas.hpp
        src/renderer/drcs_renderer.cpp
        src/renderer/drcs_renderer.hpp
        src/renderer/font_provider.cpp
        src/renderer/font_provider.hpp
        $<$<BOOL:${LIBARIBCAPTION_USE_CORETEXT}>:src/renderer/font_provider_coretext.cpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_CORETEXT}>:src/renderer/font_provider_coretext.hpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_DIRECTWRITE}>:src/renderer/font_provider_directwrite.cpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_DIRECTWRITE}>:src/renderer/font_provider_directwrite.hpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_FONTCONFIG}>:src/renderer/font_provider_fontconfig.cpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_FONTCONFIG}>:src/renderer/font_provider_fontconfig.hpp>
        src/renderer/rect.hpp
        src/renderer/region_renderer.cpp
        src/renderer/region_renderer.hpp
        src/renderer/renderer.cpp
        src/renderer/renderer_impl.cpp
        src/renderer/renderer_impl.hpp
        src/renderer/text_renderer.cpp
        src/renderer/text_renderer.hpp
        $<$<BOOL:${LIBARIBCAPTION_USE_CORETEXT}>:src/renderer/text_renderer_coretext.cpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_CORETEXT}>:src/renderer/text_renderer_coretext.hpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_DIRECTWRITE}>:src/renderer/text_renderer_directwrite.cpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_DIRECTWRITE}>:src/renderer/text_renderer_directwrite.hpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_FREETYPE}>:src/renderer/text_renderer_freetype.cpp>
        $<$<BOOL:${LIBARIBCAPTION_USE_FREETYPE}>:src/renderer/text_renderer_freetype.hpp>
        src/c_api.cpp
        src/context.cpp
)

add_library(aribcaption::aribcaption ALIAS aribcaption)

target_compile_features(aribcaption
    PRIVATE
        cxx_std_17
)

# Enable /utf-8 for MSVC
target_compile_options(aribcaption
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

### Includes
target_include_directories(aribcaption
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        $<$<BOOL:${LIBARIBCAPTION_USE_FONTCONFIG}>:${Fontconfig_INCLUDE_DIRS}>
        $<$<BOOL:${LIBARIBCAPTION_USE_FREETYPE}>:${FREETYPE_INCLUDE_DIRS}>
)

### Linking
target_link_libraries(aribcaption
    PRIVATE
        $<$<BOOL:${LIBARIBCAPTION_USE_CORETEXT}>:${COREFOUNDATION_FRAMEWORK}>
        $<$<BOOL:${LIBARIBCAPTION_USE_CORETEXT}>:${COREGRAPHICS_FRAMEWORK}>
        $<$<BOOL:${LIBARIBCAPTION_USE_CORETEXT}>:${CORETEXT_FRAMEWORK}>
        $<$<BOOL:${LIBARIBCAPTION_USE_DIRECTWRITE}>:d2d1>
        $<$<BOOL:${LIBARIBCAPTION_USE_DIRECTWRITE}>:dwrite>
        $<$<BOOL:${LIBARIBCAPTION_USE_DIRECTWRITE}>:windowscodecs>
)

# vcpkg uses optimized/debug keyword in XXXXX_LIBRARIES variables
# which is incompatible with generator expressions
if(LIBARIBCAPTION_USE_FONTCONFIG)
    target_link_libraries(aribcaption
        PRIVATE
            ${Fontconfig_LIBRARIES}
    )
endif()

if(LIBARIBCAPTION_USE_FREETYPE)
    target_link_libraries(aribcaption
        PRIVATE
            ${FREETYPE_LIBRARIES}
    )
endif()


### Installing
include(GNUInstallDirs)

# Install public headers
install(
    DIRECTORY
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/aribcaption
    PATTERN "config.h.in" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE
)

# Install generated config header
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/config.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/aribcaption
)

# Install
install(
    TARGETS aribcaption
        EXPORT aribcaption-targets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        FRAMEWORK DESTINATION Library/Frameworks
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Export targets to cmake script
install(
    EXPORT
        aribcaption-targets
    FILE
        aribcaption-targets.cmake
    NAMESPACE
        aribcaption::
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/aribcaption
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/aribcaption-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/aribcaption-config.cmake
    INSTALL_DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/aribcaption
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/aribcaption-config-version.cmake
    VERSION
        ${PROJECT_VERSION}
    COMPATIBILITY
        SameMajorVersion
)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/aribcaption-config.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/aribcaption-config-version.cmake
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/cmake/aribcaption
)

add_subdirectory(test EXCLUDE_FROM_ALL)
