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

#ifndef ARIBCAPTION_TEXT_RENDERER_FREETYPE_HPP
#define ARIBCAPTION_TEXT_RENDERER_FREETYPE_HPP

#include <ft2build.h>
#include FT_FREETYPE_H
#include <cstdint>
#include <vector>
#include <string>
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include "aribcaption/caption.hpp"
#include "aribcaption/color.hpp"
#include "aribcaption/context.hpp"
#include "base/logger.hpp"
#include "base/result.hpp"
#include "base/scoped_holder.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/fallback_codepoint_cache.hpp"
#include "renderer/font_provider.hpp"
#include "renderer/text_renderer.hpp"

namespace aribcaption {

class TextRendererFreetype : public TextRenderer {
public:
    TextRendererFreetype(Context& context, FontProvider& font_provider);
    ~TextRendererFreetype() override;
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

    enum class OpenFaceError {
        kUnsupportedFace,  // Unusable by FreeType (no path/data, non-scalable, unsupported format, no subface match)
        kOpenError,        // File/stream open or I/O failure (e.g. FT_Err_Cannot_Open_Resource, transient EMFILE)
        kInvalidInfo,      // Missing family_name and postscript_name when face_index < 0
    };

    // Owns an FT_Face and the memory buffer it may reference
    // The face is always closed before the buffer is freed
    struct FTFaceHolder {
        FTFaceHolder() = default;
        FTFaceHolder(std::vector<uint8_t>&& data, ScopedHolder<FT_Face>&& face)
            : data(std::move(data)), face(std::move(face)) {}
        FTFaceHolder(FTFaceHolder&&) noexcept = default;

        FTFaceHolder& operator=(FTFaceHolder&& other) noexcept {
            if (this != &other) {
                face.Reset();
                data = std::move(other.data);
                face = std::move(other.face);
            }
            return *this;
        }

        operator FT_Face() const noexcept {
            return face.Get();
        }

        FT_Face operator->() const noexcept {
            return face.Get();
        }

        // data is declared before `face`, so destruction also closes the face first
        std::vector<uint8_t> data;
        ScopedHolder<FT_Face> face;
    };

    struct FontSlot {
        SlotState state = SlotState::kUnloaded;
        FTFaceHolder face;
        std::string filename;
        int face_index = 0;
        std::string family_name;
        std::string postscript_name;
        std::optional<std::unordered_map<uint32_t, uint32_t>> halfwidth_subst_map;
        std::unordered_map<uint64_t, std::optional<int>> baseline_cache;
        uint32_t slot_id = 0;
        uint64_t last_used = 0;
    };
private:
    static Bitmap FTBitmapToColoredBitmap(const FT_Bitmap& ft_bmp, ColorRGBA color);
    static bool MatchesFontInfo(const FontSlot& existing, const FontfaceInfo& info);
    static bool IsSameLoadedSlot(const FontSlot& a, const FontSlot& b);
    auto OpenFTFace(FontfaceInfo& info, FontSlot& out_slot) -> std::optional<OpenFaceError>;
    bool IsDuplicateFontInfo(const FontfaceInfo& info) const;
    bool MatchesConfiguredSlot(const FontSlot& candidate) const;
    FontSlot* FindSystemFallbackSlotById(uint32_t slot_id);
    FontSlot* FindSystemFallbackSlot(const FontfaceInfo& info);
    FontSlot* FindSystemFallbackSlot(const FontSlot& candidate);
    FontSlot* StoreSystemFallbackSlot(FontSlot&& slot);
    auto LoadSlot(size_t slot_index) -> std::optional<FontProviderError>;
    auto EnsureMainSlot() -> std::optional<FontProviderError>;
    auto FindOrLoadSystemFallbackSlot(uint32_t ucs4, std::string_view base_family_name, FT_UInt& out_glyph_index)
        -> Result<FontSlot*, TextRenderStatus>;
    void ResetFontFaces();
private:
    std::shared_ptr<Logger> log_;

    FontProvider& font_provider_;
    uint32_t iso6392_language_code_ = 0;
    std::vector<std::string> font_family_;

    ScopedHolder<FT_Library> library_;
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

#endif  // ARIBCAPTION_TEXT_RENDERER_FREETYPE_HPP
