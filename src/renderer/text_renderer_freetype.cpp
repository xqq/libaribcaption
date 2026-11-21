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

#include <cassert>
#include <cstring>
#include <cstdint>
#include <cmath>
#include <string_view>
#include "base/floating_helper.hpp"
#include "base/scoped_holder.hpp"
#include "base/unicode_helper.hpp"
#include "base/utf_helper.hpp"
#include "renderer/alphablend.hpp"
#include "renderer/canvas.hpp"
#include "renderer/open_type_gsub.hpp"
#include "renderer/text_renderer_freetype.hpp"
#include FT_GLYPH_H
#include FT_STROKER_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H
#include FT_TRUETYPE_TABLES_H

namespace aribcaption {

TextRendererFreetype::TextRendererFreetype(Context& context, FontProvider& font_provider) :
      log_(GetContextLogger(context)), font_provider_(font_provider) {}

TextRendererFreetype::~TextRendererFreetype() = default;

bool TextRendererFreetype::Initialize() {
    FT_Library library;
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        log_->e("TextRendererFreetype: FT_Init_FreeType() failed");
        library_ = nullptr;
        return false;
    }

    library_ = ScopedHolder<FT_Library>(library, FT_Done_FreeType);
    return true;
}

void TextRendererFreetype::SetLanguage(uint32_t iso6392_language_code) {
    if (iso6392_language_code_ != iso6392_language_code) {
        // Font provider resolves font families (e.g. sans-serif) per language, reset loaded faces
        ResetFontFaces();
    }
    iso6392_language_code_ = iso6392_language_code;
}

bool TextRendererFreetype::SetFontFamily(const std::vector<std::string>& font_family) {
    if (font_family.empty()) {
        return false;
    }

    if (!font_family_.empty() && font_family_ != font_family) {
        ResetFontFaces();
    }

    font_family_ = font_family;
    return true;
}

void TextRendererFreetype::ResetFontFaces() {
    font_slots_.clear();
    system_fallback_slots_.clear();
    main_slot_index_.reset();
    main_load_error_ = FontProviderError::kFontNotFound;
    missing_codepoints_.clear();
}

void TextRendererFreetype::SetReplaceMSZHalfWidthGlyph(bool replace) {
    replace_msz_halfwidth_glyph_ = replace;
}

auto TextRendererFreetype::BeginDraw(Bitmap& target_bmp) -> TextRenderContext {
    return TextRenderContext(target_bmp);
}

void TextRendererFreetype::EndDraw(TextRenderContext& context) {
    (void)context;
    // No-op
}

static auto LoadSFNTTable(FT_Face face, FT_Tag tag) -> std::vector<uint8_t> {
    std::vector<uint8_t> gsub;
    FT_ULong gsub_size = 0;
    if (FT_Load_Sfnt_Table(face, tag, 0, nullptr, &gsub_size)) {
        return gsub;
    }
    gsub.resize(static_cast<size_t>(gsub_size));
    if (FT_Load_Sfnt_Table(face, tag, 0, reinterpret_cast<FT_Byte*>(gsub.data()), &gsub_size)) {
        gsub.clear();
    }
    return gsub;
}

static auto CalculateBaselineFromGlyphBounds(FT_Face face, FT_UInt glyph_index, int char_height)
        -> std::optional<int> {
    if (glyph_index == 0 || FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP)) {
        return std::nullopt;
    }

    ScopedHolder<FT_Glyph> glyph(nullptr, FT_Done_Glyph);
    if (FT_Get_Glyph(face->glyph, &glyph)) {
        return std::nullopt;
    }

    FT_BBox bounds{};
    FT_Glyph_Get_CBox(glyph, FT_GLYPH_BBOX_PIXELS, &bounds);
    int glyph_height = static_cast<int>(bounds.yMax - bounds.yMin);
    if (glyph_height <= 0) {
        return std::nullopt;
    }

    // Center the glyph's ink bounds in the ARIB character box and return the
    // corresponding baseline position measured from the top of that box.
    return (char_height - glyph_height) / 2 + static_cast<int>(bounds.yMax);
}

