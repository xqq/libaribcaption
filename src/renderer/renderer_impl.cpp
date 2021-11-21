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

#include <cmath>
#include <algorithm>
#include "aribcaption/context.hpp"
#include "renderer/renderer_impl.hpp"

namespace aribcaption::internal {

RendererImpl::RendererImpl(Context& context)
    : context_(context), log_(GetContextLogger(context)), region_renderer_(context) {}

RendererImpl::~RendererImpl() = default;

bool RendererImpl::Initialize(CaptionType caption_type,
                              FontProviderType font_provider_type,
                              TextRendererType text_renderer_type) {
    expected_caption_type_ = caption_type;
    LoadDefaultFontFamilies();
    return region_renderer_.Initialize(font_provider_type, text_renderer_type);
}

void RendererImpl::LoadDefaultFontFamilies() {
    // Font face for default language (0)
    language_font_family_[0] = { "sans-serif" };

    // Default fonts for Japanese (jpn)
    std::vector<std::string> jpn_default_font_family;
#if defined(_WIN32)
    jpn_default_font_family = {
        "Windows TV MaruGothic",
        "MS Gothic",
        "sans-serif",
    };
#elif defined(__APPLE__)
    jpn_default_font_family = {
        "Hiragino Maru Gothic ProN",
        "Hiragino Sans",
        "sans-serif",
    };
#else  // Linux, or other platforms
    jpn_default_font_family = {
        "Noto Sans CJK JP",
        "Noto Sans CJK",
        "Source Han Sans JP",
        "sans-serif",
    };
#endif
    language_font_family_[ThreeCC("jpn")] = std::move(jpn_default_font_family);

    // Default fonts for latin languages (Portuguese / Spanish)
    std::vector<std::string> latin_default_font_family = { "sans-serif" };
    language_font_family_[ThreeCC("por")] = latin_default_font_family;  // Portuguese
    language_font_family_[ThreeCC("spa")] = latin_default_font_family;  // Spanish
}

void RendererImpl::SetStrokeWidth(float dots) {
    region_renderer_.SetStrokeWidth(dots);
}

void RendererImpl::SetReplaceDRCS(bool replace) {
    region_renderer_.SetReplaceDRCS(replace);
}

void RendererImpl::SetForceStrokeText(bool force_stroke) {
    region_renderer_.SetForceStrokeText(force_stroke);
}

void RendererImpl::SetForceNoBackground(bool force_no_background) {
    region_renderer_.SetForceNoBackground(force_no_background);
}

bool RendererImpl::SetDefaultFontFamily(const std::vector<std::string>& font_family, bool force_default) {
    force_default_font_family_ = force_default;
    return SetLanguageSpecificFontFamily(0, font_family);
}

bool RendererImpl::SetLanguageSpecificFontFamily(uint32_t language_code, const std::vector<std::string>& font_family) {
    if (font_family.empty()) {
        return false;
    }

    language_font_family_[language_code] = font_family;

    InvalidatePrevRenderedImages();
    return true;
}

bool RendererImpl::SetFrameSize(int frame_width, int frame_height) {
    if (frame_width_ != frame_width || frame_height_ != frame_height) {
        InvalidatePrevRenderedImages();
    }

    frame_width_ = frame_width;
    frame_height_ = frame_height;
    frame_size_inited_ = true;

    if (!margins_inited_) {
        SetMargins(0, 0, 0, 0);
    }

    return true;
}

bool RendererImpl::SetMargins(int top, int bottom, int left, int right) {
    assert(frame_size_inited_ && "Frame size is not indicated, call SetFrameSize() first");

    int video_width = frame_width_ - left - right;
    int video_height = frame_height_ - top - bottom;

    if (video_width < 0 || video_height < 0) {
        log_->e("RendererImpl: Invalid margins, video area size attempts to be < 0");
        return false;
    }

    if (margin_top_ != top || margin_bottom_ != bottom || margin_left_ != left || margin_right_ != right) {
        InvalidatePrevRenderedImages();
    }

    video_area_width_ = video_width;
    video_area_height_ = video_height;
    video_area_start_x_ = left;
    video_area_start_y_ = top;
    video_area_size_inited_ = true;

    margin_top_ = top;
    margin_bottom_ = bottom;
    margin_left_ = left;
    margin_right_ = right;
    margins_inited_ = true;

    return true;
}

bool RendererImpl::AppendCaption(const Caption& caption) {
    assert(caption.pts != PTS_NOPTS && "Caption without PTS is not supported");
    assert(caption.plane_width > 0 && caption.plane_height > 0);

    if (caption.pts == PTS_NOPTS || caption.plane_width <= 0 || caption.plane_height <= 0) {
        return false;
    }

    auto prev = captions_.rbegin();
    if (prev != captions_.rend() && prev->second.duration == DURATION_INDEFINITE) {
        Caption& prev_caption = prev->second;
        prev_caption.duration = caption.pts - prev_caption.pts;
    }

    captions_.insert_or_assign(captions_.end(), caption.pts, caption);
    return true;
}

bool RendererImpl::AppendCaption(Caption&& caption) {
    assert(caption.pts != PTS_NOPTS && "Caption without PTS is not supported");
    assert(caption.plane_width > 0 && caption.plane_height > 0);

    if (caption.pts == PTS_NOPTS || caption.plane_width <= 0 || caption.plane_height <= 0) {
        return false;
    }

    auto prev = captions_.rbegin();
    if (prev != captions_.rend() && prev->second.duration == DURATION_INDEFINITE) {
        Caption& prev_caption = prev->second;
        prev_caption.duration = caption.pts - prev_caption.pts;
    }

    captions_.insert_or_assign(captions_.end(), caption.pts, std::move(caption));
    return true;
}

Renderer::RenderStatus RendererImpl::Render(int64_t pts, const Renderer::OutputCB& output_cb) {
    assert(frame_size_inited_ && margins_inited_ && "Frame size / margins must be indicated first");

    if (has_prev_rendered_caption_) {
        if (pts >= prev_rendered_caption_pts_ && pts < prev_rendered_caption_pts_ + prev_rendered_caption_duration_) {
            if (!prev_rendered_images_.empty()) {
                // Re-use previous rendered images
                output_cb(prev_rendered_caption_pts_, prev_rendered_caption_duration_, prev_rendered_images_);
                return Renderer::kRenderStatusGotImageUnchanged;
            } else {
                return Renderer::kRenderStatusNoImage;
            }
        }
    }

    if (captions_.empty()) {
        return Renderer::kRenderStatusNoImage;
    }

    auto iter = captions_.lower_bound(pts);
    if (iter == captions_.end()) {
        --iter;
    } else if (iter != captions_.begin() && iter->first > pts) {
        --iter;
    }

    Caption& caption = iter->second;
    if (pts < caption.pts || (caption.duration != DURATION_INDEFINITE && pts >= caption.pts + caption.duration)) {
        // Timeout
        return Renderer::kRenderStatusNoImage;
    }

    // Prepare for rendering

    // Set up Font Language
    region_renderer_.SetFontLanguage(caption.iso6392_language_code);

    // Set up Font Family
    uint32_t language_code = caption.iso6392_language_code;
    if (force_default_font_family_) {
        language_code = 0;
    }
    region_renderer_.SetFontFamily(language_font_family_[language_code]);

    // Set up origin plane size / target caption area
    AdjustCaptionArea(caption.plane_width, caption.plane_height);

    std::vector<Image> images;
    for (CaptionRegion& region : caption.regions) {
        Result<Image, RegionRenderError> result = region_renderer_.RenderCaptionRegion(region, caption.drcs_map);
        if (result.is_ok()) {
            images.push_back(std::move(result.value()));
        } else {
            log_->e("RendererImpl: Render caption region failed: ", static_cast<int>(result.error()));
            return Renderer::kRenderStatusError;
        }
    }

    has_prev_rendered_caption_ = true;
    prev_rendered_caption_pts_ = caption.pts;
    prev_rendered_caption_duration_ = caption.duration;
    prev_rendered_images_ = std::move(images);

    output_cb(caption.pts, caption.duration, prev_rendered_images_);
    return Renderer::kRenderStatusGotImage;
}

void RendererImpl::AdjustCaptionArea(int origin_plane_width, int origin_plane_height) {
    float x_magnification = static_cast<float>(video_area_width_) / static_cast<float>(origin_plane_width);
    float y_magnification = static_cast<float>(video_area_height_) / static_cast<float>(origin_plane_height);
    float magnification = std::min(x_magnification, y_magnification);

    int caption_area_width = static_cast<int>(std::floor(static_cast<float>(origin_plane_width) * magnification));
    int caption_area_height = static_cast<int>(std::floor(static_cast<float>(origin_plane_height) * magnification));
    int caption_area_start_x = (video_area_width_ - caption_area_width) / 2;
    int caption_area_start_y = (video_area_height_ - caption_area_height) / 2;

    Rect caption_area(caption_area_start_x,
                      caption_area_start_y,
                      caption_area_start_x + caption_area_width,
                      caption_area_start_y + caption_area_height);

    region_renderer_.SetOriginPlaneSize(origin_plane_width, origin_plane_height);
    region_renderer_.SetTargetCaptionAreaRect(caption_area);
}

bool RendererImpl::Flush() {
    captions_.clear();
    InvalidatePrevRenderedImages();
    return true;
}

void RendererImpl::InvalidatePrevRenderedImages() {
    has_prev_rendered_caption_ = false;
    prev_rendered_caption_pts_ = PTS_NOPTS;
    prev_rendered_caption_duration_ = 0;
    prev_rendered_images_.clear();
}

}  // namespace aribcaption::internal
