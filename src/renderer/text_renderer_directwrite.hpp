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
#include <dwrite_2.h>
#include <wincodec.h>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_set>
#include <vector>
#include "aribcaption/context.hpp"
#include "base/scoped_com_initializer.hpp"
#include "renderer/fallback_codepoint_cache.hpp"
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
    void SetReplaceMSZHalfWidthGlyph(bool replace) override;
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
        FontfaceInfo faceinfo;
        ComPtr<IDWriteTextFormat> text_format;
        int pixel_height = 0;
        uint32_t slot_id = 0;
        uint64_t last_used = 0;
    };

    struct SystemFallbackLookup {
        FontSlot* existing_slot = nullptr;
        FontSlot new_slot;
    };
private:
    static bool MatchesFontInfo(const FontSlot& existing, const FontfaceInfo& info, bool compare_scale);
    bool MatchesConfiguredSlot(const FontfaceInfo& info) const;
    FontSlot* FindSystemFallbackSlotById(uint32_t slot_id);
    FontSlot* FindSystemFallbackSlot(const FontfaceInfo& info);
    auto LoadSlot(size_t slot_index) -> std::optional<FontProviderError>;
    auto EnsureMainSlot() -> std::optional<FontProviderError>;
    auto TryDrawCharWithSlot(TextRenderContext& render_ctx, FontSlot& slot, std::u16string_view wide_char,
                             int target_x, int target_y, uint32_t ucs4, CharStyle style,
                             ColorRGBA color, ColorRGBA stroke_color, float stroke_width,
                             int char_width, int char_height, float aspect_ratio,
                             std::optional<UnderlineInfo> underline_info) -> TextRenderStatus;
    auto LoadSystemFallbackSlot(uint32_t ucs4, std::string_view base_family_name)
        -> Result<SystemFallbackLookup, TextRenderStatus>;
    uint32_t CacheSystemFallbackSlot(FontSlot&& slot);
    auto EnsureTextFormat(FontSlot& slot, int char_height) -> IDWriteTextFormat*;
    auto CreateDWriteTextFormat(FontfaceInfo& face_info, float font_size) -> ComPtr<IDWriteTextFormat>;
    auto CreateWICRenderTarget(IWICBitmap* target) -> ComPtr<ID2D1RenderTarget>;
    bool BlendWICBitmapToBitmap(IWICBitmap* wic_bitmap, Bitmap& target_bmp,
                                int target_x, int target_y);
    void ResetFontFaces();
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
    ComPtr<IDWriteFontFallback> no_fallback_;  // Empty fallback, null on Windows 7

    std::vector<FontSlot> font_slots_;
    std::vector<FontSlot> system_fallback_slots_;
    std::optional<size_t> main_slot_index_;
    FontProviderError main_load_error_ = FontProviderError::kFontNotFound;
    std::unordered_set<uint32_t> missing_codepoints_;
    FallbackCodepointCache fallback_codepoint_cache_;
    uint32_t next_fallback_slot_id_ = 1;
    uint64_t fallback_lru_counter_ = 0;

    bool replace_msz_halfwidth_glyph_ = true;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_TEXT_RENDERER_DIRECTWRITE_HPP
