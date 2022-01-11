/*
 * Copyright (C) 2022 magicxqq <xqq@xqq.im>. All rights reserved.
 *
 * This file is part of libaribcaption.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ARIBCAPTION_ARIBCC_EXPORT_H
#define ARIBCAPTION_ARIBCC_EXPORT_H

#include "aribcc_config.h"

#ifdef ARIBCC_SHARED_LIBRARY
    #ifdef _WIN32
        #ifdef ARIBCC_IMPLEMENTATION
            #define ARIBCC_API __declspec(dllexport)
        #else
            #define ARIBCC_API __declspec(dllimport)
        #endif
    #else
        #ifdef ARIBCC_IMPLEMENTATION
            #define ARIBCC_API __attribute__((visibility("default")))
        #else
            #define ARIBCC_API
        #endif
    #endif
#else
    #define ARIBCC_API
#endif

#endif  // ARIBCAPTION_ARIBCC_EXPORT_H