static auto CalculateIdeographicCharacterFaceBaseline(FT_Face face, int char_height) -> std::optional<int> {
    // OpenType calls the approximate bounds shared by full-width ideographic
    // and kana glyphs the Ideographic Character Face (ICF). If a font does not
    // provide ICF values in its BASE table, the OpenType specification permits
    // estimating them from representative ideographic and kana glyph bounds.
    // Until this renderer parses the BASE table, use that recommended fallback.
    static constexpr uint32_t kReferenceChars[] = {
        0x6C38,  // CJK ideograph "永"
        0x56FD,  // CJK ideograph "国"
        0x3042,  // Hiragana "あ"
    };

    for (uint32_t ch : kReferenceChars) {
        auto baseline = CalculateBaselineFromGlyphBounds(face, FT_Get_Char_Index(face, ch), char_height);
        if (baseline) {
            return baseline;
        }
    }
    return std::nullopt;
}

static uint64_t MakePixelSizeKey(int char_width, int char_height) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(char_width)) << 32) |
           static_cast<uint32_t>(char_height);
}

auto TextRendererFreetype::DrawChar(TextRenderContext& render_ctx, int target_x, int target_y,
                                    uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                                    float stroke_width, int char_width, int char_height, float aspect_ratio,
                                    std::optional<UnderlineInfo> underline_info,
                                    TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus {
    assert(char_height > 0);
    if (stroke_width < 0.0f) {
        stroke_width = 0.0f;
    }

    // Skip space characters
    if (unicode::IsSpaceCharacter(ucs4)) {
        return TextRenderStatus::kOK;
    }

    if (auto err = EnsureMainSlot()) {
        log_->e("TextRendererFreetype: Cannot find valid font");
        return FontProviderErrorToStatus(*err);
    }

    FontSlot* selected_slot = &font_slots_[*main_slot_index_];
    FT_UInt glyph_index = FT_Get_Char_Index(selected_slot->face, ucs4);

    if (glyph_index == 0) {
        if (fallback_policy == TextRenderFallbackPolicy::kFailOnCodePointNotFound) {
            return TextRenderStatus::kCodePointNotFound;
        }

        if (missing_codepoints_.find(ucs4) != missing_codepoints_.end()) {
            return TextRenderStatus::kCodePointNotFound;
        }

        // Try fallback fonts in configured font-family list
        for (size_t i = *main_slot_index_ + 1; i < font_slots_.size(); i++) {
            FontSlot& slot = font_slots_[i];
            if (slot.state == SlotState::kUnavailable) {
                continue;
            }
            if (slot.state == SlotState::kUnloaded) {
                LoadSlot(i);
                if (slot.state != SlotState::kLoaded) {
                    continue;
                }
            }

            FT_UInt candidate_glyph = FT_Get_Char_Index(slot.face, ucs4);
            if (candidate_glyph != 0) {
                selected_slot = &slot;
                glyph_index = candidate_glyph;
                break;
            }
        }

        // Try system fallback, if allowed by fallback policy
        if (glyph_index == 0 && fallback_policy == TextRenderFallbackPolicy::kAutoFallback) {
            if (FontSlot* sys_slot =
                    FindOrLoadSystemFallbackSlot(ucs4, font_family_[*main_slot_index_], glyph_index)) {
                selected_slot = sys_slot;
            }
        }

        if (glyph_index == 0) {
            // A font-family-only miss may still be served by system fallback later, so don't cache it,
            // unless it is a PUA codepoint which system fallback never handles
            if (fallback_policy == TextRenderFallbackPolicy::kAutoFallback || unicode::IsPrivateUseArea(ucs4)) {
                missing_codepoints_.insert(ucs4);
                log_->e("TextRendererFreetype: Cannot find available fallback font for U+%04X", ucs4);
            }
            return TextRenderStatus::kCodePointNotFound;
        }
    }

    FT_Face face = selected_slot->face;

    // If aspect_ratio is 0.5 (1:2), this should be MSZ (Middle size)
    bool is_requesting_halfwidth = floating::AlmostEquals(aspect_ratio, 0.5f, 0.05f);
    if (is_requesting_halfwidth && unicode::IsHalfwidthCharacter(ucs4)) {
        // Do not do horizontal scaling since the character is halfwidth already
        char_width = char_height;
    }

    if (replace_msz_halfwidth_glyph_ && is_requesting_halfwidth) {
        if (!selected_slot->halfwidth_subst_map) {
            selected_slot->halfwidth_subst_map =
                LoadSingleGSUBTable(LoadSFNTTable(face, FT_MAKE_TAG('G', 'S', 'U', 'B')), kOpenTypeFeatureHalfWidth,
                                    kOpenTypeScriptHiraganaKatakana, kOpenTypeLangSysJapanese);
        }
        auto subst = selected_slot->halfwidth_subst_map->find(glyph_index);
        if (subst != selected_slot->halfwidth_subst_map->end()) {
            glyph_index = subst->second;
            char_width = char_height;
        }
    }

    if (FT_Set_Pixel_Sizes(face, static_cast<FT_UInt>(char_width), static_cast<FT_UInt>(char_height))) {
        log_->e("TextRendererFreetype: FT_Set_Pixel_Sizes failed");
        return TextRenderStatus::kOtherError;
    }

    int baseline = static_cast<int>(face->size->metrics.ascender >> 6);
    int ascender = static_cast<int>(face->size->metrics.ascender >> 6);
    int descender = static_cast<int>(face->size->metrics.descender >> 6);
    int underline = static_cast<int>(FT_MulFix(face->underline_position, face->size->metrics.x_scale) >> 6);
    int underline_thickness = static_cast<int>(FT_MulFix(face->underline_thickness, face->size->metrics.x_scale) >> 6);

    int em_height = ascender + std::abs(descender);
    int em_adjust_y = (char_height - em_height) / 2;
    baseline += em_adjust_y;

    auto& baseline_cache = selected_slot->baseline_cache;
    uint64_t pixel_size_key = MakePixelSizeKey(char_width, char_height);
    auto baseline_iter = baseline_cache.find(pixel_size_key);
    if (baseline_iter == baseline_cache.end()) {
        baseline_iter = baseline_cache.emplace(
                pixel_size_key, CalculateIdeographicCharacterFaceBaseline(face, char_height)).first;
    }
    if (baseline_iter->second) {
        baseline = *baseline_iter->second;
    }

    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP)) {
        log_->e("TextRendererFreetype: FT_Load_Glyph failed");
        return TextRenderStatus::kOtherError;
    }

    // Generate glyph bitmap for filling
    ScopedHolder<FT_Glyph> glyph_image(nullptr, FT_Done_Glyph);
    if (FT_Get_Glyph(face->glyph, &glyph_image)) {
        log_->e("TextRendererFreetype: FT_Get_Glyph failed");
        return TextRenderStatus::kOtherError;
    }

    if (FT_Glyph_To_Bitmap(&glyph_image, FT_RENDER_MODE_NORMAL, nullptr, true)) {
        log_->e("TextRendererFreetype: FT_Glyph_To_Bitmap failed");
        return TextRenderStatus::kOtherError;
    }

    ScopedHolder<FT_Glyph> border_glyph_image(nullptr, FT_Done_Glyph);

    // If we need stroke text (border)
    if (style & CharStyle::kCharStyleStroke && stroke_width > 0.0f) {
        // Generate glyph bitmap for stroke border
        ScopedHolder<FT_Glyph> stroke_glyph(nullptr, FT_Done_Glyph);
        if (FT_Get_Glyph(face->glyph, &stroke_glyph)) {
            log_->e("TextRendererFreetype: FT_Get_Glyph failed");
            return TextRenderStatus::kOtherError;
        }

        ScopedHolder<FT_Stroker> stroker(nullptr, FT_Stroker_Done);
        FT_Stroker_New(library_, &stroker);
        FT_Stroker_Set(stroker,
                       static_cast<FT_Fixed>(stroke_width * 64),
                       FT_STROKER_LINECAP_ROUND,
                       FT_STROKER_LINEJOIN_ROUND,
                       0);

        FT_Glyph_StrokeBorder(&stroke_glyph, stroker, false, true);

        if (FT_Glyph_To_Bitmap(&stroke_glyph, FT_RENDER_MODE_NORMAL, nullptr, true)) {
            log_->e("TextRendererFreetype: FT_Glyph_To_Bitmap failed");
            return TextRenderStatus::kOtherError;
        }

        border_glyph_image = std::move(stroke_glyph);
    }

    Canvas canvas(render_ctx.GetBitmap());

    // Draw Underline if required
    if ((style & kCharStyleUnderline) && underline_info && underline_thickness > 0) {
        int underline_y = target_y + baseline + std::abs(underline);
        Rect underline_rect(underline_info->start_x,
                            underline_y,
                            underline_info->start_x + underline_info->width,
                            underline_y + 1);

        int half_thickness = underline_thickness / 2;

        if (underline_thickness % 2) {  // odd number
            underline_rect.top -= half_thickness;
            underline_rect.bottom += half_thickness;
        } else {  // even number
            underline_rect.top -= half_thickness - 1;
            underline_rect.bottom += half_thickness;
        }

        canvas.DrawRect(color, underline_rect);
    }

    // Draw stroke border bitmap, if required
    if (border_glyph_image) {
        auto border_bitmap_glyph = reinterpret_cast<FT_BitmapGlyph>(border_glyph_image.Get());
        int start_x = target_x + border_bitmap_glyph->left;
        int start_y = target_y + baseline - border_bitmap_glyph->top;

        Bitmap bmp = FTBitmapToColoredBitmap(border_bitmap_glyph->bitmap, stroke_color);
        canvas.DrawBitmap(bmp, start_x, start_y);
    }

    // Draw filling bitmap
    if (glyph_image) {
        auto bitmap_glyph = reinterpret_cast<FT_BitmapGlyph>(glyph_image.Get());
        int start_x = target_x + bitmap_glyph->left;
        int start_y = target_y + baseline - bitmap_glyph->top;

        Bitmap bmp = FTBitmapToColoredBitmap(bitmap_glyph->bitmap, color);
        canvas.DrawBitmap(bmp, start_x, start_y);
    }

    return TextRenderStatus::kOK;
}

