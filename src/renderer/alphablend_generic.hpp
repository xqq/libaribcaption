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

#ifndef ARIBCAPTION_ALPHABLEND_GENERIC_HPP
#define ARIBCAPTION_ALPHABLEND_GENERIC_HPP

#include <cstddef>
#include <cstdint>
#include "aribcaption/color.hpp"
#include "base/always_inline.hpp"

namespace aribcaption::alphablend {

// Fast divide by 255 algorithm
ALWAYS_INLINE uint32_t Div255(uint32_t x) {
    return (x + 1 + (x >> 8)) >> 8;
}

// Fast clamp to 255 algorithm
ALWAYS_INLINE uint8_t Clamp255(uint32_t x) {
    x |= -(x > 255);
    return static_cast<uint8_t>(x);
}

// Alpha blend
// alpha is within range [0, 1] in the formula
// out_alpha = foreground_alpha + background_alpha * (1 - foreground_alpha)
// out_rgb = foreground_rgb * foreground_alpha + background_rgb * (1 - foreground_alpha)
ALWAYS_INLINE ColorRGBA BlendColor(ColorRGBA bg_color, ColorRGBA fg_color) {
    //        memory_order_RGBA   = 0xAABBGGRR
    constexpr uint32_t mask_b_r   = 0x00FF00FF;
    constexpr uint32_t mask_g     = 0x0000FF00;
    constexpr uint32_t mask_a_g   = 0xFF00FF00;
    constexpr uint32_t alpha_1    = 0x01000000;

    uint32_t fg_a = fg_color.a;
    uint32_t ff_minus_fg_a = 255 - fg_a;

    uint32_t b_r = ((fg_color.u32 & mask_b_r) * fg_a + ((bg_color.u32 & mask_b_r) * ff_minus_fg_a)) >> 8;
    uint32_t a_g = ((alpha_1 | ((fg_color.u32 & mask_g) >> 8)) * fg_a) + (((bg_color.u32 & mask_a_g) >> 8) * ff_minus_fg_a);

    return ColorRGBA((b_r & mask_b_r) | (a_g & mask_a_g));
}

// Alpha blend for premultiplied src(foreground)
// alpha is within range [0, 1] in the formula
// out_alpha = foreground_alpha + background_alpha * (1 - foreground_alpha)
// out_rgb = foreground_rgb + background_rgb * (1 - foreground_alpha)
ALWAYS_INLINE ColorRGBA BlendColor_PremultipliedSrc(ColorRGBA bg_color, ColorRGBA fg_color) {
    //        memory_order_RGBA   = 0xAABBGGRR
    constexpr uint32_t mask_b_r   = 0x00FF00FF;
    constexpr uint32_t mask_a_g   = 0xFF00FF00;

    uint32_t ff_minus_fg_a = 255 - fg_color.a;

    uint32_t b_r = (fg_color.u32 & mask_b_r) + (((bg_color.u32 & mask_b_r) * ff_minus_fg_a) >> 8);
    uint32_t a_g = (fg_color.u32 & mask_a_g) + (((bg_color.u32 & mask_a_g) >> 8) * ff_minus_fg_a);

    return ColorRGBA((b_r & mask_b_r) | (a_g & mask_a_g));
}


namespace internal {

ALWAYS_INLINE void FillLine_Generic(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    for (size_t i = 0; i < width; i++) {
        dest[i] = color;
    }
}

ALWAYS_INLINE void FillLineWithAlphas_Generic(ColorRGBA* __restrict dest,
                                              const uint8_t* __restrict src_alphas, ColorRGBA color, size_t width) {
    for (size_t i = 0; i < width; i++) {
        uint8_t alpha = (static_cast<uint32_t>(src_alphas[i]) * color.a) >> 8;
        dest[i] = ColorRGBA(color, alpha);
    }
}

ALWAYS_INLINE void BlendColorToLine_Generic(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    for (size_t i = 0; i < width; i++) {
        dest[i] = BlendColor(dest[i], color);
    }
}

ALWAYS_INLINE void BlendLine_Generic(ColorRGBA* __restrict dest, const ColorRGBA* __restrict src, size_t width) {
    for (size_t i = 0; i < width; i++) {
        dest[i] = BlendColor(dest[i], src[i]);
    }
}

ALWAYS_INLINE void BlendLine_PremultipliedSrc_Generic(ColorRGBA* __restrict dest,
                                                      const ColorRGBA* __restrict src, size_t width) {
    for (size_t i = 0; i < width; i++) {
        dest[i] = BlendColor_PremultipliedSrc(dest[i], src[i]);
    }
}

}  // namespace internal


}  // namespace aribcaption::alphablend

#endif  // ARIBCAPTION_ALPHABLEND_GENERIC_HPP
