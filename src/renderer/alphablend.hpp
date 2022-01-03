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

#ifndef ARIBCAPTION_ALPHA_BLEND_HPP
#define ARIBCAPTION_ALPHA_BLEND_HPP

#include <cstddef>
#include <cstdint>
#include "aribcaption/color.hpp"
#include "base/always_inline.hpp"
#include "renderer/alphablend_generic.hpp"

namespace aribcaption::alphablend {

ALWAYS_INLINE void FillLine(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    internal::FillLine_Generic(dest, color, width);
}

ALWAYS_INLINE void BlendColorToLine(ColorRGBA* __restrict dest, ColorRGBA color, size_t width) {
    internal::BlendColorToLine_Generic(dest, color, width);
}

ALWAYS_INLINE void BlendLine(ColorRGBA* __restrict dest, const ColorRGBA* __restrict src, size_t width) {
    internal::BlendLine_Generic(dest, src, width);
}

}  // namespace aribcaption::alphablend

#endif  // ARIBCAPTION_ALPHA_BLEND_HPP
