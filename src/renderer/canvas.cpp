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

#include <cassert>
#include "renderer/alpha_blend.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "renderer/text_renderer.hpp"

namespace aribcaption {

Canvas::Canvas(Bitmap& target_bitmap) : bitmap_(target_bitmap) {}

Canvas::~Canvas() = default;

void Canvas::SetTextRenderer(TextRenderer& text_renderer) {
    text_renderer_ = &text_renderer;
}

void Canvas::ClearColor(ColorRGBA color) {
    for (int y = 0; y < bitmap_.height(); y++) {
        for (int x = 0; x < bitmap_.width(); x++) {
            ColorRGBA* pixel = bitmap_.GetPixelAt(x, y);
            *pixel = color;
        }
    }
}

void Canvas::ClearRect(ColorRGBA color, const Rect& rect) {
    Rect clipped = rect;

    if (clipped.left < 0)
        clipped.left = 0;
    if (clipped.top < 0)
        clipped.top = 0;
    if (clipped.right > bitmap_.width())
        clipped.right = bitmap_.width();
    if (clipped.bottom > bitmap_.height())
        clipped.bottom = bitmap_.height();

    if (clipped.width() <= 0 || clipped.height() <= 0) {
        return;
    }

    for (int y = clipped.top; y < clipped.bottom; y++) {
        for (int x = clipped.left; x < clipped.right; x++) {
            ColorRGBA* dest = bitmap_.GetPixelAt(x, y);
            *dest = color;
        }
    }
}

void Canvas::DrawRect(ColorRGBA fg_color, const Rect& rect) {
    Rect clipped = rect;

    if (clipped.left < 0)
        clipped.left = 0;
    if (clipped.top < 0)
        clipped.top = 0;
    if (clipped.right > bitmap_.width())
        clipped.right = bitmap_.width();
    if (clipped.bottom > bitmap_.height())
        clipped.bottom = bitmap_.height();

    if (clipped.width() <= 0 || clipped.height() <= 0) {
        return;
    }

    for (int y = clipped.top; y < clipped.bottom; y++) {
        for (int x = clipped.left; x < clipped.right; x++) {
            ColorRGBA* dest = bitmap_.GetPixelAt(x, y);
            ColorRGBA bg_color = *dest;
            *dest = alphablend::BlendColor(bg_color, fg_color);
        }
    }
}

void Canvas::DrawBitmap(const Bitmap& bmp, const Rect& rect) {
    assert(bmp.width() == rect.width() && bmp.height() == rect.height());

    Rect clipped = rect;

    if (clipped.left < 0)
        clipped.left = 0;
    if (clipped.top < 0)
        clipped.top = 0;
    if (clipped.right > bitmap_.width())
        clipped.right = bitmap_.width();
    if (clipped.bottom > bitmap_.height())
        clipped.bottom = bitmap_.height();

    if (clipped.width() <= 0 || clipped.height() <= 0) {
        return;
    }

    for (int y = clipped.top; y < clipped.bottom; y++) {
        for (int x = clipped.left; x < clipped.right; x++) {
            ColorRGBA* dest = bitmap_.GetPixelAt(x, y);
            const ColorRGBA* src = bmp.GetPixelAt(x - clipped.left, y - clipped.top);
            ColorRGBA bg_color = *dest;  // background (dest), the canvas
            ColorRGBA fg_color = *src;   // foreground (src), the input bitmap
            *dest = alphablend::BlendColor(bg_color, fg_color);
        }
    }
}

auto Canvas::DrawChar(uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color, int stroke_width,
                      int char_width, int char_height, int x, int y,
                      std::optional<UnderlineInfo> underline_info) -> TextRenderStatus {
    assert(text_renderer_);
    return text_renderer_->DrawChar(ucs4, style, color, stroke_color,
                                    stroke_width,
                                    char_width,
                                    char_height,
                                    bitmap_,
                                    x,
                                    y,
                                    underline_info);
}

}  // namespace aribcaption