Bitmap TextRendererFreetype::FTBitmapToColoredBitmap(const FT_Bitmap& ft_bmp, ColorRGBA color) {
    Bitmap bitmap(static_cast<int>(ft_bmp.width), static_cast<int>(ft_bmp.rows), PixelFormat::kRGBA8888);

    for (uint32_t y = 0; y < ft_bmp.rows; y++) {
        uint32_t src_index = y * ft_bmp.pitch;
        const uint8_t* src = &ft_bmp.buffer[src_index];
        ColorRGBA* dest = bitmap.GetPixelAt(0, static_cast<int>(y));

        alphablend::FillLineWithAlphas(dest, src, color, ft_bmp.width);
    }

    return bitmap;
}

static bool MatchFontFamilyName(FT_Face face, std::string_view family_name) {
    FT_UInt sfnt_name_count = FT_Get_Sfnt_Name_Count(face);

    for (FT_UInt i = 0; i < sfnt_name_count; i++) {
        FT_SfntName sfnt_name{};

        if (FT_Get_Sfnt_Name(face, i, &sfnt_name)) {
            continue;
        }

        if (sfnt_name.name_id == TT_NAME_ID_FONT_FAMILY || sfnt_name.name_id == TT_NAME_ID_FULL_NAME) {
            if (sfnt_name.platform_id == TT_PLATFORM_MICROSOFT) {
                std::string name_str = utf::ConvertUTF16BEToUTF8(reinterpret_cast<uint16_t*>(sfnt_name.string),
                                                                 sfnt_name.string_len / 2);
                if (name_str == family_name) {
                    return true;
                }
            } else {
                std::string_view name_str(reinterpret_cast<const char*>(sfnt_name.string), sfnt_name.string_len);
                if (name_str == family_name) {
                    return true;
                }
            }
        }
    }

    return false;
}

