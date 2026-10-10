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

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>
#include "renderer/alphablend.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "renderer/drcs_renderer.hpp"

namespace aribcaption {

namespace {

// 8-bit grey-scale copy of a DRCS pattern, used as the source for upscaling
struct GreyPattern {
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;

    // Out-of-range coordinates are clamped to the nearest edge pixel
    [[nodiscard]] uint8_t At(int x, int y) const {
        x = std::clamp(x, 0, width - 1);
        y = std::clamp(y, 0, height - 1);
        return pixels[static_cast<size_t>(y) * width + x];
    }
};

}  // namespace

static GreyPattern DecodeDRCSPattern(const DRCS& drcs) {
    GreyPattern pattern;
    pattern.width = drcs.width;
    pattern.height = drcs.height;
    pattern.pixels.resize(static_cast<size_t>(drcs.width) * drcs.height);

    for (size_t i = 0; i < pattern.pixels.size(); i++) {
        size_t index = i * drcs.depth_bits / 8;
        size_t bit_offset = i * drcs.depth_bits % 8;
        uint8_t byte = drcs.pixels[index];

        uint8_t value = (byte >> (8 - (bit_offset + drcs.depth_bits))) & (drcs.depth - 1);
        pattern.pixels[i] = alphablend::Clamp255((uint32_t)255 * value / (drcs.depth - 1));
    }

    return pattern;
}

// Many DRCS patterns are low resolution glyphs sent pixel-doubled, e.g. an 18x18 glyph as 36x36.
// If every 2x2 block of the pattern is a single color, halve it so that the smoothing sees the actual
// resolution of the glyph. This is lossless: enlarging the result 2x gives back the original pattern.
// Only one level is collapsed, so that blocky glyphs (e.g. thick strokes) are not over-smoothed.
static void CollapsePixelDoubling(GreyPattern& pattern) {
    if (pattern.width % 2 != 0 || pattern.height % 2 != 0) {
        return;
    }

    for (int y = 0; y < pattern.height; y += 2) {
        for (int x = 0; x < pattern.width; x += 2) {
            uint8_t p = pattern.At(x, y);
            if (pattern.At(x + 1, y) != p || pattern.At(x, y + 1) != p || pattern.At(x + 1, y + 1) != p) {
                return;
            }
        }
    }

    GreyPattern collapsed;
    collapsed.width = pattern.width / 2;
    collapsed.height = pattern.height / 2;
    collapsed.pixels.resize(static_cast<size_t>(collapsed.width) * collapsed.height);
    for (int y = 0; y < collapsed.height; y++) {
        for (int x = 0; x < collapsed.width; x++) {
            collapsed.pixels[static_cast<size_t>(y) * collapsed.width + x] = pattern.At(x * 2, y * 2);
        }
    }
    pattern = std::move(collapsed);
}

// Scale2x (EPX): enlarges the pattern by 2x, rounding off the staircases of diagonal strokes
static GreyPattern Scale2x(const GreyPattern& src) {
    GreyPattern dst;
    dst.width = src.width * 2;
    dst.height = src.height * 2;
    dst.pixels.resize(static_cast<size_t>(dst.width) * dst.height);

    for (int y = 0; y < src.height; y++) {
        for (int x = 0; x < src.width; x++) {
            uint8_t p = src.At(x, y);
            uint8_t a = src.At(x, y - 1);
            uint8_t b = src.At(x + 1, y);
            uint8_t c = src.At(x - 1, y);
            uint8_t d = src.At(x, y + 1);

            uint8_t* top = &dst.pixels[static_cast<size_t>(y * 2) * dst.width + x * 2];
            uint8_t* bottom = top + dst.width;
            top[0] = (c == a && c != d && a != b) ? a : p;
            top[1] = (a == b && a != c && b != d) ? b : p;
            bottom[0] = (d == c && d != b && c != a) ? c : p;
            bottom[1] = (b == d && b != a && d != c) ? d : p;
        }
    }

    return dst;
}

// Box-filtered (area-averaged) sampling of target pixel (x, y), approximated with 4x4 sub-samples.
// Used when the pattern is at least as large as the target, so that shrinking it anti-aliases the edges.
static uint8_t SampleArea(const GreyPattern& pattern, int x, int y, int target_width, int target_height) {
    constexpr int kSubSamples = 4;
    uint32_t sum = 0;

    for (int j = 0; j < kSubSamples; j++) {
        float fy = (static_cast<float>(y) + (static_cast<float>(j) + 0.5f) / kSubSamples) *
                   static_cast<float>(pattern.height) / static_cast<float>(target_height);
        for (int i = 0; i < kSubSamples; i++) {
            float fx = (static_cast<float>(x) + (static_cast<float>(i) + 0.5f) / kSubSamples) *
                       static_cast<float>(pattern.width) / static_cast<float>(target_width);
            sum += pattern.At(static_cast<int>(fx), static_cast<int>(fy));
        }
    }

    constexpr uint32_t count = kSubSamples * kSubSamples;
    return static_cast<uint8_t>((sum + count / 2) / count);
}

// Bilinear sampling at the center of target pixel (x, y)
static uint8_t SampleBilinear(const GreyPattern& pattern, int x, int y, int target_width, int target_height) {
    float sx = (static_cast<float>(x) + 0.5f) * static_cast<float>(pattern.width) / static_cast<float>(target_width);
    float sy = (static_cast<float>(y) + 0.5f) * static_cast<float>(pattern.height) / static_cast<float>(target_height);
    sx = std::clamp(sx - 0.5f, 0.0f, static_cast<float>(pattern.width - 1));
    sy = std::clamp(sy - 0.5f, 0.0f, static_cast<float>(pattern.height - 1));

    int x0 = static_cast<int>(sx);
    int y0 = static_cast<int>(sy);
    float fx = sx - static_cast<float>(x0);
    float fy = sy - static_cast<float>(y0);

    float top = pattern.At(x0, y0) * (1.0f - fx) + pattern.At(x0 + 1, y0) * fx;
    float bottom = pattern.At(x0, y0 + 1) * (1.0f - fx) + pattern.At(x0 + 1, y0 + 1) * fx;
    return static_cast<uint8_t>(std::lround(top * (1.0f - fy) + bottom * fy));
}

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

    if (drcs.width <= 0 || drcs.height <= 0 || drcs.depth < 2 || drcs.depth_bits <= 0 ||
        drcs.pixels.size() * 8 < static_cast<size_t>(drcs.width) * drcs.height * drcs.depth_bits) {
        return bitmap;
    }

    // Enlarging a DRCS pattern with nearest neighbour leaves large jaggies next to font-rendered glyphs.
    // Round off diagonal strokes with a single Scale2x pass (repeating it would also round off corners more
    // and more), then resample to the target size: bilinear when enlarging, box filter when shrinking.
    GreyPattern pattern = DecodeDRCSPattern(drcs);
    CollapsePixelDoubling(pattern);
    if (pattern.width < target_width || pattern.height < target_height) {
        pattern = Scale2x(pattern);
    }
    bool enlarge = pattern.width < target_width || pattern.height < target_height;

    for (int y = 0; y < target_height; y++) {
        ColorRGBA* dest = bitmap.GetPixelAt(0, y);
        for (int x = 0; x < target_width; x++) {
            uint8_t grey = enlarge ? SampleBilinear(pattern, x, y, target_width, target_height)
                                   : SampleArea(pattern, x, y, target_width, target_height);

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
