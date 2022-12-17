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
#include <iterator>
#include "aribcaption/context.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
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
#elif defined(__ANDROID__)
    jpn_default_font_family = {
        "sans-serif"
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
    InvalidatePrevRenderedImages();
}

void RendererImpl::SetReplaceDRCS(bool replace) {
    region_renderer_.SetReplaceDRCS(replace);
    InvalidatePrevRenderedImages();
}

void RendererImpl::SetForceStrokeText(bool force_stroke) {
    region_renderer_.SetForceStrokeText(force_stroke);
    InvalidatePrevRenderedImages();
}

void RendererImpl::SetForceNoRuby(bool force_no_ruby) {
    force_no_ruby_ = force_no_ruby;
    InvalidatePrevRenderedImages();
}

void RendererImpl::SetForceNoBackground(bool force_no_background) {
    region_renderer_.SetForceNoBackground(force_no_background);
    InvalidatePrevRenderedImages();
}

void RendererImpl::SetMergeRegionImages(bool merge) {
    bool prev = merge_region_images_;
    merge_region_images_ = merge;
    if (prev != merge) {
        InvalidatePrevRenderedImages();
    }
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
    if (frame_width < 0 || frame_height < 0) {
        assert(frame_width >= 0 && frame_height >= 0 && "Frame width/height must >= 0");
        return false;
    }

    if (frame_width_ != frame_width || frame_height_ != frame_height) {
        InvalidatePrevRenderedImages();
    }

    frame_width_ = frame_width;
    frame_height_ = frame_height;
    frame_size_inited_ = true;

    SetMargins(margin_top_, margin_bottom_, margin_left_, margin_right_);

    return true;
}

