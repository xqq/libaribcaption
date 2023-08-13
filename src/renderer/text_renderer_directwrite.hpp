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

#ifndef ARIBCAPTION_TEXT_RENDERER_DIRECTWRITE_HPP
#define ARIBCAPTION_TEXT_RENDERER_DIRECTWRITE_HPP

#include <wrl/client.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <optional>
#include "aribcaption/context.hpp"
#include "base/scoped_com_initializer.hpp"
#include "renderer/font_provider.hpp"
#include "renderer/text_renderer.hpp"

using Microsoft::WRL::ComPtr;

namespace aribcaption {

class TextRendererDirectWrite : public TextRenderer {
public:
    TextRendererDirectWrite(Context& context, FontProvider& font_provider);
    ~TextRendererDirectWrite() override;
public:
    bool Initialize() override;
    void SetLanguage(uint32_t iso6392_language_code) override;
    bool SetFontFamily(const std::vector<std::string>& font_family) override;
    auto BeginDraw(Bitmap& target_bmp) -> TextRenderContext override;
    void EndDraw(TextRenderContext& context) override;
    auto DrawChar(TextRenderContext& render_ctx, int x, int y,
                  uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                  float stroke_width, int char_width, int char_height, float aspect_ratio,
                  std::optional<UnderlineInfo> underline_info,
                  TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus override;
private:
    auto LoadDWriteFont(std::optional<uint32_t> codepoint = std::nullopt,
                        std::optional<size_t> begin_index = std::nullopt)
        -> Result<std::pair<FontfaceInfo, size_t>, FontProviderError>;
    auto CreateDWriteTextFormat(FontfaceInfo& face_info, int font_size) -> ComPtr<IDWriteTextFormat>;
    auto CreateWICRenderTarget(IWICBitmap* target) -> ComPtr<ID2D1RenderTarget>;
    bool BlendWICBitmapToBitmap(IWICBitmap* wic_bitmap, Bitmap& target_bmp,
                                int target_x, int target_y);
private:
    static D2D1_COLOR_F RGBAToD2DColor(ColorRGBA color);
    static bool FontfaceHasCharacter(FontfaceInfo& fontface, uint32_t ucs4);
private:
    std::shared_ptr<Logger> log_;

    FontProvider& font_provider_;
    uint32_t iso6392_language_code_ = 0;
    std::vector<std::string> font_family_;

    ScopedCOMInitializer com_initializer_;
    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<IWICImagingFactory> wic_factory_;
    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<ID2D1StrokeStyle> stroke_style_;

    size_t main_face_index_ = 0;

    std::optional<FontfaceInfo> main_faceinfo_;
    std::optional<FontfaceInfo> fallback_faceinfo_;

    int main_text_format_pixel_height_ = 0;
    int fallback_text_format_pixel_height_ = 0;

    ComPtr<IDWriteTextFormat> main_text_format_;
    ComPtr<IDWriteTextFormat> fallback_text_format_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_TEXT_RENDERER_DIRECTWRITE_HPP
