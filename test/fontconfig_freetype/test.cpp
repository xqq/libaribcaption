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
#include <cstdlib>
#include <cstdio>
#include "renderer/alphablend.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "renderer/font_provider.hpp"
#include "renderer/font_provider_fontconfig.hpp"
#include "renderer/text_renderer.hpp"
#include "renderer/text_renderer_freetype.hpp"
#include "png_writer.hpp"

using namespace aribcaption;

constexpr int char_width = 36;
constexpr int char_height = 36;
constexpr int char_horizontal_spacing = 4;
constexpr int char_vertical_spacing = 24;
constexpr int section_width = char_width + char_horizontal_spacing;
constexpr int section_height = char_height + char_vertical_spacing;

constexpr bool draw_layout_box = false;
constexpr int scale_factor = 2;

int main(int argc, const char* argv[]) {
    Context context;
    context.SetLogcatCallback([](LogLevel level, const char* message) {
        if (level == LogLevel::kError) {
            fprintf(stderr, "%s\n", message);
        } else {
            printf("%s\n", message);
        }
    });

    FontProviderFontconfig font_provider(context);
    font_provider.Initialize();

    TextRendererFreetype text_renderer(context, font_provider);
    text_renderer.Initialize();
    // text_renderer.SetFontFamily({"Courier"});
    // text_renderer.SetFontFamily({"Monospace"});
    // text_renderer.SetFontFamily({"Monospace", "Hiragino Sans"});
    // text_renderer.SetFontFamily({"Hiragino Sans"});
    text_renderer.SetFontFamily({"Hiragino Maru Gothic ProN"});
    // text_renderer.SetFontFamily({"Monospace", "Rounded M+ 1m for ARIB"});
    // text_renderer.SetFontFamily({"YuGothic"});
    // text_renderer.SetFontFamily({"Noto Sans CJK JP"});
    // text_renderer.SetFontFamily({"sans-serif"});
    // text_renderer.SetFontFamily({"Zapfino"});

    // std::vector<uint32_t> chars = {U'1', U'1', U'4', U'5', U'1', U'4', U'!'};
    // std::vector<uint32_t> chars = {U'１', U'１', U'４', U'５', U'１', U'４', U'！'};
    // std::vector<uint32_t> chars = {U'A', U'a', U'B', U'g', U'b', U'p', U'f'};
    // std::vector<uint32_t> chars = {U'Ａ', U'ａ', U'Ｂ', U'ｂ', U'ｇ', U'ｐ', U'ｆ'};
    // std::vector<uint32_t> chars = {U'や', U'り', U'ま', U'す', U'ね', U'ぇ', U'。'};
    // std::vector<uint32_t> chars = {U'魑', U'魅', U'魍', U'魎', U'っ'};
    std::vector<uint32_t> chars = {U'い', U'い', U'よ', U'っ', U'！', U'こ', U'い', U'よ', U'っ', U'！'};

    Bitmap bitmap(section_width * chars.size() * scale_factor,
                  section_height * scale_factor,
                  PixelFormat::kRGBA8888);

    printf("bitmap size: width %d, height %d\n", bitmap.width(), bitmap.height());

    Canvas canvas(bitmap);
    TextRenderContext text_render_ctx = text_renderer.BeginDraw(bitmap);
    canvas.ClearColor(ColorRGBA(0, 0, 0, 60));

    for (size_t i = 0; i < chars.size(); i++) {
        uint32_t ch = chars[i];
        int section_x = section_width * (int)i * scale_factor;
        int x = (section_width * (int)i + (char_horizontal_spacing / 2)) * scale_factor;
        int y = (char_vertical_spacing / 2) * scale_factor;
        printf("x, y: %d, %d\n", x, y);

        CharStyle style = CharStyle::kCharStyleStroke;
        if (i == 2 || i == 3 || i == 5)
            style = static_cast<CharStyle>(style | CharStyle::kCharStyleUnderline);

        auto status = text_renderer.DrawChar(text_render_ctx,
                                             x,
                                             y,
                                             ch,
                                             style,
                                             ColorRGBA(  0, 255,   0, 255),
                                             ColorRGBA(  0,   0,   0, 180),
                                             3,
                                             0,  // char_width * scale_factor,
                                             char_height * scale_factor,
                                             UnderlineInfo{section_x, section_width * scale_factor},
                                             TextRenderFallbackPolicy::kAutoFallback);
        if (status != TextRenderStatus::kOK) {
            fprintf(stderr, "text_renderer.DrawChar returned error %d\n", static_cast<int>(status));
            return -1;
        }

        if (draw_layout_box) {
            ColorRGBA black(0, 0, 0, 255);
            Rect rect(x, y, x + char_width * scale_factor, y + char_height * scale_factor);
            canvas.DrawRect(black, Rect(rect.left, rect.top, rect.left + 1, rect.bottom));
            canvas.DrawRect(black, Rect(rect.right - 1, rect.top, rect.right, rect.bottom));
            canvas.DrawRect(black, Rect(rect.left, rect.top, rect.right, rect.top + 1));
            canvas.DrawRect(black, Rect(rect.left, rect.bottom - 1, rect.right, rect.bottom));
        }
    }

    text_renderer.EndDraw(text_render_ctx);

    if (!png_writer_write_bitmap("test_fontconfig_freetype_output.png", bitmap)) {
        fprintf(stderr, "write_png_file() failed\n");
        return -1;
    }

    Bitmap bmp(char_width * scale_factor, char_height * scale_factor, PixelFormat::kRGBA8888);
    text_render_ctx = text_renderer.BeginDraw(bmp);
    text_renderer.DrawChar(text_render_ctx,
                           0,
                           0,
                           U'獣',
                           CharStyle::kCharStyleStroke,
                           ColorRGBA(255, 255, 0, 255),
                           ColorRGBA(0, 0, 0, 220),
                           3,
                           char_width * scale_factor,
                           char_height * scale_factor,
                           std::nullopt,
                           TextRenderFallbackPolicy::kAutoFallback);
    text_renderer.EndDraw(text_render_ctx);

    if (!png_writer_write_bitmap("7363.png", bmp)) {
        fprintf(stderr, "write_png_file() failed\n");
        return -1;
    }

    return 0;
}
