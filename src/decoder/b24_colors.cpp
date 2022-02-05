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

#include "decoder/b24_colors.hpp"

namespace aribcaption {

extern const ColorRGBA kB24ColorCLUT[][16] = {
    {
        ColorRGBA(  0,   0,   0, 255),
        ColorRGBA(255,   0,   0, 255),
        ColorRGBA(  0, 255,   0, 255),
        ColorRGBA(255, 255,   0, 255),
        ColorRGBA(  0,   0, 255, 255),
        ColorRGBA(255,   0, 255, 255),
        ColorRGBA(  0, 255, 255, 255),
        ColorRGBA(255, 255, 255, 255),
        ColorRGBA(  0,   0,   0,   0),
        ColorRGBA(170,   0,   0, 255),
        ColorRGBA(  0, 170,   0, 255),
        ColorRGBA(170, 170,   0, 255),
        ColorRGBA(  0,   0, 170, 255),
        ColorRGBA(170,   0, 170, 255),
        ColorRGBA(  0, 170, 170, 255),
        ColorRGBA(170, 170, 170, 255)
    },
    {
        ColorRGBA(  0,   0,  85, 255),
        ColorRGBA(  0,  85,   0, 255),
        ColorRGBA(  0,  85,  85, 255),
        ColorRGBA(  0,  85, 170, 255),
        ColorRGBA(  0,  85, 255, 255),
        ColorRGBA(  0, 170,  85, 255),
        ColorRGBA(  0, 170, 255, 255),
        ColorRGBA(  0, 255,  85, 255),
        ColorRGBA(  0, 255, 170, 255),
        ColorRGBA( 85,   0,   0, 255),
        ColorRGBA( 85,   0,  85, 255),
        ColorRGBA( 85,   0, 170, 255),
        ColorRGBA( 85,   0, 255, 255),
        ColorRGBA( 85,  85,   0, 255),
        ColorRGBA( 85,  85,  85, 255),
        ColorRGBA( 85,  85, 170, 255)
    },
    {
        ColorRGBA( 85,  85, 255, 255),
        ColorRGBA( 85, 170,   0, 255),
        ColorRGBA( 85, 170,  85, 255),
        ColorRGBA( 85, 170, 170, 255),
        ColorRGBA( 85, 170, 255, 255),
        ColorRGBA( 85, 255,   0, 255),
        ColorRGBA( 85, 255,  85, 255),
        ColorRGBA( 85, 255, 170, 255),
        ColorRGBA( 85, 255, 255, 255),
        ColorRGBA(170,   0,  85, 255),
        ColorRGBA(170,   0, 255, 255),
        ColorRGBA(170,  85,   0, 255),
        ColorRGBA(170,  85,  85, 255),
        ColorRGBA(170,  85, 170, 255),
        ColorRGBA(170,  85, 255, 255),
        ColorRGBA(170, 170,  85, 255)
    },
    {
        ColorRGBA(170, 170, 255, 255),
        ColorRGBA(170, 255,   0, 255),
        ColorRGBA(170, 255,  85, 255),
        ColorRGBA(170, 255, 170, 255),
        ColorRGBA(170, 255, 255, 255),
        ColorRGBA(255,   0,  85, 255),
        ColorRGBA(255,   0, 170, 255),
        ColorRGBA(255,  85,   0, 255),
        ColorRGBA(255,  85,  85, 255),
        ColorRGBA(255,  85, 170, 255),
        ColorRGBA(255,  85, 255, 255),
        ColorRGBA(255, 170,   0, 255),
        ColorRGBA(255, 170,  85, 255),
        ColorRGBA(255, 170, 170, 255),
        ColorRGBA(255, 170, 255, 255),
        ColorRGBA(255, 255,  85, 255)
    },
    {
        ColorRGBA(255, 255, 170, 255),
        ColorRGBA(  0,   0,   0, 128),
        ColorRGBA(255,   0,   0, 128),
        ColorRGBA(  0, 255,   0, 128),
        ColorRGBA(255, 255,   0, 128),
        ColorRGBA(  0,   0, 255, 128),
        ColorRGBA(255,   0, 255, 128),
        ColorRGBA(  0, 255, 255, 128),
        ColorRGBA(255, 255, 255, 128),
        ColorRGBA(170,   0,   0, 128),
        ColorRGBA(  0, 170,   0, 128),
        ColorRGBA(170, 170,   0, 128),
        ColorRGBA(  0,   0, 170, 128),
        ColorRGBA(170,   0, 170, 128),
        ColorRGBA(  0, 170, 170, 128),
        ColorRGBA(170, 170, 170, 128)
    },
    {
        ColorRGBA(  0,   0,  85, 128),
        ColorRGBA(  0,  85,   0, 128),
        ColorRGBA(  0,  85,  85, 128),
        ColorRGBA(  0,  85, 170, 128),
        ColorRGBA(  0,  85, 255, 128),
        ColorRGBA(  0, 170,  85, 128),
        ColorRGBA(  0, 170, 255, 128),
        ColorRGBA(  0, 255,  85, 128),
        ColorRGBA(  0, 255, 170, 128),
        ColorRGBA( 85,   0,   0, 128),
        ColorRGBA( 85,   0,  85, 128),
        ColorRGBA( 85,   0, 170, 128),
        ColorRGBA( 85,   0, 255, 128),
        ColorRGBA( 85,  85,   0, 128),
        ColorRGBA( 85,  85,  85, 128),
        ColorRGBA( 85,  85, 170, 128)
    },
    {
        ColorRGBA( 85,  85, 255, 128),
        ColorRGBA( 85, 170,   0, 128),
        ColorRGBA( 85, 170,  85, 128),
        ColorRGBA( 85, 170, 170, 128),
        ColorRGBA( 85, 170, 255, 128),
        ColorRGBA( 85, 255,   9, 128),
        ColorRGBA( 85, 255,  85, 128),
        ColorRGBA( 85, 255, 170, 128),
        ColorRGBA( 85, 255, 255, 128),
        ColorRGBA(170,   0,  85, 128),
        ColorRGBA(170,   0, 255, 128),
        ColorRGBA(170,  85,   0, 128),
        ColorRGBA(170,  85,  85, 128),
        ColorRGBA(170,  85, 170, 128),
        ColorRGBA(170,  85, 255, 128),
        ColorRGBA(170, 170,  85, 128)
    },
    {
        ColorRGBA(170, 170, 255, 128),
        ColorRGBA(170, 255,   0, 128),
        ColorRGBA(170, 255,  85, 128),
        ColorRGBA(170, 255, 170, 128),
        ColorRGBA(170, 255, 255, 128),
        ColorRGBA(255,   0,  85, 128),
        ColorRGBA(255,   0, 170, 128),
        ColorRGBA(255,  85,   9, 128),
        ColorRGBA(255,  85,  85, 128),
        ColorRGBA(255,  85, 170, 128),
        ColorRGBA(255,  85, 255, 128),
        ColorRGBA(255, 170,   0, 128),
        ColorRGBA(255, 170,  85, 128),
        ColorRGBA(255, 170, 170, 128),
        ColorRGBA(255, 170, 255, 128),
        ColorRGBA(255, 255,  85, 128)
    }
};

}  // namespace aribcaption