auto TextRendererFreetype::OpenFTFace(FontfaceInfo& info, FontSlot& out_slot) -> std::optional<FontProviderError> {
    bool use_memory_data = !info.font_data.empty();
    std::vector<uint8_t> face_data;
    if (use_memory_data) {
        face_data = std::move(info.font_data);
    }

    auto create_face = [&](FT_Long index) -> ScopedHolder<FT_Face> {
        FT_Face raw_face = nullptr;
        FT_Error err = 0;
        if (!use_memory_data) {
            err = FT_New_Face(library_, info.filename.c_str(), index, &raw_face);
        } else {
            err = FT_New_Memory_Face(library_,
                                     face_data.data(),
                                     static_cast<FT_Long>(face_data.size()),
                                     index,
                                     &raw_face);
        }
        if (err || !raw_face) {
            return {};
        }
        return ScopedHolder<FT_Face>(raw_face, FT_Done_Face);
    };

    ScopedHolder<FT_Face> face = create_face(info.face_index);
    if (!face) {
        return FontProviderError::kFontNotFound;
    }

    if (info.face_index < 0) {
        // Negative face_index queries num_faces; prefer exact PostScript match across all faces,
        // falling back to the first family-name match only if no face matched the PostScript name
        if (info.family_name.empty() && info.postscript_name.empty()) {
            log_->e("TextRendererFreetype: Missing Family name / PostScript name for cases that face_index < 0");
            return FontProviderError::kOtherError;
        }

        FT_Long num_faces = face->num_faces;
        face.Reset();

        ScopedHolder<FT_Face> family_match;

        for (FT_Long i = 0; i < num_faces; i++) {
            ScopedHolder<FT_Face> candidate = create_face(i);
            if (!candidate) {
                return FontProviderError::kFontNotFound;
            }

            if (!info.postscript_name.empty()) {
                const char* ps_name = FT_Get_Postscript_Name(candidate);
                if (ps_name && info.postscript_name == ps_name) {
                    face = std::move(candidate);
                    break;
                }
            }

            if (!family_match && !info.family_name.empty() && MatchFontFamilyName(candidate, info.family_name)) {
                family_match = std::move(candidate);
                if (info.postscript_name.empty()) {
                    break;
                }
            }
        }

        if (!face) {
            face = std::move(family_match);
        }

        if (!face) {
            return FontProviderError::kFontNotFound;
        }
    }

    if (!FT_IS_SCALABLE(face)) {
        // FT_LOAD_NO_BITMAP + FT_Stroker require scalable outlines
        log_->w("TextRendererFreetype: Skipping non-scalable font %s",
                face->family_name ? face->family_name : info.family_name.c_str());
        return FontProviderError::kFontNotFound;
    }

    out_slot.face = FTFaceHolder(std::move(face_data), std::move(face));
    out_slot.filename = info.filename;
    out_slot.face_index = static_cast<int>(out_slot.face->face_index);
    out_slot.family_name = out_slot.face->family_name ? out_slot.face->family_name : info.family_name;
    const char* ps_name = FT_Get_Postscript_Name(out_slot.face);
    out_slot.postscript_name = ps_name ? ps_name : info.postscript_name;
    out_slot.state = SlotState::kLoaded;
    return std::nullopt;
}

