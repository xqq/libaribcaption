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

#ifndef ARIBCAPTION_CANVAS_HPP
#define ARIBCAPTION_CANVAS_HPP

#include <optional>
#include "aribcaption/caption.hpp"
#include "aribcaption/color.hpp"
#include "renderer/rect.hpp"

namespace aribcaption {

class Bitmap;

class Canvas {
public:
    explicit Canvas(Bitmap& target_bitmap);
    ~Canvas();
public:
    void ClearColor(ColorRGBA color);
    void ClearRect(ColorRGBA color, const Rect& rect);
    void DrawRect(ColorRGBA color, const Rect& rect);
    void DrawBitmap(const Bitmap& bmp, const Rect& rect);
    void DrawBitmap(const Bitmap& bmp, int target_x, int target_y);
public:
    // Disallow copy and assign
    Canvas(const Canvas&) = delete;
    Canvas& operator=(const Canvas&) = delete;
private:
    Bitmap& bitmap_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_CANVAS_HPP
