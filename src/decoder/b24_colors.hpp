/*
 * Copyright (C) 2021 magicxqq <xqq@xqq.im>. All rights reserved.
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

#ifndef ARIBCAPTION_B24_COLORS_HPP
#define ARIBCAPTION_B24_COLORS_HPP

#include <cstdint>

namespace aribcaption {

// Represents RGBA byte-order. Appears as AGBR in word-order on LE machines.
using B24ColorRGBA = uint32_t;

static constexpr B24ColorRGBA MakeRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // On Little-Endian machine, RGBA byte-order appears as AGBR in the word-order
    return (uint32_t)a << 24 |
           (uint32_t)b << 16 |
           (uint32_t)g <<  8 |
           (uint32_t)r <<  0;
}

static constexpr B24ColorRGBA kB24ColorCLUT[][16] = {
    {
        MakeRGBA(  0,   0,   0, 255),
        MakeRGBA(255,   0,   0, 255),
        MakeRGBA(  0, 255,   0, 255),
        MakeRGBA(255, 255,   0, 255),
        MakeRGBA(  0,   0, 255, 255),
        MakeRGBA(255,   0, 255, 255),
        MakeRGBA(  0, 255, 255, 255),
        MakeRGBA(255, 255, 255, 255),
        MakeRGBA(  0,   0,   0,   0),
        MakeRGBA(170,   0,   0, 255),
        MakeRGBA(  0, 170,   0, 255),
        MakeRGBA(170, 170,   0, 255),
        MakeRGBA(  0,   0, 170, 255),
        MakeRGBA(170,   0, 170, 255),
        MakeRGBA(  0, 170, 170, 255),
        MakeRGBA(170, 170, 170, 255)
    },
    {
        MakeRGBA(  0,   0,  85, 255),
        MakeRGBA(  0,  85,   0, 255),
        MakeRGBA(  0,  85,  85, 255),
        MakeRGBA(  0,  85, 170, 255),
        MakeRGBA(  0,  85, 255, 255),
        MakeRGBA(  0, 170,  85, 255),
        MakeRGBA(  0, 170, 255, 255),
        MakeRGBA(  0, 255,  85, 255),
        MakeRGBA(  0, 255, 170, 255),
        MakeRGBA( 85,   0,   0, 255),
        MakeRGBA( 85,   0,  85, 255),
        MakeRGBA( 85,   0, 170, 255),
        MakeRGBA( 85,   0, 255, 255),
        MakeRGBA( 85,  85,   0, 255),
        MakeRGBA( 85,  85,  85, 255),
        MakeRGBA( 85,  85, 170, 255)
    },
    {
        MakeRGBA( 85,  85, 255, 255),
        MakeRGBA( 85, 170,   0, 255),
        MakeRGBA( 85, 170,  85, 255),
        MakeRGBA( 85, 170, 170, 255),
        MakeRGBA( 85, 170, 255, 255),
        MakeRGBA( 85, 255,   0, 255),
        MakeRGBA( 85, 255,  85, 255),
        MakeRGBA( 85, 255, 170, 255),
        MakeRGBA( 85, 255, 255, 255),
        MakeRGBA(170,   0,  85, 255),
        MakeRGBA(170,   0, 255, 255),
        MakeRGBA(170,  85,   0, 255),
        MakeRGBA(170,  85,  85, 255),
        MakeRGBA(170,  85, 170, 255),
        MakeRGBA(170,  85, 255, 255),
        MakeRGBA(170, 170,  85, 255)
    },
    {
        MakeRGBA(170, 170, 255, 255),
        MakeRGBA(170, 255,   0, 255),
        MakeRGBA(170, 255,  85, 255),
        MakeRGBA(170, 255, 170, 255),
        MakeRGBA(170, 255, 255, 255),
        MakeRGBA(255,   0,  85, 255),
        MakeRGBA(255,   0, 170, 255),
        MakeRGBA(255,  85,   0, 255),
        MakeRGBA(255,  85,  85, 255),
        MakeRGBA(255,  85, 170, 255),
        MakeRGBA(255,  85, 255, 255),
        MakeRGBA(255, 170,   0, 255),
        MakeRGBA(255, 170,  85, 255),
        MakeRGBA(255, 170, 170, 255),
        MakeRGBA(255, 170, 255, 255),
        MakeRGBA(255, 255,  85, 255)
    },
    {
        MakeRGBA(255, 255, 170, 255),
        MakeRGBA(  0,   0,   0, 128),
        MakeRGBA(255,   0,   0, 128),
        MakeRGBA(  0, 255,   0, 128),
        MakeRGBA(255, 255,   0, 128),
        MakeRGBA(  0,   0, 255, 128),
        MakeRGBA(255,   0, 255, 128),
        MakeRGBA(  0, 255, 255, 128),
        MakeRGBA(255, 255, 255, 128),
        MakeRGBA(170,   0,   0, 128),
        MakeRGBA(  0, 170,   0, 128),
        MakeRGBA(170, 170,   0, 128),
        MakeRGBA(  0,   0, 170, 128),
        MakeRGBA(170,   0, 170, 128),
        MakeRGBA(  0, 170, 170, 128),
        MakeRGBA(170, 170, 170, 128)
    },
    {
        MakeRGBA(  0,   0,  85, 128),
        MakeRGBA(  0,  85,   0, 128),
        MakeRGBA(  0,  85,  85, 128),
        MakeRGBA(  0,  85, 170, 128),
        MakeRGBA(  0,  85, 255, 128),
        MakeRGBA(  0, 170,  85, 128),
        MakeRGBA(  0, 170, 255, 128),
        MakeRGBA(  0, 255,  85, 128),
        MakeRGBA(  0, 255, 170, 128),
        MakeRGBA( 85,   0,   0, 128),
        MakeRGBA( 85,   0,  85, 128),
        MakeRGBA( 85,   0, 170, 128),
        MakeRGBA( 85,   0, 255, 128),
        MakeRGBA( 85,  85,   0, 128),
        MakeRGBA( 85,  85,  85, 128),
        MakeRGBA( 85,  85, 170, 128)
    },
    {
        MakeRGBA( 85,  85, 255, 128),
        MakeRGBA( 85, 170,   0, 128),
        MakeRGBA( 85, 170,  85, 128),
        MakeRGBA( 85, 170, 170, 128),
        MakeRGBA( 85, 170, 255, 128),
        MakeRGBA( 85, 255,   9, 128),
        MakeRGBA( 85, 255,  85, 128),
        MakeRGBA( 85, 255, 170, 128),
        MakeRGBA( 85, 255, 255, 128),
        MakeRGBA(170,   0,  85, 128),
        MakeRGBA(170,   0, 255, 128),
        MakeRGBA(170,  85,   0, 128),
        MakeRGBA(170,  85,  85, 128),
        MakeRGBA(170,  85, 170, 128),
        MakeRGBA(170,  85, 255, 128),
        MakeRGBA(170, 170,  85, 128)
    },
    {
        MakeRGBA(170, 170, 255, 128),
        MakeRGBA(170, 255,   0, 128),
        MakeRGBA(170, 255,  85, 128),
        MakeRGBA(170, 255, 170, 128),
        MakeRGBA(170, 255, 255, 128),
        MakeRGBA(255,   0,  85, 128),
        MakeRGBA(255,   0, 170, 128),
        MakeRGBA(255,  85,   9, 128),
        MakeRGBA(255,  85,  85, 128),
        MakeRGBA(255,  85, 170, 128),
        MakeRGBA(255,  85, 255, 128),
        MakeRGBA(255, 170,   0, 128),
        MakeRGBA(255, 170,  85, 128),
        MakeRGBA(255, 170, 170, 128),
        MakeRGBA(255, 170, 255, 128),
        MakeRGBA(255, 255,  85, 128)
    }
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_B24_COLORS_HPP
