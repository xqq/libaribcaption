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

/**
 * enums for pixel format used by aribcc api.
 *
 * Only @ARIBCC_PIXELFORMAT_RGBA8888 is used for now.
 */
typedef enum aribcc_pixelformat_t {
    ARIBCC_PIXELFORMAT_RGBA8888 = 0,
    ARIBCC_PIXELFORMAT_DEFAULT = ARIBCC_PIXELFORMAT_RGBA8888
} aribcc_pixelformat_t;


/**
 * Structure represents a rendered caption image produced by the renderer
 */
typedef struct aribcc_image_t {
    int width;     ///< bitmap width
    int height;    ///< bitmap height
    int stride;    ///< bytes in a line, include margins for memory alignment

    int dst_x;     ///< x coordinate of bitmap's top-left corner inside the player's renderer frame
    int dst_y;     ///< y coordinate of bitmap's top-left corner inside the player's renderer frame

    aribcc_pixelformat_t pixel_format;    ///< pixel format, always be @ARIBCC_PIXELFORMAT_RGBA8888

    /**
     * Pointer pointed to the bitmap area. The buffer size is indicated in bitmap_size field.
     *
     * Do not manually free this pointer if you received this image from the renderer.
     * Call @aribcc_image_cleanup() instead.
     */
    uint8_t* bitmap;
    uint32_t bitmap_size;
} aribcc_image_t;


/**
 * Cleanup the @aribcc_image_t structure, include the buffer where the bitmap pointed to.
 *
 * Call this function only if if you received the image from aribcc API.
 * Otherwise it may cause a crash.
 *
 * This function doesn't release the memory of the @aribcc_image_t itself.
 */
ARIBCC_API void aribcc_image_cleanup(aribcc_image_t* image);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_IMAGE_H
