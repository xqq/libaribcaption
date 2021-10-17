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
#include "renderer/alpha_blend.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/drcs_renderer.hpp"

namespace aribcaption {

bool DRCSRenderer::DrawDRCS(const DRCS& drcs, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                            int stroke_width, int target_width, int target_height,
                            Bitmap& target_bmp, int target_x, int target_y) {
    if (drcs.width == 0 || drcs.height == 0 || drcs.pixels.empty()) {
        return false;
    }

    // Draw stroke (border) if needed
    if (style & CharStyle::kCharStyleStroke) {
        RenderDRCS(drcs, stroke_color, target_width, target_height, target_bmp, target_x - stroke_width, target_y);
        RenderDRCS(drcs, stroke_color, target_width, target_height, target_bmp, target_x + stroke_width, target_y);
        RenderDRCS(drcs, stroke_color, target_width, target_height, target_bmp, target_x, target_y - stroke_width);
        RenderDRCS(drcs, stroke_color, target_width, target_height, target_bmp, target_x, target_y + stroke_width);
    }

    // Draw DRCS with text color
    RenderDRCS(drcs, color, target_width, target_height, target_bmp, target_x, target_y);

    return true;
}

void DRCSRenderer::RenderDRCS(const DRCS& drcs, ColorRGBA color, int target_width, int target_height,
                              Bitmap& target_bmp, int target_x, int target_y) {
    float x_fraction = static_cast<float>(drcs.width) / static_cast<float>(target_width);
    float y_fraction = static_cast<float>(drcs.height) / static_cast<float>(target_height);

    // Draw DRCS with specific color
    // Nearest-neighbor scaling
    for (int y = 0; y < target_height; y++) {
        bool should_exit = false;
        for (int x = 0; x < target_width; x++) {
            int drcs_x = static_cast<int>(x_fraction * static_cast<float>(x));
            int drcs_y = static_cast<int>(y_fraction * static_cast<float>(y));

            intptr_t index = (drcs_y * drcs.width + drcs_x) * drcs.depth_bits / 8;
            intptr_t bit_offset = (drcs_y * drcs.width + drcs_x) * drcs.depth_bits % 8;
            uint8_t byte = drcs.pixels[index];

            uint8_t value = (byte >> (8 - (bit_offset + drcs.depth_bits))) & (drcs.depth - 1);
            uint8_t alpha = alphablend::Clamp255((uint32_t)255 * value / (drcs.depth - 1));
            if (alpha == 0)
                continue;

            int dst_x = target_x + x;
            int dst_y = target_y + y;

            if (dst_x < 0 || dst_y < 0) {
                continue;
            } else if (dst_x >= target_bmp.width()) {
                break;
            } else if (dst_y >= target_bmp.height()) {
                should_exit = true;
                break;
            }

            ColorRGBA* dst_addr = target_bmp.GetPixelAt(dst_x, dst_y);
            ColorRGBA bg_color = *dst_addr;
            ColorRGBA fg_color = color;
            fg_color.a = alphablend::Div255(static_cast<uint32_t>(fg_color.a) * alpha);

            *dst_addr = alphablend::BlendColor(bg_color, fg_color);
        }
        if (should_exit)
            break;
    }
}

}  // namespace aribcaption