bool TextRendererFreetype::IsDuplicateFontInfo(const FontfaceInfo& info) const {
    if (info.filename.empty()) {
        return false;
    }

    for (const auto& existing : font_slots_) {
        if (existing.state != SlotState::kLoaded || existing.filename != info.filename) {
            continue;
        }
        if (info.face_index >= 0) {
            if (existing.face_index == info.face_index) {
                return true;
            }
        } else if (!info.postscript_name.empty() && existing.postscript_name == info.postscript_name) {
            return true;
        }
    }
    return false;
}

bool TextRendererFreetype::IsDuplicateSlot(const FontSlot& candidate) const {
    auto matches_list = [&](const std::vector<FontSlot>& slots) -> bool {
        for (const auto& existing : slots) {
            if (existing.state != SlotState::kLoaded) {
                continue;
            }
            if (!candidate.filename.empty() && !existing.filename.empty()) {
                if (candidate.filename == existing.filename && candidate.face_index == existing.face_index) {
                    return true;
                }
            } else if (!candidate.postscript_name.empty() && !existing.postscript_name.empty()) {
                if (candidate.postscript_name == existing.postscript_name) {
                    return true;
                }
            } else if (!candidate.family_name.empty() &&
                       candidate.family_name == existing.family_name &&
                       candidate.face_index == existing.face_index) {
                return true;
            }
        }
        return false;
    };

    return matches_list(font_slots_) || matches_list(system_fallback_slots_);
}