bool RendererImpl::SetMargins(int top, int bottom, int left, int right) {
    if (!frame_size_inited_) {
        assert(frame_size_inited_ && "Frame size is not indicated, call SetFrameSize() first");
        return false;
    }

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

void RendererImpl::SetStoragePolicy(CaptionStoragePolicy policy, std::optional<size_t> upper_limit) {
    storage_policy_ = policy;

    if (policy == CaptionStoragePolicy::kUpperLimitCount) {
        assert(upper_limit.has_value());
        upper_limit_count_ = upper_limit.value();
    } else if (policy == CaptionStoragePolicy::kUpperLimitDuration) {
        assert(upper_limit.has_value());
        upper_limit_duration_ = upper_limit.value();
    }
}

bool RendererImpl::AppendCaption(const Caption& caption) {
    assert(caption.pts != PTS_NOPTS && "Caption without PTS is not supported");
    assert(caption.plane_width > 0 && caption.plane_height > 0);

    if (caption.pts == PTS_NOPTS || caption.plane_width <= 0 || caption.plane_height <= 0) {
        return false;
    }

    int64_t pts = caption.pts;

    if (captions_.empty()) {
        captions_.emplace(pts, caption);
    } else {
        auto prev = captions_.lower_bound(pts - 1);
        if (prev == captions_.end() || (prev != captions_.begin() && prev->first > pts - 1)) {
            --prev;
        }

        // Correct previous caption's duration
        if (prev->first < pts && prev->second.wait_duration == DURATION_INDEFINITE) {
            Caption& prev_caption = prev->second;
            prev_caption.wait_duration = pts - prev_caption.pts;
        }

        captions_.insert_or_assign(std::next(prev), pts, caption);
    }

    if (pts <= prev_rendered_caption_pts_) {
        InvalidatePrevRenderedImages();
    }

    CleanupCaptionsIfNecessary();
    return true;
}

bool RendererImpl::AppendCaption(Caption&& caption) {
    assert(caption.pts != PTS_NOPTS && "Caption without PTS is not supported");
    assert(caption.plane_width > 0 && caption.plane_height > 0);

    if (caption.pts == PTS_NOPTS || caption.plane_width <= 0 || caption.plane_height <= 0) {
        return false;
    }

    int64_t pts = caption.pts;

    if (captions_.empty()) {
        captions_.emplace(pts, std::move(caption));
    } else {
        auto prev = captions_.lower_bound(pts - 1);
        if (prev == captions_.end() || (prev != captions_.begin() && prev->first > pts - 1)) {
            --prev;
        }

        // Correct previous caption's duration
        if (prev->first < pts && prev->second.wait_duration == DURATION_INDEFINITE) {
            Caption& prev_caption = prev->second;
            prev_caption.wait_duration = pts - prev_caption.pts;
        }

        captions_.insert_or_assign(std::next(prev), pts, std::move(caption));
    }

    if (pts <= prev_rendered_caption_pts_) {
        InvalidatePrevRenderedImages();
    }

    CleanupCaptionsIfNecessary();
    return true;
}

void RendererImpl::CleanupCaptionsIfNecessary() {
    if (storage_policy_ == CaptionStoragePolicy::kUnlimited) {
        return;
    } else if (storage_policy_ == CaptionStoragePolicy::kMinimum) {
        if (prev_rendered_caption_pts_ == PTS_NOPTS) {
            return;
        }
        auto prev_rendered_caption_iter = captions_.find(prev_rendered_caption_pts_);
        if (prev_rendered_caption_iter != captions_.end()) {
            captions_.erase(captions_.begin(), prev_rendered_caption_iter);
        }
    } else if (storage_policy_ == CaptionStoragePolicy::kUpperLimitCount) {
        if (captions_.size() <= upper_limit_count_) {
            return;
        }
        auto erase_end = std::prev(captions_.end(), static_cast<ptrdiff_t>(upper_limit_count_));
        if (erase_end != captions_.begin()) {
            captions_.erase(captions_.begin(), erase_end);
        }
    } else if (storage_policy_ == CaptionStoragePolicy::kUpperLimitDuration) {
        if (captions_.empty()) {
            return;
        }
        int64_t last_caption_pts = captions_.rbegin()->first;
        int64_t erase_end_pts = last_caption_pts - static_cast<int64_t>(upper_limit_duration_);
        auto erase_end = captions_.lower_bound(erase_end_pts);
        if (erase_end != captions_.end() && erase_end != captions_.begin()) {
            captions_.erase(captions_.begin(), erase_end);
        }
    }
}

RenderStatus RendererImpl::TryRender(int64_t pts) {
    if (!frame_size_inited_ || !margins_inited_) {
        return RenderStatus::kError;
    }

    if (captions_.empty()) {
        return RenderStatus::kNoImage;
    }

    auto iter = captions_.lower_bound(pts);
    if (iter == captions_.end() || (iter != captions_.begin() && iter->first > pts)) {
        --iter;
    }

    Caption& caption = iter->second;
    if (pts < caption.pts || (caption.wait_duration != DURATION_INDEFINITE && pts >= caption.pts + caption.wait_duration)) {
        // Timeout
        return RenderStatus::kNoImage;
    }
    if (caption.regions.empty()) {
        return RenderStatus::kNoImage;
    }

    if (has_prev_rendered_caption_ && prev_rendered_caption_pts_ == caption.pts) {
        if (!prev_rendered_images_.empty()) {
            return RenderStatus::kGotImageUnchanged;
        } else {
            return RenderStatus::kNoImage;
        }
    }

    return RenderStatus::kGotImage;
}

RenderStatus RendererImpl::Render(int64_t pts, RenderResult& out_result) {
    if (!frame_size_inited_ || !margins_inited_) {
        assert(frame_size_inited_ && margins_inited_ && "Frame size / margins must be indicated first");
        return RenderStatus::kError;
    }

    out_result.pts = 0;
    out_result.duration = 0;
    out_result.images.clear();

    if (captions_.empty()) {
        InvalidatePrevRenderedImages();
        return RenderStatus::kNoImage;
    }

    auto iter = captions_.lower_bound(pts);
    if (iter == captions_.end() || (iter != captions_.begin() && iter->first > pts)) {
        --iter;
    }

    Caption& caption = iter->second;
    if (pts < caption.pts || (caption.wait_duration != DURATION_INDEFINITE && pts >= caption.pts + caption.wait_duration)) {
        // Timeout
        InvalidatePrevRenderedImages();
        return RenderStatus::kNoImage;
    }
    if (caption.regions.empty()) {
        InvalidatePrevRenderedImages();
        return RenderStatus::kNoImage;
    }

    if (has_prev_rendered_caption_ && prev_rendered_caption_pts_ == caption.pts) {
        // Reuse previous rendered caption
        if (!prev_rendered_images_.empty()) {
            out_result.pts = prev_rendered_caption_pts_;
            out_result.duration = prev_rendered_caption_duration_;
            out_result.images = prev_rendered_images_;
            return RenderStatus::kGotImageUnchanged;
        } else {
            InvalidatePrevRenderedImages();
            return RenderStatus::kNoImage;
        }
    }

    // Prepare for rendering

    // Set up Font Language
    region_renderer_.SetFontLanguage(caption.iso6392_language_code);

    // Set up Font Family
    uint32_t language_code = caption.iso6392_language_code;
    if (force_default_font_family_ || language_font_family_.find(language_code) == language_font_family_.end()) {
        language_code = 0;
    }
    region_renderer_.SetFontFamily(language_font_family_[language_code]);

    // Set up origin plane size / target caption area
    AdjustCaptionArea(caption.plane_width, caption.plane_height);

    std::vector<Image> images;
    for (CaptionRegion& region : caption.regions) {
        if (region.is_ruby && force_no_ruby_) {
            continue;
        }

        Result<Image, RegionRenderError> result = region_renderer_.RenderCaptionRegion(region, caption.drcs_map);
        if (result.is_ok()) {
            images.push_back(std::move(result.value()));
        } else if (result.error() == RegionRenderError::kImageTooSmall) {
            // Skip image which is too small
            continue;
        } else {
            log_->e("RendererImpl: RenderCaptionRegion() failed with error: %d", static_cast<int>(result.error()));
            InvalidatePrevRenderedImages();
            return RenderStatus::kError;
        }
    }

    if (merge_region_images_ && images.size() > 1) {
        Image merged = MergeImages(images);
        images.clear();
        images.push_back(std::move(merged));
    }

    has_prev_rendered_caption_ = true;
    prev_rendered_caption_pts_ = caption.pts;
    prev_rendered_caption_duration_ = caption.wait_duration;
    prev_rendered_images_ = std::move(images);

    out_result.pts = caption.pts;
    out_result.duration = caption.wait_duration;
    out_result.images = prev_rendered_images_;
    return RenderStatus::kGotImage;
}

Image RendererImpl::MergeImages(std::vector<Image>& images) {
    if (images.empty()) return Image{};

    Rect rect(images[0].dst_x, images[0].dst_y, images[0].dst_x, images[0].dst_y);

    for (auto& image : images) {
        rect.Include(image.dst_x, image.dst_y);  // top left corner
        rect.Include(image.dst_x + image.width - 1, image.dst_y + image.height - 1);  // bottom right corner
    }

    Bitmap bitmap(rect.width(), rect.height(), PixelFormat::kRGBA8888);
    Canvas canvas(bitmap);

    for (auto& image : images) {
        int x = image.dst_x - rect.left;
        int y = image.dst_y - rect.top;
        Bitmap bmp = Bitmap::FromImage(std::move(image));
        canvas.DrawBitmap(bmp, x, y);
    }

    Image merged = Bitmap::ToImage(std::move(bitmap));
    merged.dst_x = rect.left;
    merged.dst_y = rect.top;
    return merged;
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

    region_renderer_.SetOriginalPlaneSize(origin_plane_width, origin_plane_height);
    region_renderer_.SetTargetCaptionAreaRect(caption_area);
}

void RendererImpl::Flush() {
    captions_.clear();
    InvalidatePrevRenderedImages();
}

void RendererImpl::InvalidatePrevRenderedImages() {
    has_prev_rendered_caption_ = false;
    prev_rendered_caption_pts_ = PTS_NOPTS;
    prev_rendered_caption_duration_ = 0;
    prev_rendered_images_.clear();
}

}  // namespace aribcaption::internal
