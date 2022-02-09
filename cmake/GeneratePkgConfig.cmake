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

function(generate_pkg_config_pc_file TARGET TEMPLATE template OUTPUT output)
    # Library output name, used in Libs
    get_target_property(LIB_OUTPUT_NAME "${TARGET}" LIBRARY_OUTPUT_NAME)
    if(NOT LIB_OUTPUT_NAME)
        get_target_property(LIB_OUTPUT_NAME "${TARGET}" NAME)
    endif()

    set(PKG_REQUIRES_PRIVATE "")
    set(PKG_LIBS_PRIVATE "")

    if(NOT BUILD_SHARED_LIBS)
        if(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
            foreach(IMPLICIT_LIB ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
                string(APPEND PKG_LIBS_PRIVATE "-l${IMPLICIT_LIB} ")
                unset(IMPLICIT_LIB)
            endforeach()
        endif()

        if(ARIBCC_USE_FREETYPE AND NOT ARIBCC_USE_EMBEDDED_FREETYPE)
            # Only required for system-wide installed FreeType
            string(APPEND PKG_LIBS_PRIVATE "-lfreetype ")
            string(APPEND PKG_REQUIRES_PRIVATE "freetype2 ")
        endif()

        if(ARIBCC_USE_FONTCONFIG)
            string(APPEND PKG_LIBS_PRIVATE "-lfontconfig ")
            string(APPEND PKG_REQUIRES_PRIVATE "fontconfig ")
        endif()

        if(ARIBCC_USE_CORETEXT)
            string(APPEND PKG_LIBS_PRIVATE "-framework CoreFoundation -framework CoreGraphics -framework CoreText")
        endif()

        if(ARIBCC_USE_DIRECTWRITE)
            string(APPEND PKG_LIBS_PRIVATE "-lole32 -ld2d1 -ldwrite -lwindowscodecs")
        endif()
    endif()

    configure_file(${template} ${output} @ONLY)
endfunction()
