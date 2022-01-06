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

#ifndef ARIBCAPTION_ALPHABLEND_X86_HPP
#define ARIBCAPTION_ALPHABLEND_X86_HPP

#include <xmmintrin.h>  // SSE
#include <emmintrin.h>  // SSE2
#include <pmmintrin.h>  // SSE3
#include <tmmintrin.h>  // SSSE3
#include "renderer/alphablend_generic.hpp"

namespace aribcaption::alphablend::internal {

namespace x86 {

union alignas(16) Vec128i {
    uint8_t u8[16];
    __m128i m128i;
};

ALWAYS_INLINE void FillLine_SSE2(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    if (uint32_t unaligned_prefix_pixels = (reinterpret_cast<uintptr_t>(dest) % 16) / 4) {
        uint32_t unaligned_pixels = 4 - unaligned_prefix_pixels;
        FillLine_Generic(dest, color, unaligned_pixels);
        dest += unaligned_pixels;
        width -= unaligned_pixels;
    }

    uint32_t tail_remain_pixels = 0;
    if ((tail_remain_pixels = width % 4) != 0) {
        width -= tail_remain_pixels;
    }

    for (size_t i = 0; i < width; i += 4, dest += 4) {
        __m128i color4 = _mm_set1_epi32(static_cast<int>(color.u32));
        _mm_store_si128(reinterpret_cast<__m128i*>(dest), color4);
    }

    if (tail_remain_pixels) {
        FillLine_Generic(dest, color, tail_remain_pixels);
    }
}

ALWAYS_INLINE void FillLineWithAlphas_SSE2(ColorRGBA* __restrict dest,
                                           const uint8_t* __restrict src, ColorRGBA color, size_t width) {
    //            RGBA_0xAABBGGRR
    const __m128i mask_0xffffffff = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    const __m128i mask_0xff000000 = _mm_slli_epi32(mask_0xffffffff, 24);
    const __m128i mask_0x00ffffff = _mm_srli_epi32(mask_0xffffffff, 8);

    uint32_t tail_remain_pixels = 0;
    if ((tail_remain_pixels = width % 4) != 0) {
        width -= tail_remain_pixels;
    }

    __m128i color4 = _mm_set1_epi32(static_cast<int>(color.u32));
    __m128i color4_rgb = _mm_and_si128(color4, mask_0x00ffffff);
    __m128i color4_alpha = _mm_srli_epi32(_mm_and_si128(color4, mask_0xff000000), 8);

    for (size_t i = 0; i < width; i += 4, dest += 4, src += 4) {
        __m128i alpha4 = _mm_cvtsi32_si128(*reinterpret_cast<const int*>(src));
        alpha4 = _mm_unpacklo_epi8(alpha4, _mm_setzero_si128());
        alpha4 = _mm_unpacklo_epi8(alpha4, _mm_setzero_si128());
        alpha4 = _mm_slli_epi32(alpha4, 16);

        __m128i weighted_alpha = _mm_and_si128(mask_0xff000000, _mm_mullo_epi16(color4_alpha, alpha4));

        __m128i result = _mm_or_si128(color4_rgb, weighted_alpha);

        _mm_storeu_si128(reinterpret_cast<__m128i*>(dest), result);
    }

    if (tail_remain_pixels) {
        FillLineWithAlphas_Generic(dest, src, color, tail_remain_pixels);
    }
}

constexpr Vec128i ssse3_mask_extract_alpha = {
    0x03, 0x80, 0x03, 0x80, 0x07, 0x80, 0x07, 0x80,
    0x0b, 0x80, 0x0b, 0x80, 0x0f, 0x80, 0x0f, 0x80
};

ALWAYS_INLINE void BlendColorToLine_SSSE3(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    //            RGBA_0xAABBGGRR
    const __m128i mask_0xffffffff = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    const __m128i mask_0xff000000 = _mm_slli_epi32(mask_0xffffffff, 24);
    const __m128i mask_0x00ff00ff = _mm_srli_epi16(mask_0xffffffff, 8);
    const __m128i mask_0xff00ff00 = _mm_slli_epi16(mask_0xffffffff, 8);
    const __m128i mask_0x0000ff00 = _mm_srli_epi32(mask_0xff000000, 16);
    const __m128i value_0x01010101 = _mm_xor_si128(_mm_add_epi8(mask_0xffffffff, mask_0xffffffff), mask_0xffffffff);
    const __m128i value_0x01000000 = _mm_and_si128(value_0x01010101, mask_0xff000000);

    __m128i src = _mm_set1_epi32(static_cast<int>(color.u32));
    __m128i src_alpha = _mm_shuffle_epi8(src, ssse3_mask_extract_alpha.m128i);
    __m128i src_ff_minus_alpha = _mm_xor_si128(src_alpha, mask_0x00ff00ff);

    // 0x00BB00RR
    __m128i src_b_r = _mm_and_si128(src, mask_0x00ff00ff);
    src_b_r = _mm_mullo_epi16(src_b_r, src_alpha);
    src_b_r = _mm_srli_epi16(src_b_r, 8);

    // 0x010000GG
    __m128i src_1_g = _mm_or_si128(value_0x01000000, _mm_srli_epi32(_mm_and_si128(src, mask_0x0000ff00), 8));
    // 0xAA00GG00
    __m128i src_a_g = _mm_and_si128(mask_0xff00ff00, _mm_mullo_epi16(src_1_g, src_alpha));

    __m128i premultiplied_src = _mm_or_si128(src_b_r, src_a_g);


    if (uint32_t unaligned_prefix_pixels = (reinterpret_cast<uintptr_t>(dest) % 16) / 4) {
        uint32_t unaligned_pixels = 4 - unaligned_prefix_pixels;

        for (uint32_t i = 0; i < unaligned_pixels; i++) {
            __m128i dst = _mm_cvtsi32_si128(static_cast<int>(dest[i].u32));

            __m128i dst_b_r = _mm_and_si128(dst, mask_0x00ff00ff);
            dst_b_r = _mm_mullo_epi16(dst_b_r, src_ff_minus_alpha);
            dst_b_r = _mm_srli_epi16(dst_b_r, 8);

            __m128i dst_a_g = _mm_srli_epi16(dst, 8);
            dst_a_g = _mm_mullo_epi16(dst_a_g, src_ff_minus_alpha);
            dst_a_g = _mm_and_si128(dst_a_g, mask_0xff00ff00);

            __m128i result = _mm_adds_epu8(premultiplied_src, _mm_or_si128(dst_b_r, dst_a_g));
            dest[i].u32 = _mm_cvtsi128_si32(result);
        }

        dest += unaligned_pixels;
        width -= unaligned_pixels;
    }

    uint32_t tail_remain_pixels = 0;
    if ((tail_remain_pixels = width % 4) != 0) {
        width -= tail_remain_pixels;
    }

    for (size_t i = 0; i < width; i += 4, dest += 4) {
        __m128i dst = _mm_load_si128(reinterpret_cast<__m128i*>(dest));

        __m128i dst_b_r = _mm_and_si128(dst, mask_0x00ff00ff);
        dst_b_r = _mm_mullo_epi16(dst_b_r, src_ff_minus_alpha);
        dst_b_r = _mm_srli_epi16(dst_b_r, 8);

        __m128i dst_a_g = _mm_srli_epi16(dst, 8);
        dst_a_g = _mm_mullo_epi16(dst_a_g, src_ff_minus_alpha);
        dst_a_g = _mm_and_si128(dst_a_g, mask_0xff00ff00);

        __m128i result = _mm_adds_epu8(premultiplied_src, _mm_or_si128(dst_b_r, dst_a_g));
        _mm_store_si128(reinterpret_cast<__m128i*>(dest), result);
    }

    for (uint32_t i = 0; i < tail_remain_pixels; i++) {
        __m128i dst = _mm_cvtsi32_si128(static_cast<int>(dest[i].u32));

        __m128i dst_b_r = _mm_and_si128(dst, mask_0x00ff00ff);
        dst_b_r = _mm_mullo_epi16(dst_b_r, src_ff_minus_alpha);
        dst_b_r = _mm_srli_epi16(dst_b_r, 8);

        __m128i dst_a_g = _mm_srli_epi16(dst, 8);
        dst_a_g = _mm_mullo_epi16(dst_a_g, src_ff_minus_alpha);
        dst_a_g = _mm_and_si128(dst_a_g, mask_0xff00ff00);

        __m128i result = _mm_adds_epu8(premultiplied_src, _mm_or_si128(dst_b_r, dst_a_g));
        dest[i].u32 = _mm_cvtsi128_si32(result);
    }
}

ALWAYS_INLINE void BlendLine_SSSE3(ColorRGBA* __restrict dest, const ColorRGBA* __restrict source, size_t width) {
    //            RGBA_0xAABBGGRR
    const __m128i mask_0xffffffff = _mm_cmpeq_epi8(_mm_setzero_si128(), _mm_setzero_si128());
    const __m128i mask_0xff000000 = _mm_slli_epi32(mask_0xffffffff, 24);
    const __m128i mask_0x00ff00ff = _mm_srli_epi16(mask_0xffffffff, 8);
    const __m128i mask_0xff00ff00 = _mm_slli_epi16(mask_0xffffffff, 8);
    const __m128i mask_0x0000ff00 = _mm_srli_epi32(mask_0xff000000, 16);
    const __m128i value_0x01010101 = _mm_xor_si128(_mm_add_epi8(mask_0xffffffff, mask_0xffffffff), mask_0xffffffff);
    const __m128i value_0x01000000 = _mm_and_si128(value_0x01010101, mask_0xff000000);

    uint32_t tail_remain_pixels = 0;
    if ((tail_remain_pixels = width % 4) != 0) {
        width -= tail_remain_pixels;
    }

    for (size_t i = 0; i < width; i += 4, source += 4, dest += 4) {
        __m128i src = _mm_loadu_si128(reinterpret_cast<const __m128i*>(source));
        __m128i src_alpha = _mm_shuffle_epi8(src, ssse3_mask_extract_alpha.m128i);
        __m128i src_ff_minus_alpha = _mm_xor_si128(src_alpha, mask_0x00ff00ff);

        // 0x00BB00RR
        __m128i src_b_r = _mm_and_si128(src, mask_0x00ff00ff);
        src_b_r = _mm_mullo_epi16(src_b_r, src_alpha);
        src_b_r = _mm_srli_epi16(src_b_r, 8);

        // 0x010000GG
        __m128i src_1_g = _mm_or_si128(value_0x01000000, _mm_srli_epi32(_mm_and_si128(src, mask_0x0000ff00), 8));
        // 0xAA00GG00
        __m128i src_a_g = _mm_and_si128(mask_0xff00ff00, _mm_mullo_epi16(src_1_g, src_alpha));

        __m128i multiplied_src = _mm_or_si128(src_b_r, src_a_g);


        __m128i dst = _mm_loadu_si128(reinterpret_cast<__m128i*>(dest));

        __m128i dst_b_r = _mm_and_si128(dst, mask_0x00ff00ff);
        dst_b_r = _mm_mullo_epi16(dst_b_r, src_ff_minus_alpha);
        dst_b_r = _mm_srli_epi16(dst_b_r, 8);

        __m128i dst_a_g = _mm_srli_epi16(dst, 8);
        dst_a_g = _mm_mullo_epi16(dst_a_g, src_ff_minus_alpha);
        dst_a_g = _mm_and_si128(dst_a_g, mask_0xff00ff00);

        __m128i result = _mm_adds_epu8(multiplied_src, _mm_or_si128(dst_b_r, dst_a_g));
        _mm_storeu_si128(reinterpret_cast<__m128i*>(dest), result);
    }

    for (uint32_t i = 0; i < tail_remain_pixels; i++) {
        __m128i src = _mm_cvtsi32_si128(static_cast<int>(source[i].u32));
        __m128i src_alpha = _mm_shuffle_epi8(src, ssse3_mask_extract_alpha.m128i);
        __m128i src_ff_minus_alpha = _mm_xor_si128(src_alpha, mask_0x00ff00ff);

        // 0x00BB00RR
        __m128i src_b_r = _mm_and_si128(src, mask_0x00ff00ff);
        src_b_r = _mm_mullo_epi16(src_b_r, src_alpha);
        src_b_r = _mm_srli_epi16(src_b_r, 8);

        // 0x010000GG
        __m128i src_1_g = _mm_or_si128(value_0x01000000, _mm_srli_epi32(_mm_and_si128(src, mask_0x0000ff00), 8));
        // 0xAA00GG00
        __m128i src_a_g = _mm_and_si128(mask_0xff00ff00, _mm_mullo_epi16(src_1_g, src_alpha));

        __m128i multiplied_src = _mm_or_si128(src_b_r, src_a_g);

        __m128i dst = _mm_cvtsi32_si128(static_cast<int>(dest[i].u32));

        __m128i dst_b_r = _mm_and_si128(dst, mask_0x00ff00ff);
        dst_b_r = _mm_mullo_epi16(dst_b_r, src_ff_minus_alpha);
        dst_b_r = _mm_srli_epi16(dst_b_r, 8);

        __m128i dst_a_g = _mm_srli_epi16(dst, 8);
        dst_a_g = _mm_mullo_epi16(dst_a_g, src_ff_minus_alpha);
        dst_a_g = _mm_and_si128(dst_a_g, mask_0xff00ff00);

        __m128i result = _mm_adds_epu8(multiplied_src, _mm_or_si128(dst_b_r, dst_a_g));
        dest[i].u32 = _mm_cvtsi128_si32(result);
    }
}

}  // namespace x86


ALWAYS_INLINE void FillLine_x86(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    x86::FillLine_SSE2(dest, color, width);
}

ALWAYS_INLINE void FillLineWithAlphas_x86(ColorRGBA* __restrict dest,
                                          const uint8_t* __restrict src_alphas, ColorRGBA color, size_t width) {
    x86::FillLineWithAlphas_SSE2(dest, src_alphas, color, width);
}

ALWAYS_INLINE void BlendColorToLine_x86(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    x86::BlendColorToLine_SSSE3(dest, color, width);
}

ALWAYS_INLINE void BlendLine_x86(ColorRGBA* __restrict dest, const ColorRGBA* __restrict src, size_t width) {
    x86::BlendLine_SSSE3(dest, src, width);
}


}  // namespace aribcaption::alphablend::internal

#endif  // ARIBCAPTION_ALPHABLEND_X86_HPP