auto TextRendererFreetype::LoadSlot(size_t slot_index) -> std::optional<FontProviderError> {
    FontSlot& slot = font_slots_[slot_index];
    if (slot.state != SlotState::kUnloaded) {
        return std::nullopt;
    }

    auto result = font_provider_.GetFontFace(font_family_[slot_index]);
    if (result.is_err()) {
        slot.state = SlotState::kUnavailable;
        return result.error();
    }

    FontfaceInfo& info = result.value();
    if (IsDuplicateFontInfo(info)) {
        slot.state = SlotState::kUnavailable;
        return std::nullopt;
    }

    FontSlot loaded_slot;
    if (auto err = OpenFTFace(info, loaded_slot)) {
        slot.state = SlotState::kUnavailable;
        return err;
    }

    if (IsDuplicateSlot(loaded_slot)) {
        slot.state = SlotState::kUnavailable;
        return std::nullopt;
    }

    slot = std::move(loaded_slot);
    return std::nullopt;
}

auto TextRendererFreetype::EnsureMainSlot() -> std::optional<FontProviderError> {
    if (font_family_.empty()) {
        return FontProviderError::kFontNotFound;
    }

    if (main_slot_index_) {
        return std::nullopt;
    }

    if (font_slots_.empty()) {
        font_slots_.resize(font_family_.size());
    }

    for (size_t i = 0; i < font_slots_.size(); i++) {
        if (font_slots_[i].state == SlotState::kUnavailable) {
            continue;
        }
        if (auto err = LoadSlot(i)) {
            main_load_error_ = *err;
        }
        if (font_slots_[i].state == SlotState::kLoaded) {
            main_slot_index_ = i;
            return std::nullopt;
        }
    }

    return main_load_error_;
}

auto TextRendererFreetype::FindOrLoadSystemFallbackSlot(uint32_t ucs4,
                                                        std::string_view base_family_name,
                                                        FT_UInt& out_glyph_index) -> FontSlot* {
    if (unicode::IsPrivateUseArea(ucs4)) {
        return nullptr;
    }

    for (auto& slot : system_fallback_slots_) {
        if (slot.state != SlotState::kLoaded) {
            continue;
        }
        FT_UInt glyph = FT_Get_Char_Index(slot.face, ucs4);
        if (glyph != 0) {
            out_glyph_index = glyph;
            return &slot;
        }
    }

    auto result = font_provider_.GetFallbackFontFace(ucs4, base_family_name);
    if (result.is_err()) {
        return nullptr;
    }

    FontSlot loaded_slot;
    if (OpenFTFace(result.value(), loaded_slot)) {
        return nullptr;
    }

    FT_UInt glyph = FT_Get_Char_Index(loaded_slot.face, ucs4);
    if (glyph == 0 || IsDuplicateSlot(loaded_slot)) {
        return nullptr;
    }

    static constexpr size_t kMaxSystemFallbackSlots = 16;
    if (system_fallback_slots_.size() >= kMaxSystemFallbackSlots) {
        system_fallback_slots_.erase(system_fallback_slots_.begin());
    }

    system_fallback_slots_.push_back(std::move(loaded_slot));
    out_glyph_index = glyph;
    return &system_fallback_slots_.back();
}

}  // namespace aribcaption
