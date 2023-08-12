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

project(libaribcaption
    VERSION
        1.0.1
    DESCRIPTION
        "Portable ARIB STD-B24 Caption Decoder/Renderer"
    HOMEPAGE_URL
        "https://github.com/xqq/libaribcaption"
    LANGUAGES
        C CXX
)

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

# Indicate -DARIBCC_BUILD_TESTS:BOOL=ON to build tests
option(ARIBCC_BUILD_TESTS "Build libaribcaption tests" OFF)

# Indicate -DARIBCC_SHARED_LIBRARY:BOOL=ON to build as shared library
option(ARIBCC_SHARED_LIBRARY "Build libaribcaption as shared library" OFF)

# Indicate -DARIBCC_NO_EXCEPTIONS:BOOL=ON to disable C++ exceptions
option(ARIBCC_NO_EXCEPTIONS "Disable C++ Exceptions" OFF)

# Indicate -DARIBCC_NO_RTTI:BOOL=ON to disable C++ RTTI
option(ARIBCC_NO_RTTI "Disable C++ RTTI" OFF)

# Indicate -DARIBCC_NO_RENDERER:BOOL=ON to disable renderer
option(ARIBCC_NO_RENDERER "Disable Renderer" OFF)

if(ARIBCC_NO_RENDERER)
    # Renderer is disabled
    message(STATUS "Renderer has been disabled by ARIBCC_NO_RENDERER")
elseif(WIN32)
    # Windows, use DirectWrite by default
    option(ARIBCC_USE_DIRECTWRITE "Enable DirectWrite text rendering backend" ON)
    option(ARIBCC_USE_GDI_FONT "Enable Win32 GDI font provider" OFF)
elseif(APPLE)
    # macOS or iOS, use CoreText by default
    option(ARIBCC_USE_CORETEXT "Enable CoreText text rendering backend" ON)
elseif(ANDROID OR (${CMAKE_SYSTEM_NAME} STREQUAL "Android") OR ARIBCC_IS_ANDROID)
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
        set(FT_DISABLE_BZIP2 ON CACHE BOOL "Disable bzip2 for embedded Freetype" FORCE)
        set(FT_DISABLE_PNG ON CACHE BOOL "Disable libpng for embedded Freetype" FORCE)
        set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "Disable harfbuzz for embedded Freetype" FORCE)
        set(FT_DISABLE_BROTLI ON CACHE BOOL "Disable brotli for embedded Freetype" FORCE)
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

if(ARIBCC_SHARED_LIBRARY AND (NOT BUILD_SHARED_LIBS))
    set(BUILD_SHARED_LIBS ON)
endif()

if(BUILD_SHARED_LIBS AND (NOT ARIBCC_SHARED_LIBRARY))
    set(ARIBCC_SHARED_LIBRARY ON CACHE BOOL "Indicate built as shared library" FORCE)
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
        include/aribcaption/aribcc_export.h
        include/aribcaption/caption.h
        include/aribcaption/caption.hpp
        include/aribcaption/color.h
        include/aribcaption/color.hpp
        include/aribcaption/context.h
        include/aribcaption/context.hpp
        include/aribcaption/decoder.h
        include/aribcaption/decoder.hpp
        src/base/aligned_alloc.cpp
        src/base/always_inline.hpp
        src/base/cfstr_helper.hpp
        src/base/floating_helper.hpp
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
        src/base/unicode_helper.hpp
        src/base/utf_helper.hpp
        src/base/wchar_helper.hpp
        src/common/caption_capi.cpp
        src/common/context.cpp
        src/common/context_capi.cpp
        src/decoder/b24_codesets.cpp
        src/decoder/b24_codesets.hpp
        src/decoder/b24_colors.cpp
        src/decoder/b24_colors.hpp
        src/decoder/b24_controlsets.hpp
        src/decoder/b24_conv_tables.hpp
        src/decoder/b24_drcs_conv.cpp
        src/decoder/b24_drcs_conv.hpp
        src/decoder/b24_gaiji_table.hpp
        src/decoder/b24_macros.hpp
        src/decoder/decoder.cpp
        src/decoder/decoder_capi.cpp
        src/decoder/decoder_impl.cpp
        src/decoder/decoder_impl.hpp
)

# Append renderer-related sources if renderer not disabled
if(NOT ARIBCC_NO_RENDERER)
    target_sources(aribcaption PRIVATE
        include/aribcaption/image.h
        include/aribcaption/image.hpp
        include/aribcaption/renderer.h
        include/aribcaption/renderer.hpp
        $<$<BOOL:${ARIBCC_IS_ANDROID}>:src/base/tinyxml2.cpp>
        $<$<BOOL:${ARIBCC_IS_ANDROID}>:src/base/tinyxml2.h>
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
        $<$<BOOL:${ARIBCC_USE_GDI_FONT}>:src/renderer/font_provider_gdi.cpp>
        $<$<BOOL:${ARIBCC_USE_GDI_FONT}>:src/renderer/font_provider_gdi.hpp>
        src/renderer/image_capi.cpp
        src/renderer/rect.hpp
        src/renderer/region_renderer.cpp
        src/renderer/region_renderer.hpp
        src/renderer/renderer.cpp
        src/renderer/renderer_capi.cpp
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
    )
