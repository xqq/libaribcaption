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

cmake_minimum_required(VERSION 3.11)

project(libaribcaption VERSION 0.1.0 LANGUAGES C CXX)

# main project detecting
# avoid building test subdirectory if used as subproject (e.g. add_subdirectory)
if (CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(ARIBCC_IS_MAIN_PROJECT TRUE)
endif()

# Release build by default
if(NOT CMAKE_BUILD_TYPE)
    message("Build type will be Release. Set CMAKE_BUILD_TYPE to indicate other types.")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Default to Release build" FORCE)
endif()

if(WIN32)
    # Windows, use DirectWrite
    option(ARIBCC_USE_DIRECTWRITE "Enable DirectWrite text rendering backend" ON)
elseif(APPLE)
    # macOS or iOS, use CoreText by default
    option(ARIBCC_USE_CORETEXT "Enable CoreText text rendering backend" ON)
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Android")
    # Android, FreeType required
    set(ARIBCC_IS_ANDROID TRUE CACHE BOOL "Specify target OS is Android")
    option(ARIBCC_USE_FREETYPE "Enable FreeType text rendering backend" ON)
else()
    # Linux or other Unix systems, requires Fontconfig & FreeType
    option(ARIBCC_USE_FONTCONFIG "Enable Fontconfig font provider" ON)
    option(ARIBCC_USE_FREETYPE "Enable FreeType text rendering backend" ON)
endif()

option(ARIBCC_USE_EMBEDDED_FREETYPE "Use embedded FreeType instead of find_package from system" OFF)

if(ARIBCC_USE_CORETEXT)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
    find_library(CORETEXT_FRAMEWORK CoreText)
endif()

if(ARIBCC_USE_FONTCONFIG)
    find_package(Fontconfig REQUIRED)
endif()

function(import_embedded_freetype)
    include(FetchContent)
    FetchContent_Declare(freetype
        GIT_REPOSITORY https://github.com/freetype/freetype.git
        GIT_TAG VER-2-11-1
        GIT_SHALLOW TRUE             # Get only target tag version
        GIT_PROGRESS TRUE            # Show download progress
        USES_TERMINAL_DOWNLOAD TRUE  # show download progress in ninja
        USES_TERMINAL_CONFIGURE TRUE # show configure progress in ninja
        USES_TERMINAL_BUILD TRUE     # show build progress in ninja
    )
    FetchContent_GetProperties(freetype)
    if(NOT freetype_POPULATED)
        # Fetch the content
        FetchContent_Populate(freetype)
        # Set variables for FreeType
        set(FT_DISABLE_ZLIB ON CACHE BOOL "Disable zlib for embedded Freetype" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "Disable libpng for embedded Freetype" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable harfbuzz for embedded Freetype" FORCE)
        set(BUILD_SHARED_LIBS OFF)       # Force static link

        # Suppress CMake Policy CMP0048 warning
        set(CMAKE_PROJECT_INCLUDE_BEFORE "${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnableCMP0048.cmake")
        # Import FreeType directory
        add_subdirectory(${freetype_SOURCE_DIR} ${freetype_BINARY_DIR} EXCLUDE_FROM_ALL)
        unset(CMAKE_PROJECT_INCLUDE_BEFORE)

        set(FREETYPE_INCLUDE_DIRS "${freetype_BINARY_DIR}/include;${freetype_SOURCE_DIR}/include" PARENT_SCOPE)
        set(FREETYPE_LIBRARIES freetype PARENT_SCOPE)
    endif()
endfunction()

if(ARIBCC_USE_FREETYPE AND (NOT ARIBCC_USE_EMBEDDED_FREETYPE))
    # Try Use system-wide installed freetype
    find_package(Freetype)
    if(NOT FREETYPE_FOUND)
        # System-wide freetype not found, use FetchContent to import embedded freetype
        message(STATUS "System-wide installed FreeType not found, fallback to use embedded FreeType")
        set(ARIBCC_USE_EMBEDDED_FREETYPE ON CACHE BOOL "Force embedded FreeType" FORCE)
    else()
        message(STATUS "Use system-wide installed FreeType library from find_package")
    endif()
endif()

if(ARIBCC_USE_EMBEDDED_FREETYPE)
    set(ARIBCC_USE_FREETYPE ON CACHE BOOL "Enable Freetype by ARIBCC_USE_EMBEDDED_FREETYPE" FORCE)
    message(STATUS "Use static-linked embedded FreeType library")
    import_embedded_freetype()
endif()

# Generate config header
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/aribcc_config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/include/aribcc_config.h"
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
        src/base/logger.cpp
        src/base/logger.hpp
        src/base/md5.c
        src/base/md5.h
        src/base/md5_helper.hpp
        src/base/result.hpp
        src/base/scoped_cfref.hpp
        src/base/scoped_com_initializer.hpp
        src/base/scoped_holder.hpp
        $<$<BOOL:${ARIBCC_IS_ANDROID}>:src/base/tinyxml2.cpp>
        $<$<BOOL:${ARIBCC_IS_ANDROID}>:src/base/tinyxml2.h>
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
        src/renderer/alphablend.hpp
        src/renderer/alphablend_generic.hpp
        src/renderer/alphablend_x86.hpp
        src/renderer/bitmap.cpp
        src/renderer/bitmap.hpp
        src/renderer/canvas.cpp
        src/renderer/canvas.hpp
        src/renderer/drcs_renderer.cpp
        src/renderer/drcs_renderer.hpp
        src/renderer/font_provider.cpp
        src/renderer/font_provider.hpp
        $<$<BOOL:${ARIBCC_IS_ANDROID}>:src/renderer/font_provider_android.cpp>
        $<$<BOOL:${ARIBCC_IS_ANDROID}>:src/renderer/font_provider_android.hpp>
        $<$<BOOL:${ARIBCC_USE_CORETEXT}>:src/renderer/font_provider_coretext.cpp>
        $<$<BOOL:${ARIBCC_USE_CORETEXT}>:src/renderer/font_provider_coretext.hpp>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:src/renderer/font_provider_directwrite.cpp>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:src/renderer/font_provider_directwrite.hpp>
        $<$<BOOL:${ARIBCC_USE_FONTCONFIG}>:src/renderer/font_provider_fontconfig.cpp>
        $<$<BOOL:${ARIBCC_USE_FONTCONFIG}>:src/renderer/font_provider_fontconfig.hpp>
        src/renderer/rect.hpp
        src/renderer/region_renderer.cpp
        src/renderer/region_renderer.hpp
        src/renderer/renderer.cpp
        src/renderer/renderer_impl.cpp
        src/renderer/renderer_impl.hpp
        src/renderer/text_renderer.cpp
        src/renderer/text_renderer.hpp
        $<$<BOOL:${ARIBCC_USE_CORETEXT}>:src/renderer/text_renderer_coretext.cpp>
        $<$<BOOL:${ARIBCC_USE_CORETEXT}>:src/renderer/text_renderer_coretext.hpp>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:src/renderer/text_renderer_directwrite.cpp>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:src/renderer/text_renderer_directwrite.hpp>
        $<$<BOOL:${ARIBCC_USE_FREETYPE}>:src/renderer/text_renderer_freetype.cpp>
        $<$<BOOL:${ARIBCC_USE_FREETYPE}>:src/renderer/text_renderer_freetype.hpp>
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

# Enable necessary SSE instruction sets for x86/x64 build on gcc/clang
if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)" AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86|X86|x64|X64|amd64|AMD64|i386|i686)")
    target_compile_options(aribcaption
        PRIVATE
            -msse
            -msse2
    )
