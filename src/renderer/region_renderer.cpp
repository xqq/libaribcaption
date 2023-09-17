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
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "renderer/region_renderer.hpp"

namespace aribcaption {

RegionRenderer::RegionRenderer(Context& context) : context_(context), log_(GetContextLogger(context)) {}

bool RegionRenderer::Initialize(FontProviderType font_provider_type, TextRendererType text_renderer_type) {
    font_provider_ = FontProvider::Create(font_provider_type, context_);
    if (!font_provider_->Initialize()) {
        return false;
    }

    text_renderer_ = TextRenderer::Create(text_renderer_type, context_, *font_provider_);
    if (!text_renderer_->Initialize()) {
        return false;
    }

    return true;
}

void RegionRenderer::SetFontLanguage(uint32_t iso6392_language_code) {
    assert(font_provider_ && text_renderer_);
    font_provider_->SetLanguage(iso6392_language_code);
    text_renderer_->SetLanguage(iso6392_language_code);
}

bool RegionRenderer::SetFontFamily(const std::vector<std::string>& font_family) {
    assert(text_renderer_);
    return text_renderer_->SetFontFamily(font_family);
}

void RegionRenderer::SetOriginalPlaneSize(int plane_width, int plane_height) {
    assert(plane_width > 0 && plane_height > 0);
    plane_width_ = plane_width;
    plane_height_ = plane_height;
    plane_inited_ = true;

    if (caption_area_inited_) {
        x_magnification_ = static_cast<float>(caption_area_width_) / static_cast<float>(plane_width);
        y_magnification_ = static_cast<float>(caption_area_height_) / static_cast<float>(plane_height);
    }
}

void RegionRenderer::SetTargetCaptionAreaRect(const Rect& rect) {
    caption_area_start_x_ = rect.left;
    caption_area_start_y_ = rect.top;
    caption_area_width_ = rect.width();
    caption_area_height_ = rect.height();
    caption_area_inited_ = true;

    if (plane_inited_) {
        x_magnification_ = static_cast<float>(caption_area_width_) / static_cast<float>(plane_width_);
        y_magnification_ = static_cast<float>(caption_area_height_) / static_cast<float>(plane_height_);
    }
}

void RegionRenderer::SetStrokeWidth(float dots) {
    if (dots >= 0.0f) {
        stroke_width_ = dots;
    }
}

void RegionRenderer::SetReplaceDRCS(bool replace) {
    replace_drcs_ = replace;
}

void RegionRenderer::SetForceStrokeText(bool force_stroke) {
    force_stroke_text_ = force_stroke;
}

void RegionRenderer::SetForceNoBackground(bool force_no_background) {
    force_no_background_ = force_no_background;
}

void RegionRenderer::SetReplaceMSZHalfWidthGlyph(bool replace) {
    assert(text_renderer_);
    text_renderer_->SetReplaceMSZHalfWidthGlyph(replace);
}

auto RegionRenderer::RenderCaptionRegion(const CaptionRegion& region,
                                         const std::unordered_map<uint32_t, DRCS>& drcs_map)
                                         -> Result<Image, RegionRenderError> {
    assert(text_renderer_ && plane_inited_ && caption_area_inited_);

    size_t char_count = region.chars.size();
    size_t succeed = 0;
    bool has_font_not_found_error = false;
    bool has_codepoint_not_found_error = false;
    [[maybe_unused]] bool has_other_error = false;

    if (ScaleWidth(region.width, region.x) < 3 || ScaleHeight(region.height, region.y) < 3) {
        return Err(RegionRenderError::kImageTooSmall);
    }

    Bitmap bitmap(ScaleWidth(region.width, region.x),
                  ScaleHeight(region.height, region.y),
                  PixelFormat::kRGBA8888);
    Canvas canvas(bitmap);
    TextRenderContext text_render_ctx = text_renderer_->BeginDraw(bitmap);

    for (const CaptionChar& ch : region.chars) {
        int section_x = ScaleX(ch.x) - ScaleX(region.x);
        int section_y = ScaleY(ch.y) - ScaleY(region.y);
        Rect section_rect(section_x,
                          section_y,
                          section_x + ScaleWidth(ch.section_width(), ch.x),
                          section_y + ScaleHeight(ch.section_height(), ch.y));
        if (section_rect.width() < 3 || section_rect.height() < 3) {
            continue;  // Too small, skip
        }

        // Draw background if not disabled
        if (!force_no_background_) {
            canvas.ClearRect(ch.back_color, section_rect);
        }

        // Draw enclosure if needed
        if (ch.enclosure_style) {
            int w = std::max(ScaleX(1), 1);  // use floor
            int h = std::max(ScaleY(1), 1);  // use floor
            if (ch.enclosure_style & EnclosureStyle::kEnclosureStyleTop) {
                canvas.ClearRect(ch.text_color,
                                 Rect(section_rect.left,
                                      section_rect.top,
                                      section_rect.right,
                                      section_rect.top + h));
            }
            if (ch.enclosure_style & EnclosureStyle::kEnclosureStyleBottom) {
                canvas.ClearRect(ch.text_color,
                                 Rect(section_rect.left,
                                      section_rect.bottom - h,
                                      section_rect.right,
                                      section_rect.bottom));
            }
            if (ch.enclosure_style & EnclosureStyle::kEnclosureStyleLeft) {
                canvas.ClearRect(ch.text_color,
                                 Rect(section_rect.left,
                                      section_rect.top,
                                      section_rect.left + w,
                                      section_rect.bottom));
            }
            if (ch.enclosure_style & EnclosureStyle::kEnclosureStyleRight) {
                canvas.ClearRect(ch.text_color,
                                 Rect(section_rect.right - w,
                                      section_rect.top,
                                      section_rect.right,
                                      section_rect.bottom));
            }
        }

        int char_x = ScaleX((float)(ch.x - region.x) + (float)ch.char_horizontal_spacing * ch.char_horizontal_scale / 2);
        int char_y = ScaleY((float)(ch.y - region.y) + (float)ch.char_vertical_spacing * ch.char_vertical_scale / 2);
        int char_width = ScaleWidth((float)ch.char_width * ch.char_horizontal_scale);
        int char_height = ScaleHeight((float)ch.char_height * ch.char_vertical_scale);
        float aspect_ratio = ((float)ch.char_width * ch.char_horizontal_scale) /
                             ((float)ch.char_height * ch.char_vertical_scale);

        if (char_width < 2 || char_height < 2) {
            continue;  // Too small, skip
        }

        CaptionCharType type = ch.type;
        CharStyle style = ch.style;
        ColorRGBA stroke_color = ch.stroke_color;
        float stroke_width = stroke_width_ * x_magnification_;
        UnderlineInfo underline_info{section_rect.left, section_rect.width()};

        if (force_stroke_text_ && !(ch.style & CharStyle::kCharStyleStroke)) {
            style = static_cast<CharStyle>(ch.style | CharStyle::kCharStyleStroke);
            // Use background color for stroke text when forcing stroke text
            stroke_color = ch.back_color;
        }

        // Draw char
        if (type == CaptionCharType::kText) {
            // Do automatic fallback rendering by default.
            TextRenderFallbackPolicy fallback_policy = TextRenderFallbackPolicy::kAutoFallback;
            if (ch.pua_codepoint) {
                // If ch contains an alternative PUA codepoint, it should be an additional symbol (gaiji)
                // Manually fallback to PUA(Private Use Area) codepoint on failure
                fallback_policy = TextRenderFallbackPolicy::kFailOnCodePointNotFound;
            }
            TextRenderStatus status = text_renderer_->DrawChar(text_render_ctx, char_x, char_y,
                                                               ch.codepoint, style, ch.text_color, stroke_color,
                                                               stroke_width, char_width, char_height, aspect_ratio,
                                                               underline_info, fallback_policy);
            if (status == TextRenderStatus::kOK) {
                succeed++;
            } else if (status == TextRenderStatus::kCodePointNotFound && ch.pua_codepoint) {
                // Additional symbol (gaiji)'s Unicode codepoint not found in font
                // Try fallback rendering with pua_codepoint
                status = text_renderer_->DrawChar(text_render_ctx, char_x, char_y,
                                                  ch.pua_codepoint, style, ch.text_color, stroke_color,
                                                  stroke_width, char_width, char_height, aspect_ratio,
                                                  underline_info, TextRenderFallbackPolicy::kAutoFallback);
                if (status == TextRenderStatus::kCodePointNotFound) {
                    // If failed, try fallback rendering with Unicode codepoint again
                    status = text_renderer_->DrawChar(text_render_ctx, char_x, char_y,
                                                      ch.codepoint, style, ch.text_color, stroke_color,
                                                      stroke_width, char_width, char_height, aspect_ratio,
                                                      underline_info, TextRenderFallbackPolicy::kAutoFallback);
                }
            }

            if (status != TextRenderStatus::kOK){
                log_->e("RegionRenderer: TextRenderer::DrawChar() returned error: %d", static_cast<int>(status));
                if (status == TextRenderStatus::kFontNotFound) {
                    has_font_not_found_error = true;
                } else if (status == TextRenderStatus::kCodePointNotFound) {
                    has_codepoint_not_found_error = true;
                } else if (status == TextRenderStatus::kOtherError) {
                    has_other_error = true;
                }
            }
        } else if (replace_drcs_ && type == CaptionCharType::kDRCSReplaced) {
            // Draw replaced DRCS (alternative ucs4)
            TextRenderStatus status = text_renderer_->DrawChar(text_render_ctx, char_x, char_y,
                                                               ch.codepoint, style, ch.text_color, stroke_color,
                                                               stroke_width, char_width, char_height, aspect_ratio,
                                                               underline_info, TextRenderFallbackPolicy::kAutoFallback);
            if (status == TextRenderStatus::kOK) {
                succeed++;
            } else {
                if (status == TextRenderStatus::kCodePointNotFound) {
                    log_->w("RegionRenderer: Cannot find alternative codepoint U+%04X, fallback to DRCS rendering",
                            ch.codepoint);
                    has_codepoint_not_found_error = true;
                } else {
                    log_->e("RegionRenderer: TextRenderer::DrawChar() returned error: %d", static_cast<int>(status));
                    if (status == TextRenderStatus::kFontNotFound) {
                        has_font_not_found_error = true;
                    } else if (status == TextRenderStatus::kOtherError) {
                        has_other_error = true;
                    }
                }
                // Fallback to DRCS rendering
                type = CaptionCharType::kDRCS;
            }
        } else if (!replace_drcs_) {
            // if DRCS replacement is disabled, force fallback to DRCS rendering
            type = CaptionCharType::kDRCS;
        }

        // Draw DRCS
        if (type == CaptionCharType::kDRCS) {
            auto iter = drcs_map.find(ch.drcs_code);
            if (iter != drcs_map.end()) {
                const DRCS& drcs = iter->second;
                bool ret = drcs_renderer_.DrawDRCS(drcs, style, ch.text_color, stroke_color,
                                                   static_cast<int>(stroke_width),
                                                   char_width, char_height, bitmap, char_x, char_y);
                if (ret) {
                    succeed++;
                } else {
                    log_->e("RegionRenderer: drcs_renderer_.DrawDRCS() returned error");
                }
            } else {
                // DRCS not found in drcs_map
                log_->e("RegionRenderer: Missing DRCS for drcs_code %u", ch.drcs_code);
            }
        }
    }

    text_renderer_->EndDraw(text_render_ctx);

    // If there's no successfully rendered char, return RegionRenderError
    if (char_count > 0 && succeed == 0) {
        if (has_font_not_found_error) {
            return Err(RegionRenderError::kFontNotFound);
        } else if (has_codepoint_not_found_error) {
            return Err(RegionRenderError::kCodePointNotFound);
        } else {
            return Err(RegionRenderError::kOtherError);
        }
    }

    Image image = Bitmap::ToImage(std::move(bitmap));
    image.dst_x = caption_area_start_x_ + ScaleX(region.x);
    image.dst_y = caption_area_start_y_ + ScaleY(region.y);

    return Ok(std::move(image));
}

}  // namespace aribcaption
