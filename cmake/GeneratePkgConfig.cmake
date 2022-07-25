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
    list(APPEND LIBS_LIST "-l${LIB_OUTPUT_NAME}")

    set(PKG_REQUIRES "")
    set(PKG_LIBS "")

    if(NOT BUILD_SHARED_LIBS)
        if(CMAKE_CXX_IMPLICIT_LINK_LIBRARIES)
            foreach(IMPLICIT_LIB ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
                if (IMPLICIT_LIB MATCHES "-l.*")
                    list(APPEND LIBS_LIST "${IMPLICIT_LIB}")
                else()
                    list(APPEND LIBS_LIST "-l${IMPLICIT_LIB}")
                endif()
                unset(IMPLICIT_LIB)
            endforeach()
        endif()

        if(LIBS_LIST)
            # Blacklist for MinGW-w64
            list(REMOVE_ITEM LIBS_LIST
                "-lmingw32" "-lgcc_s" "-lgcc" "-lmoldname" "-lmingwex" "-lmingwthrd"
                "-lmsvcrt" "-lpthread" "-ladvapi32" "-lshell32" "-luser32" "-lkernel32")
        endif()

        if(ARIBCC_USE_FREETYPE AND NOT ARIBCC_USE_EMBEDDED_FREETYPE)
            # Only required for system-wide installed FreeType
            list(APPEND REQUIRES_LIST "freetype2")
            list(APPEND LIBS_LIST "-lfreetype")
        endif()

        if(ARIBCC_USE_FONTCONFIG)
            list(APPEND REQUIRES_LIST "fontconfig")
            list(APPEND LIBS_LIST "-lfontconfig")
        endif()

        if(ARIBCC_USE_CORETEXT)
            list(APPEND LIBS_LIST "-framework CoreFoundation" "-framework CoreGraphics" "-framework CoreText")
        endif()

        if(ARIBCC_USE_DIRECTWRITE)
            list(APPEND LIBS_LIST "-lole32" "-ld2d1" "-ldwrite" "-lwindowscodecs")
        endif()

        if(ARIBCC_USE_GDI_FONT)
            list(APPEND LIBS_LIST "-lgdi32")
        endif()
    endif()

    string(REPLACE ";" " " PKG_REQUIRES "${REQUIRES_LIST}")
    string(REPLACE ";" " " PKG_LIBS "${LIBS_LIST}")

    configure_file(${template} ${output} @ONLY)
endfunction()