endif()

target_compile_definitions(aribcaption
    PRIVATE
        $<$<BOOL:${WIN32}>:
            NOMINMAX
            UNICODE
            _UNICODE
            _CRT_SECURE_NO_WARNINGS
        >
)

### Includes
target_include_directories(aribcaption
    PUBLIC
        $<INSTALL_INTERFACE:include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        $<$<BOOL:${ARIBCC_USE_FONTCONFIG}>:${Fontconfig_INCLUDE_DIRS}>
        $<$<BOOL:${ARIBCC_USE_FREETYPE}>:${FREETYPE_INCLUDE_DIRS}>
)

### Linking
target_link_libraries(aribcaption
    PRIVATE
        $<$<BOOL:${ARIBCC_USE_CORETEXT}>:${COREFOUNDATION_FRAMEWORK}>
        $<$<BOOL:${ARIBCC_USE_CORETEXT}>:${COREGRAPHICS_FRAMEWORK}>
        $<$<BOOL:${ARIBCC_USE_CORETEXT}>:${CORETEXT_FRAMEWORK}>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:d2d1>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:dwrite>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:windowscodecs>
)

# vcpkg uses optimized/debug keyword in XXXXX_LIBRARIES variables
# which is incompatible with generator expressions
if(ARIBCC_USE_FONTCONFIG)
    target_link_libraries(aribcaption
        PRIVATE
            ${Fontconfig_LIBRARIES}
    )
endif()

if(ARIBCC_USE_FREETYPE)
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
    PATTERN "aribcc_config.h.in" EXCLUDE
    PATTERN ".DS_Store" EXCLUDE
)

# Install generated config header
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/aribcc_config.h
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

if(ARIBCC_IS_MAIN_PROJECT)
    add_subdirectory(test EXCLUDE_FROM_ALL)
endif()
