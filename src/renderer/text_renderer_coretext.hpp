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

#ifndef ARIBCAPTION_TEXT_RENDERER_CORETEXT_HPP
#define ARIBCAPTION_TEXT_RENDERER_CORETEXT_HPP

#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
    #include <CoreText/CoreText.h>
    #include <CoreGraphics/CoreGraphics.h>
#else
    #include <ApplicationServices/ApplicationServices.h>
#endif
#include <vector>
#include <string>
#include <memory>
#include <optional>
#include "aribcaption/context.hpp"
#include "base/logger.hpp"
#include "base/scoped_cfref.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/font_provider.hpp"
#include "renderer/text_renderer.hpp"

namespace aribcaption {

class TextRendererCoreText : public TextRenderer {
public:
    TextRendererCoreText(Context& context, FontProvider& font_provider);
    ~TextRendererCoreText() override;
public:
    bool Initialize() override;
    void SetLanguage(uint32_t iso6392_language_code) override;
    bool SetFontFamily(const std::vector<std::string>& font_family) override;
    auto BeginDraw(Bitmap& target_bmp) -> TextRenderContext override;
    void EndDraw(TextRenderContext& context) override;
    auto DrawChar(TextRenderContext& render_ctx, int x, int y,
                  uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                  float stroke_width, int char_width, int char_height,
                  std::optional<UnderlineInfo> underline_info,
                  TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus override;
private:
    auto LoadCTFont(std::optional<uint32_t> codepoint = std::nullopt,
                    std::optional<size_t> begin_index = std::nullopt)
                    -> Result<std::pair<ScopedCFRef<CTFontRef>, size_t>, FontProviderError>;
private:
    static auto RGBAToCGColor(ColorRGBA rgba) -> ScopedCFRef<CGColorRef>;
    static auto CreateBitmapTargetCGContext(Bitmap& bitmap) -> ScopedCFRef<CGContextRef>;
    static auto CreateSizedCTFont(CTFontRef ctfont, int char_height) -> ScopedCFRef<CTFontRef>;
private:
    std::shared_ptr<Logger> log_;

    FontProvider& font_provider_;
    std::vector<std::string> font_family_;

    size_t main_face_index_ = 0;

    ScopedCFRef<CTFontRef> main_ctfont_;
    ScopedCFRef<CTFontRef> fallback_ctfont_;

    int main_ctfont_pixel_height_ = 0;
    int fallback_ctfont_pixel_height_ = 0;

    ScopedCFRef<CTFontRef> main_ctfont_sized_;
    ScopedCFRef<CTFontRef> fallback_ctfont_sized_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_TEXT_RENDERER_CORETEXT_HPP
