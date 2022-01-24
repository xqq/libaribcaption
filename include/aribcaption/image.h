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

#ifndef ARIBCAPTION_IMAGE_H
#define ARIBCAPTION_IMAGE_H

#include <stddef.h>
#include <stdint.h>
#include "aribcc_export.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum aribcc_pixelformat_t {
    ARIBCC_PIXELFORMAT_RGBA8888 = 0,
    ARIBCC_PIXELFORMAT_DEFAULT = ARIBCC_PIXELFORMAT_RGBA8888
} aribcc_pixelformat_t;


typedef struct aribcc_image_t {
    int width;
    int height;
    int stride;

    int dst_x;
    int dst_y;

    aribcc_pixelformat_t pixel_format;

    uint8_t* bitmap;
    uint32_t bitmap_size;
} aribcc_image_t;


ARIBCC_API void aribcc_image_cleanup(aribcc_image_t* image);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_IMAGE_H
