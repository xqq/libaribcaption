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

#ifndef ARIBCAPTION_DRCS_RENDERER_HPP
#define ARIBCAPTION_DRCS_RENDERER_HPP

#include "aribcaption/caption.hpp"
#include "aribcaption/color.hpp"

namespace aribcaption {

class Bitmap;

class DRCSRenderer {
public:
    DRCSRenderer() = default;
    ~DRCSRenderer() = default;
public:
    bool DrawDRCS(const DRCS& drcs, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                  int stroke_width, int char_width, int char_height,
                  Bitmap& target_bmp, int x, int y);
private:
    static Bitmap DRCSToColoredBitmap(const DRCS& drcs, int target_width, int target_height, ColorRGBA color);
public:
    DRCSRenderer(const DRCSRenderer&) = delete;
    DRCSRenderer& operator=(const DRCSRenderer&) = delete;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_DRCS_RENDERER_HPP
