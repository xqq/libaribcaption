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
#include <cstdio>
#include <memory>
#include "aribcaption/decoder.hpp"
#include "renderer/canvas.hpp"
#include "renderer/drcs_renderer.hpp"
#include "renderer/font_provider.hpp"
#include "renderer/font_provider_fontconfig.hpp"
#include "renderer/text_renderer.hpp"
#include "renderer/text_renderer_freetype.hpp"
#include "png_writer.hpp"

inline constexpr uint8_t test_data1[] = {
    0x80, 0xff, 0xf0, 0x04, 0x00, 0x00, 0x03, 0x4c, 0x3f, 0x00, 0x03, 0x48, 0x1f, 0x20, 0x00, 0x00, 0x01, 0x0c,
    0x1f, 0x30, 0x00, 0x02, 0x9e, 0x03, 0x41, 0x21, 0x01, 0x01, 0x02, 0x12, 0x24, 0x00, 0x00, 0xf0, 0x0f, 0x00,
    0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00,
    0x00, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00,
    0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00,
    0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00,
    0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00,
    0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00,
    0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00,
    0x0f, 0x00, 0xf0, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00,
    0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x41, 0x22, 0x01, 0x01, 0x02,
    0x12, 0x24, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0,
    0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f,
    0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f,
    0x00, 0xf0, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00,
    0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00,
    0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00,
    0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f,
    0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0,
    0x0f, 0x00, 0x00, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x0f, 0x00,
    0xf0, 0x00, 0x41, 0x23, 0x01, 0x01, 0x02, 0x24, 0x24, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
    0x00, 0x0f, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x00, 0x00, 0xf0, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00,
    0x00, 0x00, 0xf0, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0xff, 0xff, 0x0f, 0x00,
    0x00, 0x00, 0xf0, 0x0f, 0x0f, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x00,
    0x00, 0x00, 0xf0, 0x0f, 0x0f, 0x00, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0xff, 0xff, 0x0f, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0x0f, 0xff, 0xff, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00,
    0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xf0, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xf0, 0x00, 0x0f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0,
    0x00, 0x00, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x0f, 0x00, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00,
    0x00, 0x0f, 0x00, 0x00, 0x00, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x00, 0x00,
    0x00, 0x0f, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x00,
    0x00, 0x0f, 0x00, 0x00, 0xf0, 0x0f, 0x00, 0xf0, 0x00, 0x00, 0xf0, 0x00, 0xff, 0x00, 0x0f, 0x00, 0x0f, 0x00,
    0x00, 0xf0, 0x00, 0xff, 0x00, 0x0f, 0x00, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0x20, 0x00, 0x00, 0x9a, 0x9b, 0x37, 0x20, 0x53,
    0x9b, 0x31, 0x37, 0x30, 0x3b, 0x33, 0x30, 0x20, 0x5f, 0x9b, 0x36, 0x32, 0x30, 0x3b, 0x34, 0x38, 0x30, 0x20,
    0x56, 0x1d, 0x61, 0x9b, 0x33, 0x36, 0x3b, 0x33, 0x36, 0x20, 0x57, 0x9b, 0x34, 0x20, 0x58, 0x9b, 0x32, 0x34,
    0x20, 0x59, 0x89, 0x87, 0x90, 0x20, 0x44, 0x90, 0x51, 0x9b, 0x35, 0x31, 0x30, 0x3b, 0x34, 0x31, 0x39, 0x20,
    0x61, 0x1b, 0x2a, 0x20, 0x41, 0x1b, 0x7d, 0xa1, 0x8a, 0x0e, 0x50, 0x35, 0x50, 0x35, 0x43, 0x1b, 0x29, 0x4a,
    0x0e, 0x21, 0x89, 0xa2, 0x88, 0x90, 0x20, 0x40, 0x83, 0x9b, 0x33, 0x37, 0x30, 0x3b, 0x34, 0x34, 0x39, 0x20,
    0x61, 0x1b, 0x29, 0x30, 0x0e, 0x37, 0x69, 0x2d, 0x24, 0x73, 0x20, 0x6a, 0x6a, 0x41, 0x68, 0x89, 0x9b, 0x33,
    0x33, 0x30, 0x3b, 0x35, 0x30, 0x39, 0x20, 0x61, 0x1b, 0x29, 0x4a, 0x0e, 0x28, 0x8a, 0x0f, 0x47, 0x72, 0x35,
    0x34, 0x31, 0x21, 0xa3, 0x21, 0x39, 0x44, 0x33, 0x89, 0x0e, 0x29, 0xa1, 0x8a, 0x1b, 0x29, 0x30, 0x0e, 0x22,
    0x43, 0x0f, 0x21, 0x44, 0x89, 0xa2, 0x3d, 0xe2
};