endif()

add_library(aribcaption::aribcaption ALIAS aribcaption)

target_compile_features(aribcaption
    PRIVATE
        cxx_std_17
)

# Indicate `-fvisibility=hidden` to hide non-export symbols
set_target_properties(aribcaption
    PROPERTIES
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
)

# Enable /utf-8 for MSVC
target_compile_options(aribcaption
    PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/utf-8>
)

# Disable C++ exceptions, if required
if(ARIBCC_NO_EXCEPTIONS)
    if(MSVC)
        string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        target_compile_options(aribcaption
            PRIVATE
                /EHs-c-
        )
        target_compile_definitions(aribcaption
            PRIVATE
                _HAS_EXCEPTIONS=0
        )
    else()
        string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        target_compile_options(aribcaption
            PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
        )
    endif()
endif()

# Disable C++ RTTI, if required
if(ARIBCC_NO_RTTI)
    if(MSVC)
        string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        target_compile_options(aribcaption
            PUBLIC
                /GR-
        )
    else()
        string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        target_compile_options(aribcaption
            PUBLIC
                $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
        )
    endif()
endif()

# Enable necessary SSE instruction sets for x86/x64 build on gcc/clang
if(CMAKE_CXX_COMPILER_ID MATCHES "(GNU|Clang)" AND CMAKE_SYSTEM_PROCESSOR MATCHES "(x86|X86|x64|X64|amd64|AMD64|i386|i686)")
    target_compile_options(aribcaption
        PRIVATE
            -msse
            -msse2
    )
endif()

# Disable aligned allocation on Apple platforms, which is only supported on macOS 10.14 / iOS 11 or newer
if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(aribcaption
        PRIVATE
            -fno-aligned-allocation
    )
endif()

target_compile_definitions(aribcaption
    PRIVATE
        ARIBCC_IMPLEMENTATION
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
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:ole32>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:d2d1>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:dwrite>
        $<$<BOOL:${ARIBCC_USE_DIRECTWRITE}>:windowscodecs>
        $<$<BOOL:${ARIBCC_USE_GDI_FONT}>:gdi32>
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

# Install generated config header
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/include/aribcc_config.h
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/aribcaption
)

# Install decoder-related public headers
install(
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/aribcc_export.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/aribcaption.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/aribcaption.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/caption.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/caption.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/color.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/color.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/context.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/context.hpp
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/decoder.h
        ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/decoder.hpp
    DESTINATION
        ${CMAKE_INSTALL_INCLUDEDIR}/aribcaption
)

# Install renderer-related public headers, if not disabled
if(NOT ARIBCC_NO_RENDERER)
    install(
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/aligned_alloc.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/image.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/image.hpp
            ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/renderer.h
            ${CMAKE_CURRENT_SOURCE_DIR}/include/aribcaption/renderer.hpp
        DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}/aribcaption
    )
endif()

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

## Generate pkg-config config file (.pc)
include(cmake/GeneratePkgConfig.cmake)
generate_pkg_config_pc_file(aribcaption
    TEMPLATE
        ${CMAKE_CURRENT_SOURCE_DIR}/libaribcaption.pc.in
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/libaribcaption.pc
)

## Install pkg-config config file (.pc)
install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/libaribcaption.pc
    DESTINATION
        ${CMAKE_INSTALL_LIBDIR}/pkgconfig
)

### Testing (if enabled)
if(ARIBCC_IS_MAIN_PROJECT AND ARIBCC_BUILD_TESTS)
    add_subdirectory(test EXCLUDE_FROM_ALL)
endif()


### Packaging
set(CPACK_PACKAGE_NAME ${CMAKE_PROJECT_NAME})
set(CPACK_PACKAGE_VENDOR "magicxqq")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${CMAKE_PROJECT_DESCRIPTION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})

if(WIN32)
    set(CPACK_GENERATOR ZIP)
    set(CPACK_SOURCE_GENERATOR ZIP)
else()
    set(CPACK_GENERATOR TGZ)
    set(CPACK_SOURCE_GENERATOR TGZ)
endif()

set(CPACK_SOURCE_IGNORE_FILES
    /.idea
    /.vs
    /.vscode
    /.git
    /.*build.*
    /out
    /screenshots
    /\\\\.DS_Store
    CMakeSettings.json
    compile_commands.json
)

include(CPack)
