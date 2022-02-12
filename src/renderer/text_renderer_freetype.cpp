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
#include "base/scoped_holder.hpp"
#include "base/utf_helper.hpp"
#include "renderer/alphablend.hpp"
#include "renderer/canvas.hpp"
#include "renderer/text_renderer_freetype.hpp"
#include FT_STROKER_H
#include FT_SFNT_NAMES_H
#include FT_TRUETYPE_IDS_H

namespace aribcaption {

TextRendererFreetype::TextRendererFreetype(Context& context, FontProvider& font_provider) :
      log_(GetContextLogger(context)), font_provider_(font_provider) {}

TextRendererFreetype::~TextRendererFreetype() = default;

bool TextRendererFreetype::Initialize() {
    FT_Library library;
    FT_Error error = FT_Init_FreeType(&library);
    if (error) {
        log_->e("Freetype: FT_Init_FreeType() failed");
        library_ = nullptr;
        return false;
    }

    library_ = ScopedHolder<FT_Library>(library, FT_Done_FreeType);
    return true;
}

void TextRendererFreetype::SetLanguage(uint32_t iso6392_language_code) {
    (void)iso6392_language_code;
    // No-OP
}

bool TextRendererFreetype::SetFontFamily(const std::vector<std::string>& font_family) {
    if (font_family.empty()) {
        return false;
    }

    if (!font_family_.empty() && font_family_ != font_family) {
        // Reset Freetype faces
        main_face_.Reset();
        fallback_face_.Reset();
        main_face_data_.clear();
        fallback_face_data_.clear();
        main_face_index_ = 0;
    }

    font_family_ = font_family;
    return true;
}

auto TextRendererFreetype::BeginDraw(Bitmap& target_bmp) -> TextRenderContext {
    return TextRenderContext(target_bmp);
}

void TextRendererFreetype::EndDraw(TextRenderContext& context) {
    (void)context;
    // No-op
}

auto TextRendererFreetype::DrawChar(TextRenderContext& render_ctx, int target_x, int target_y,
                                    uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                                    float stroke_width, int char_width, int char_height,
                                    std::optional<UnderlineInfo> underline_info,
                                    TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus {
    assert(char_height > 0);
    if (stroke_width < 0.0f) {
        stroke_width = 0.0f;
    }

    // Handle space characters
    if (ucs4 == 0x0009 || ucs4 == 0x0020 || ucs4 == 0x00A0 || ucs4 == 0x1680 ||
        ucs4 == 0x3000 || ucs4 == 0x202F || ucs4 == 0x205F || (ucs4 >= 0x2000 && ucs4 <= 0x200A)) {
        return TextRenderStatus::kOK;
    }

    if (!main_face_) {
        // If main FT_Face is not yet loaded, try load FT_Face from font_family_
        // We don't care about the codepoint (ucs4) now
        auto result = LoadFontFace(false);
        if (result.is_err()) {
            log_->e("Freetype: Cannot find valid font");
            return FontProviderErrorToStatus(result.error());
        }
        std::pair<FT_Face, size_t>& pair = result.value();
        main_face_ = ScopedHolder<FT_Face>(pair.first, FT_Done_Face);
        main_face_index_ = pair.second;
    }

    FT_Face face = main_face_;
    FT_UInt glyph_index = FT_Get_Char_Index(face, ucs4);

    if (glyph_index == 0) {
        log_->w("Freetype: Main font %s doesn't contain U+%04X", face->family_name, ucs4);

        if (fallback_policy == TextRenderFallbackPolicy::kFailOnCodePointNotFound) {
            return TextRenderStatus::kCodePointNotFound;
        }

        // Missing glyph, check fallback face
        if (fallback_face_ && (glyph_index = FT_Get_Char_Index(fallback_face_, ucs4))) {
            face = fallback_face_;
        } else if (main_face_index_ + 1 >= font_family_.size()) {
            // Fallback fonts not available
            return TextRenderStatus::kCodePointNotFound;
        } else {
            // Fallback fontface not loaded, or fallback fontface doesn't contain required codepoint
            // Load next fallback font face by specific codepoint
            auto result = LoadFontFace(true, ucs4, main_face_index_ + 1);
            if (result.is_err()) {
                log_->e("Freetype: Cannot find available fallback font for U+%04X", ucs4);
                return FontProviderErrorToStatus(result.error());
            }
            std::pair<FT_Face, size_t>& pair = result.value();
            fallback_face_ = ScopedHolder<FT_Face>(pair.first, FT_Done_Face);

            // Use this fallback fontface for rendering this time
            face = fallback_face_;
            glyph_index = FT_Get_Char_Index(face, ucs4);
            if (glyph_index == 0) {
                log_->e("Freetype: Got glyph_index == 0 for U+%04X in fallback font", ucs4);
                return TextRenderStatus::kCodePointNotFound;
            }
        }
    }

    if (FT_Set_Pixel_Sizes(face, static_cast<FT_UInt>(char_width), static_cast<FT_UInt>(char_height))) {
        log_->e("Freetype: FT_Set_Pixel_Sizes failed");
        return TextRenderStatus::kOtherError;
    }

    int baseline = static_cast<int>(face->size->metrics.ascender >> 6);
    int ascender = static_cast<int>(face->size->metrics.ascender >> 6);
    int descender = static_cast<int>(face->size->metrics.descender >> 6);
    int underline = static_cast<int>(FT_MulFix(face->underline_position, face->size->metrics.x_scale) >> 6);
    int underline_thickness = static_cast<int>(FT_MulFix(face->underline_thickness, face->size->metrics.x_scale) >> 6);

    int em_height = ascender + std::abs(descender);
    int em_adjust_y = (char_height - em_height) / 2;

    if (FT_Load_Glyph(face, glyph_index, FT_LOAD_NO_BITMAP)) {
        log_->e("Freetype: FT_Load_Glyph failed");
        return TextRenderStatus::kOtherError;
    }

    // Generate glyph bitmap for filling
    ScopedHolder<FT_Glyph> glyph_image(nullptr, FT_Done_Glyph);
    if (FT_Get_Glyph(face->glyph, &glyph_image)) {
        log_->e("Freetype: FT_Get_Glyph failed");
        return TextRenderStatus::kOtherError;
    }

    if (FT_Glyph_To_Bitmap(&glyph_image, FT_RENDER_MODE_NORMAL, nullptr, true)) {
        log_->e("Freetype: FT_Glyph_To_Bitmap failed");
        return TextRenderStatus::kOtherError;
    }

    ScopedHolder<FT_Glyph> border_glyph_image(nullptr, FT_Done_Glyph);

    // If we need stroke text (border)
    if (style & CharStyle::kCharStyleStroke && stroke_width > 0.0f) {
        // Generate glyph bitmap for stroke border
        ScopedHolder<FT_Glyph> stroke_glyph(nullptr, FT_Done_Glyph);
        if (FT_Get_Glyph(face->glyph, &stroke_glyph)) {
            log_->e("Freetype: FT_Get_Glyph failed");
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
            log_->e("Freetype: FT_Glyph_To_Bitmap failed");
            return TextRenderStatus::kOtherError;
        }

        border_glyph_image = std::move(stroke_glyph);
    }

    Canvas canvas(render_ctx.GetBitmap());

    // Draw Underline if required
    if ((style & kCharStyleUnderline) && underline_info && underline_thickness > 0) {
        int underline_y = target_y + baseline + em_adjust_y + std::abs(underline);
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
        int start_y = target_y + baseline + em_adjust_y - border_bitmap_glyph->top;

        Bitmap bmp = FTBitmapToColoredBitmap(border_bitmap_glyph->bitmap, stroke_color);
        canvas.DrawBitmap(bmp, start_x, start_y);
    }

    // Draw filling bitmap
    if (glyph_image) {
        auto bitmap_glyph = reinterpret_cast<FT_BitmapGlyph>(glyph_image.Get());
        int start_x = target_x + bitmap_glyph->left;
        int start_y = target_y + baseline + em_adjust_y - bitmap_glyph->top;

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

static bool MatchFontFamilyName(FT_Face face, const std::string& family_name) {
    FT_UInt sfnt_name_count = FT_Get_Sfnt_Name_Count(face);

    for (FT_UInt i = 0; i < sfnt_name_count; i++) {
        FT_SfntName sfnt_name{};

        if (FT_Get_Sfnt_Name(face, i, &sfnt_name)) {
            continue;
        }

        if (sfnt_name.name_id == TT_NAME_ID_FONT_FAMILY || sfnt_name.name_id == TT_NAME_ID_FULL_NAME) {
            std::string name_str;
            if (sfnt_name.platform_id == TT_PLATFORM_MICROSOFT) {
                name_str = utf::ConvertUTF16BEToUTF8(reinterpret_cast<uint16_t*>(sfnt_name.string),
                                                     sfnt_name.string_len / 2);
            } else {
                name_str = std::string(sfnt_name.string, sfnt_name.string + sfnt_name.string_len);
            }
            if (name_str == family_name) {
                return true;
            }
        }
    }

    return false;
}

auto TextRendererFreetype::LoadFontFace(bool is_fallback,
                                        std::optional<uint32_t> codepoint,
                                        std::optional<size_t> begin_index)
        -> Result<std::pair<FT_Face, size_t>, FontProviderError> {
    if (begin_index && begin_index.value() >= font_family_.size()) {
        return Err(FontProviderError::kFontNotFound);
    }

    // begin_index is optional
    size_t font_index = begin_index.value_or(0);

    const std::string& font_name = font_family_[font_index];
    auto result = font_provider_.GetFontFace(font_name, codepoint);

    while (result.is_err() && font_index + 1 < font_family_.size()) {
        // Find next suitable font
        font_index++;
        result = font_provider_.GetFontFace(font_family_[font_index], codepoint);
    }
    if (result.is_err()) {
        // Not found, return Err Result
        return Err(result.error());
    }

    FontfaceInfo& info = result.value();

    bool use_memory_data = false;
    std::vector<uint8_t>* memory_data = nullptr;
    if (!info.font_data.empty()) {
        use_memory_data = true;
        if (!is_fallback) {
            main_face_.Reset();
            main_face_data_ = std::move(info.font_data);
            memory_data = &main_face_data_;
        } else {  // is_fallback
            fallback_face_.Reset();
            fallback_face_data_ = std::move(info.font_data);
            memory_data = &fallback_face_data_;
        }
    }

    FT_Face face = nullptr;
    if (!use_memory_data) {
        if (FT_New_Face(library_, info.filename.c_str(), info.face_index, &face)) {
            return Err(FontProviderError::kFontNotFound);
        }
    } else {  // use_memory_data
        if (FT_New_Memory_Face(library_,
                               memory_data->data(),
                               static_cast<FT_Long>(memory_data->size()),
                               info.face_index,
                               &face)) {
            return Err(FontProviderError::kFontNotFound);
        }
    }

    if (info.face_index >= 0) {
        return Ok(std::make_pair(face, font_index));
    } else {
        // face_index is negative, e.g. -1, means face index is unknown
        // Find exact font face by PostScript name or Family name
        if (info.family_name.empty() && info.postscript_name.empty()) {
            log_->e("Freetype: Missing Family name / PostScript name for cases that face_index < 0");
            return Err(FontProviderError::kOtherError);
        }

        for (FT_Long i = 0; i < face->num_faces; i++) {
            FT_Done_Face(face);

            if (!use_memory_data) {
                if (FT_New_Face(library_, info.filename.c_str(), i, &face)) {
                    return Err(FontProviderError::kFontNotFound);
                }
            } else {  // use_memory_data
                if (FT_New_Memory_Face(library_,
                                       memory_data->data(),
                                       static_cast<FT_Long>(memory_data->size()),
                                       i,
                                       &face)) {
                    return Err(FontProviderError::kFontNotFound);
                }
            }

            // Find by comparing PostScript name
            if (!info.postscript_name.empty() && info.postscript_name == FT_Get_Postscript_Name(face)) {
                return Ok(std::make_pair(face, font_index));
            } else if (!info.family_name.empty() && MatchFontFamilyName(face, info.family_name)) {
                // Find by matching family name
                return Ok(std::make_pair(face, font_index));
            }
        }
        return Err(FontProviderError::kFontNotFound);
    }
}

}  // namespace aribcaption
