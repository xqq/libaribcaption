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

#ifndef ARIBCAPTION_REGION_RENDERER_HPP
#define ARIBCAPTION_REGION_RENDERER_HPP

#include <cmath>
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "aribcaption/caption.hpp"
#include "aribcaption/context.hpp"
#include "aribcaption/image.hpp"
#include "base/logger.hpp"
#include "base/result.hpp"
#include "renderer/drcs_renderer.hpp"
#include "renderer/font_provider.hpp"
#include "renderer/rect.hpp"
#include "renderer/text_renderer.hpp"

namespace aribcaption {

enum class RegionRenderError {
    kFontNotFound,
    kCodePointNotFound,
    kImageTooSmall,
    kOtherError,
};

class RegionRenderer {
public:
    explicit RegionRenderer(Context& context);
    ~RegionRenderer() = default;
public:
    bool Initialize(FontProviderType font_provider_type = FontProviderType::kAuto,
                    TextRendererType text_renderer_type = TextRendererType::kAuto);
    void SetFontLanguage(uint32_t iso6392_language_code);
    bool SetFontFamily(const std::vector<std::string>& font_family);
    void SetOriginalPlaneSize(int plane_width, int plane_height);
    void SetTargetCaptionAreaRect(const Rect& rect);
    void SetStrokeWidth(float dots);
    void SetReplaceDRCS(bool replace);
    void SetForceStrokeText(bool force_stroke);
    void SetForceNoBackground(bool force_no_background);
    auto RenderCaptionRegion(const CaptionRegion& region,
                             const std::unordered_map<uint32_t, DRCS>& drcs_map) -> Result<Image, RegionRenderError>;
private:
    template <typename T>
    [[nodiscard]]
    int ScaleX(T x) const {
        return static_cast<int>(std::floor(static_cast<float>(x) * x_magnification_));
    }

    template <typename T>
    [[nodiscard]]
    int ScaleY(T y) const {
        return static_cast<int>(std::floor(static_cast<float>(y) * y_magnification_));
    }

    template <typename T>
    [[nodiscard]]
    int ScaleWidth(T width, T x = 0) const {
        return ScaleX(x + width) - ScaleX(x);
    }

    template <typename T>
    [[nodiscard]]
    int ScaleHeight(T height, T y = 0) const {
        return ScaleY(y + height) - ScaleY(y);
    }
public:
    RegionRenderer(const RegionRenderer&) = delete;
    RegionRenderer& operator=(const RegionRenderer&) = delete;
private:
    Context& context_;
    std::shared_ptr<Logger> log_;

    std::unique_ptr<FontProvider> font_provider_;
    std::unique_ptr<TextRenderer> text_renderer_;
    DRCSRenderer drcs_renderer_;

    bool plane_inited_ = false;
    int plane_width_ = 0;
    int plane_height_ = 0;

    bool caption_area_inited_ = false;
    int caption_area_start_x_ = 0;
    int caption_area_start_y_ = 0;
    int caption_area_width_ = 0;
    int caption_area_height_ = 0;

    float stroke_width_ = 1.5f;
    bool replace_drcs_ = true;
    bool force_stroke_text_ = false;
    bool force_no_background_ = false;

    float x_magnification_ = 0.0f;
    float y_magnification_ = 0.0f;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_REGION_RENDERER_HPP
