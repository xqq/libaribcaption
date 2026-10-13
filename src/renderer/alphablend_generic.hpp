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
#include <algorithm>
#include <array>
#include "aribcaption/color.hpp"
#include "base/always_inline.hpp"

// Workaround Windows.h (minwindef.h) max/min macro definitions
#ifdef max
    #undef max
#endif

#ifdef min
    #undef min
#endif

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

// Rounded x * y / 255, for x and y within [0, 255]
ALWAYS_INLINE uint32_t MultiplyDiv255(uint32_t x, uint32_t y) {
    uint32_t product = x * y + 128;
    return (product + (product >> 8)) >> 8;
}

ALWAYS_INLINE ColorRGBA Premultiply(ColorRGBA color) {
    return ColorRGBA(static_cast<uint8_t>(MultiplyDiv255(color.r, color.a)),
                     static_cast<uint8_t>(MultiplyDiv255(color.g, color.a)),
                     static_cast<uint8_t>(MultiplyDiv255(color.b, color.a)),
                     color.a);
}

// Fast approximation of alpha / 255 * 256, exact at both ends (0 -> 0, 255 -> 256)
ALWAYS_INLINE uint32_t WeightOf(uint32_t alpha) {
    return alpha + (alpha >> 7);
}

// Background weight under a premultiplied foreground: 256 - alpha, or 0 for an opaque foreground.
// Unlike 256 - WeightOf(alpha), this keeps an opaque background opaque.
ALWAYS_INLINE uint32_t BackgroundWeightOf(uint32_t alpha) {
    return 256 - alpha - ((alpha + 1) >> 8);
}

// Alpha blend for straight src(foreground) over premultiplied dst(background)
// alpha is within range [0, 1] in the formula
// out_alpha = foreground_alpha + background_alpha * (1 - foreground_alpha)
// out_rgb = foreground_rgb * foreground_alpha + background_rgb * (1 - foreground_alpha)
ALWAYS_INLINE uint32_t BlendColor(uint32_t bg_color, uint32_t fg_color) {
    //        memory_order_RGBA   = 0xAABBGGRR
    constexpr uint32_t mask_b_r   = 0x00FF00FF;
    constexpr uint32_t mask_g     = 0x0000FF00;
    constexpr uint32_t mask_a_g   = 0xFF00FF00;
    constexpr uint32_t alpha_255  = 0x00FF0000;

    uint32_t fg_weight = WeightOf(fg_color >> 24);
    uint32_t bg_weight = 256 - fg_weight;

    // Alpha is weighted like a color channel of 255, which keeps out_rgb <= out_alpha
    uint32_t b_r = ((fg_color & mask_b_r) * fg_weight + (bg_color & mask_b_r) * bg_weight) >> 8;
    uint32_t a_g = ((alpha_255 | ((fg_color & mask_g) >> 8)) * fg_weight) +
                   (((bg_color & mask_a_g) >> 8) * bg_weight);

    return (b_r & mask_b_r) | (a_g & mask_a_g);
}

// Alpha blend for premultiplied src(foreground) over premultiplied dst(background)
// alpha is within range [0, 1] in the formula
// out_alpha = foreground_alpha + background_alpha * (1 - foreground_alpha)
// out_rgb = foreground_rgb + background_rgb * (1 - foreground_alpha)
ALWAYS_INLINE uint32_t BlendColor_PremultipliedSrc(uint32_t bg_color, uint32_t fg_color) {
    //        memory_order_RGBA   = 0xAABBGGRR
    constexpr uint32_t mask_b_r   = 0x00FF00FF;
    constexpr uint32_t mask_a_g   = 0xFF00FF00;

    uint32_t bg_weight = BackgroundWeightOf(fg_color >> 24);

    uint32_t b_r = (fg_color & mask_b_r) + (((bg_color & mask_b_r) * bg_weight) >> 8);
    uint32_t a_g = (fg_color & mask_a_g) + (((bg_color & mask_a_g) >> 8) * bg_weight);

    return (b_r & mask_b_r) | (a_g & mask_a_g);
}

struct alignas(8) UnpremultiplyFactors {
    uint16_t rgba[4];
};

// {255 / alpha, 255 / alpha, 255 / alpha, 1} in 8.8 fixed point
inline constexpr std::array<UnpremultiplyFactors, 256> kUnpremultiplyFactors = [] {
    std::array<UnpremultiplyFactors, 256> table{};
    table[0] = {{0, 0, 0, 256}};
    for (uint32_t alpha = 1; alpha < table.size(); alpha++) {
        auto factor = static_cast<uint16_t>((65280 + alpha / 2) / alpha);
        table[alpha] = {{factor, factor, factor, 256}};
    }
    return table;
}();

// Channels are clamped to alpha first, which also keeps channel * factor + 128 within 16 bits
ALWAYS_INLINE uint32_t UnpremultiplyChannel(uint32_t channel, uint32_t alpha, uint32_t factor) {
    return (std::min(channel, alpha) * factor + 128) >> 8;
}

ALWAYS_INLINE uint32_t UnpremultiplyPixel(uint32_t pixel) {
    uint32_t alpha = pixel >> 24;
    uint32_t factor = kUnpremultiplyFactors[alpha].rgba[0];

    return UnpremultiplyChannel(pixel & 0xFF, alpha, factor) |
           (UnpremultiplyChannel((pixel >> 8) & 0xFF, alpha, factor) << 8) |
           (UnpremultiplyChannel((pixel >> 16) & 0xFF, alpha, factor) << 16) |
           (pixel & 0xFF000000);
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
        uint8_t alpha = (WeightOf(src_alphas[i]) * color.a) >> 8;
        dest[i] = ColorRGBA(color, alpha);
    }
}

// Pixels are accessed as uint32_t: GCC doesn't vectorize in-place access through the ColorRGBA union
ALWAYS_INLINE void BlendColorToLine_Generic(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    uint32_t* __restrict pixels = reinterpret_cast<uint32_t*>(dest);
    for (size_t i = 0; i < width; i++) {
        pixels[i] = BlendColor(pixels[i], color.u32);
    }
}

ALWAYS_INLINE void BlendLine_Generic(ColorRGBA* __restrict dest, const ColorRGBA* __restrict src, size_t width) {
    uint32_t* __restrict dest_pixels = reinterpret_cast<uint32_t*>(dest);
    const uint32_t* __restrict src_pixels = reinterpret_cast<const uint32_t*>(src);
    for (size_t i = 0; i < width; i++) {
        dest_pixels[i] = BlendColor(dest_pixels[i], src_pixels[i]);
    }
}

ALWAYS_INLINE void BlendLine_PremultipliedSrc_Generic(ColorRGBA* __restrict dest,
                                                      const ColorRGBA* __restrict src, size_t width) {
    uint32_t* __restrict dest_pixels = reinterpret_cast<uint32_t*>(dest);
    const uint32_t* __restrict src_pixels = reinterpret_cast<const uint32_t*>(src);
    for (size_t i = 0; i < width; i++) {
        dest_pixels[i] = BlendColor_PremultipliedSrc(dest_pixels[i], src_pixels[i]);
    }
}

ALWAYS_INLINE void UnpremultiplyLine_Generic(ColorRGBA* __restrict dest, size_t width) {
    uint32_t* __restrict pixels = reinterpret_cast<uint32_t*>(dest);
    for (size_t i = 0; i < width; i++) {
        pixels[i] = UnpremultiplyPixel(pixels[i]);
    }
}

}  // namespace internal


}  // namespace aribcaption::alphablend

#endif  // ARIBCAPTION_ALPHABLEND_GENERIC_HPP