constexpr bool draw_layout_box = false;
constexpr int scale_factor = 2;
constexpr bool force_stroke_text = true;
constexpr int stroke_width = 3;

using namespace aribcaption;

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
    text_renderer.SetFontFamily({"Hiragino Maru Gothic ProN"});
    // text_renderer.SetFontFamily({"Rounded M+ 1m for ARIB"});

    DRCSRenderer drcs_renderer;

    Decoder decoder(context);
    decoder.Initialize();

    DecodeResult result;

    auto status = decoder.Decode(test_data1, sizeof(test_data1), 0, result);

    if (status == DecodeStatus::kError) {
        fprintf(stderr, "Decoder::Decode() returned error\n");
        return -1;
    } else if (status == DecodeStatus::kNoCaption) {
        printf("kDecodeStatusNoCaption\n");
        return 0;
    }

    std::unique_ptr<Caption> caption = std::move(result.caption);

    Bitmap caption_frame(caption->plane_width * scale_factor,
                         caption->plane_height * scale_factor,
                         PixelFormat::kRGBA8888);
    Canvas caption_canvas(caption_frame);

    for (size_t i = 0; i < caption->regions.size(); i++) {
        CaptionRegion& region = caption->regions[i];
        Bitmap region_bmp(region.width * scale_factor,
                          region.height * scale_factor,
                          PixelFormat::kRGBA8888);
        Canvas canvas(region_bmp);
        TextRenderContext text_render_ctx = text_renderer.BeginDraw(region_bmp);

        for (CaptionChar& ch : region.chars) {
            // background
            int section_x = (ch.x - region.x) * scale_factor;
            int section_y = (ch.y - region.y) * scale_factor;
            canvas.ClearRect(ch.back_color,
                             Rect(section_x,
                                  section_y,
                                  section_x + ch.section_width() * scale_factor,
                                  section_y + ch.section_height() * scale_factor));

            int x = (int)((ch.x - region.x + (float)ch.char_horizontal_spacing * ch.char_horizontal_scale / 2) * scale_factor);
            int y = (int)((ch.y - region.y + (float)ch.char_vertical_spacing * ch.char_vertical_scale / 2) * scale_factor);
            int char_width = static_cast<int>((float)ch.char_width * ch.char_horizontal_scale * scale_factor);
            int char_height = static_cast<int>((float)ch.char_height * ch.char_vertical_scale * scale_factor);

            CharStyle style = ch.style;
            ColorRGBA stroke_color = ch.stroke_color;

            if (force_stroke_text) {
                style = static_cast<CharStyle>(ch.style | CharStyle::kCharStyleStroke);
                if (~(ch.style & CharStyle::kCharStyleStroke)) {
                    stroke_color = ch.back_color;
                }
            }

            if (ch.type == CaptionCharType::kDRCS
                || ch.type == CaptionCharType::kDRCSReplaced) {

                DRCS& drcs = caption->drcs_map[ch.drcs_code];
                bool ret = drcs_renderer.DrawDRCS(drcs, style, ch.text_color, stroke_color, stroke_width,
                                       char_width, char_height, region_bmp, x, y);
                if (!ret) {
                    fprintf(stderr, "drcs_renderer.DrawDRCS() returned error\n");
                }
            } else {
                auto render_status = text_renderer.DrawChar(text_render_ctx, x, y,
                                                            ch.codepoint, style, ch.text_color, stroke_color,
                                                            stroke_width, char_width, char_height,
                                                            std::nullopt, TextRenderFallbackPolicy::kAutoFallback);
                if (render_status != TextRenderStatus::kOK) {
                    fprintf(stderr, "canvas.DrawChar() returned error %d\n", static_cast<int>(render_status));
                }
            }

            if (draw_layout_box) {
                ColorRGBA black(0, 0, 0, 255);
                Rect rect(x, y, x + char_width, y + char_height);
                canvas.DrawRect(black, Rect(rect.left, rect.top, rect.left + 1, rect.bottom));
                canvas.DrawRect(black, Rect(rect.right - 1, rect.top, rect.right, rect.bottom));
                canvas.DrawRect(black, Rect(rect.left, rect.top, rect.right, rect.top + 1));
                canvas.DrawRect(black, Rect(rect.left, rect.bottom - 1, rect.right, rect.bottom));
            }
        }

        text_renderer.EndDraw(text_render_ctx);
        caption_canvas.DrawBitmap(region_bmp, region.x * scale_factor, region.y * scale_factor);
    }

    std::string filename("test_drcs_output.png");
    png_writer_write_bitmap(filename.c_str(), caption_frame);

    return 0;
}
