/*
 * Copyright (C) 2026 Ruk Doe <info@doany.io>. All rights reserved.
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

// Renders synthetic DRCS patterns enlarged 2x / 4x and checks that the result is smoothed,
// i.e. that diagonal strokes don't turn into large staircases.

#include <cstdint>
#include <cstdio>
#include <functional>
#include "aribcaption/caption.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "renderer/drcs_renderer.hpp"
#include "png_writer.hpp"

using namespace aribcaption;

constexpr int glyph_size = 18;    // actual resolution of the synthetic glyphs
constexpr int pattern_size = 36;  // glyphs are sent pixel-doubled, which is common in broadcasts
constexpr int target_size = 72;   // 4x the glyph, roughly what a 1080p caption plane needs

static int failures = 0;

#define CHECK(cond)                                                                  \
    do {                                                                             \
        if (!(cond)) {                                                               \
            fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); \
            failures++;                                                              \
        }                                                                            \
    } while (0)

// Make a 1-bit DRCS pattern from a glyph_size x glyph_size picture, pixel-doubled to pattern_size
static DRCS MakePixelDoubledDRCS(const std::function<bool(int x, int y)>& on) {
    DRCS drcs;
    drcs.width = pattern_size;
    drcs.height = pattern_size;
    drcs.depth = 2;
    drcs.depth_bits = 1;
    drcs.pixels.assign(pattern_size * pattern_size / 8, 0);

    for (int y = 0; y < pattern_size; y++) {
        for (int x = 0; x < pattern_size; x++) {
            if (on(x * glyph_size / pattern_size, y * glyph_size / pattern_size)) {
                int i = y * pattern_size + x;
                drcs.pixels[i / 8] |= static_cast<uint8_t>(0x80 >> (i % 8));
            }
        }
    }

    return drcs;
}

static Bitmap RenderDRCS(const DRCS& drcs, int size) {
    Bitmap bitmap(size, size, PixelFormat::kRGBA8888);
    DRCSRenderer renderer;
    bool ret = renderer.DrawDRCS(drcs, kCharStyleDefault, ColorRGBA(255, 255, 255, 255), ColorRGBA(0, 0, 0, 255), 0,
                                 size, size, bitmap, 0, 0);
    CHECK(ret);
    return bitmap;
}

int main(int argc, const char* argv[]) {
    DRCS diagonal = MakePixelDoubledDRCS([](int x, int y) { return x == y; });
    DRCS ring = MakePixelDoubledDRCS([](int x, int y) {
        int dx = 2 * x + 1 - glyph_size;
        int dy = 2 * y + 1 - glyph_size;
        int d2 = dx * dx + dy * dy;
        return d2 >= 11 * 11 && d2 <= 16 * 16;
    });
    DRCS cross = MakePixelDoubledDRCS([](int x, int y) { return x == y || x == glyph_size - 1 - y; });
    DRCS filled = MakePixelDoubledDRCS([](int, int) { return true; });
    DRCS box = MakePixelDoubledDRCS([](int x, int y) { return x >= 4 && x < 14 && y >= 4 && y < 14; });

    // Write all glyphs side by side at 1x (36px) and 2x (72px) for visual inspection
    const DRCS* glyphs[] = {&diagonal, &ring, &cross, &box};
    constexpr int glyph_count = sizeof(glyphs) / sizeof(glyphs[0]);
    constexpr int margin = 8;
    Bitmap frame(margin + glyph_count * (target_size + margin), margin + pattern_size + margin + target_size + margin,
                 PixelFormat::kRGBA8888);
    Canvas canvas(frame);
    canvas.ClearColor(ColorRGBA(0, 0, 0, 255));
    for (int i = 0; i < glyph_count; i++) {
        int x = margin + i * (target_size + margin);
        canvas.DrawBitmap(RenderDRCS(*glyphs[i], pattern_size), x, margin);
        canvas.DrawBitmap(RenderDRCS(*glyphs[i], target_size), x, margin + pattern_size + margin);
    }
    png_writer_write_bitmap("test_drcs_smooth_output.png", frame);

    // A diagonal line: with nearest neighbour every glyph pixel becomes a 4x4 block, i.e. a 4px staircase.
    // After smoothing, the left edge of the stroke advances at most 2px per row and edges are anti-aliased.
    Bitmap line = RenderDRCS(diagonal, target_size);
    bool has_partial_alpha = false;
    int prev_left = -1;
    for (int y = 0; y < target_size; y++) {
        int left = -1;
        for (int x = 0; x < target_size; x++) {
            uint8_t alpha = line.GetPixelAt(x, y)->a;
            if (alpha > 0 && alpha < 250) {
                has_partial_alpha = true;
            }
            if (left < 0 && alpha >= 128) {
                left = x;
            }
        }
        CHECK(left >= 0);
        if (left >= 0 && prev_left >= 0) {
            CHECK(left - prev_left >= 0 && left - prev_left <= 2);
        }
        prev_left = left;
    }
    CHECK(has_partial_alpha);
    CHECK(line.GetPixelAt(0, target_size - 1)->a == 0);
    CHECK(line.GetPixelAt(target_size - 1, 0)->a == 0);

    // A filled pattern stays fully opaque, edges included
    Bitmap block = RenderDRCS(filled, target_size);
    CHECK(block.GetPixelAt(0, 0)->a >= 250);
    CHECK(block.GetPixelAt(target_size / 2, target_size / 2)->a >= 250);
    CHECK(block.GetPixelAt(target_size - 1, target_size - 1)->a >= 250);

    // Rendering at the pattern size keeps the edges of an axis-aligned box sharp, apart from a 1px anti-aliased
    // border. Scale2x rounds off convex corners, so the corners themselves are not checked.
    Bitmap same_size = RenderDRCS(box, pattern_size);
    for (int y = 0; y < pattern_size; y++) {
        for (int x = 0; x < pattern_size; x++) {
            bool inside = x >= 9 && x < 27 && y >= 9 && y < 27;
            bool corner = (x < 11 || x >= 25) && (y < 11 || y >= 25);
            bool outside = x < 7 || x >= 29 || y < 7 || y >= 29;
            uint8_t alpha = same_size.GetPixelAt(x, y)->a;
            if (inside && !corner) {
                CHECK(alpha >= 250);
            } else if (outside) {
                CHECK(alpha == 0);
            }
        }
    }

    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return -1;
    }

    printf("OK\n");
    return 0;
}
