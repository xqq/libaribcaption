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
#include <unordered_set>
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
                  float stroke_width, int char_width, int char_height, float aspect_ratio,
                  std::optional<UnderlineInfo> underline_info,
                  TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus override;
private:
    enum class SlotState {
        kUnloaded,
        kLoaded,
        kUnavailable
    };

    struct FontSlot {
        SlotState state = SlotState::kUnloaded;
        ScopedCFRef<CTFontRef> ctfont;
        ScopedCFRef<CTFontRef> ctfont_sized;
        int pixel_height = 0;
        std::string family_name;
        std::string postscript_name;
    };
private:
    bool IsDuplicateFont(const std::string& family_name, const std::string& postscript_name) const;
    auto LoadSlot(size_t slot_index) -> std::optional<FontProviderError>;
    auto EnsureMainSlot() -> std::optional<FontProviderError>;
    auto EnsureSizedCTFont(FontSlot& slot, int char_height) -> CTFontRef;
    void ResetFontFaces();
private:
    static auto RGBAToCGColor(ColorRGBA rgba) -> ScopedCFRef<CGColorRef>;
    static auto CreateBitmapTargetCGContext(Bitmap& bitmap) -> ScopedCFRef<CGContextRef>;
    static auto CreateSizedCTFont(CTFontRef ctfont, int char_height) -> ScopedCFRef<CTFontRef>;
private:
    std::shared_ptr<Logger> log_;

    FontProvider& font_provider_;
    uint32_t iso6392_language_code_ = 0;
    std::vector<std::string> font_family_;

    std::vector<FontSlot> font_slots_;
    std::optional<size_t> main_slot_index_;
    FontProviderError main_load_error_ = FontProviderError::kFontNotFound;
    std::unordered_set<uint32_t> missing_codepoints_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_TEXT_RENDERER_CORETEXT_HPP
