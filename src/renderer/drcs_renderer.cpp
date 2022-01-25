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

#include <cstdint>
#include "renderer/alphablend.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "renderer/drcs_renderer.hpp"

namespace aribcaption {

bool DRCSRenderer::DrawDRCS(const DRCS& drcs, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                            int stroke_width, int target_width, int target_height,
                            Bitmap& target_bmp, int target_x, int target_y) {
    if (drcs.width == 0 || drcs.height == 0 || drcs.pixels.empty()) {
        return false;
    }

    Canvas canvas(target_bmp);

    // Draw stroke (border) if needed
    if (style & CharStyle::kCharStyleStroke) {
        Bitmap stroke_bitmap = DRCSToColoredBitmap(drcs, target_width, target_height, stroke_color);

        canvas.DrawBitmap(stroke_bitmap, target_x - stroke_width, target_y);
        canvas.DrawBitmap(stroke_bitmap, target_x + stroke_width, target_y);
        canvas.DrawBitmap(stroke_bitmap, target_x, target_y - stroke_width);
        canvas.DrawBitmap(stroke_bitmap, target_x, target_y + stroke_width);
    }

    // Draw DRCS with text color
    Bitmap text_bitmap = DRCSToColoredBitmap(drcs, target_width, target_height, color);
    canvas.DrawBitmap(text_bitmap, target_x, target_y);

    return true;
}

Bitmap DRCSRenderer::DRCSToColoredBitmap(const DRCS& drcs, int target_width, int target_height, ColorRGBA color) {
    Bitmap bitmap(target_width, target_height, PixelFormat::kRGBA8888);

    float x_fraction = static_cast<float>(drcs.width) / static_cast<float>(target_width);
    float y_fraction = static_cast<float>(drcs.height) / static_cast<float>(target_height);

    for (int y = 0; y < target_height; y++) {
        ColorRGBA* dest = bitmap.GetPixelAt(0, y);
        int drcs_y = static_cast<int>(y_fraction * static_cast<float>(y));
        for (int x = 0; x < target_width; x++) {
            int drcs_x = static_cast<int>(x_fraction * static_cast<float>(x));

            intptr_t index = (drcs_y * drcs.width + drcs_x) * drcs.depth_bits / 8;
            intptr_t bit_offset = (drcs_y * drcs.width + drcs_x) * drcs.depth_bits % 8;
            uint8_t byte = drcs.pixels[index];

            uint8_t value = (byte >> (8 - (bit_offset + drcs.depth_bits))) & (drcs.depth - 1);
            uint8_t grey = alphablend::Clamp255((uint32_t)255 * value / (drcs.depth - 1));

            if (grey) {
                uint8_t alpha = (static_cast<uint32_t>(grey) * color.a) >> 8;
                dest[x] = ColorRGBA(color, alpha);
            } else {
                dest[x] = ColorRGBA(0);
            }
        }
    }

    return bitmap;
}

}  // namespace aribcaption
