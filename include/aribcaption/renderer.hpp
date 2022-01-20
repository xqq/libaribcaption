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

#ifndef ARIBCAPTION_RENDERER_HPP
#define ARIBCAPTION_RENDERER_HPP

#include <memory>
#include <optional>
#include "aribcc_config.h"
#include "aribcc_export.h"
#include "context.hpp"
#include "caption.hpp"
#include "image.hpp"

namespace aribcaption {

enum class FontProviderType {
    kAuto = 0,
#if defined(ARIBCC_USE_CORETEXT)
    kCoreText = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    kDirectWrite = 2,
#endif

#if defined(ARIBCC_USE_FONTCONFIG)
    kFontconfig = 3,
#endif

#if defined(ARIBCC_IS_ANDROID)
    kAndroid = 4,
#endif
};

enum class TextRendererType {
    kAuto = 0,
#if defined(ARIBCC_USE_CORETEXT)
    kCoreText = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    kDirectWrite = 2,
#endif

#if defined(ARIBCC_USE_FREETYPE)
    kFreetype = 3,
#endif
};

namespace internal { class RendererImpl; }

enum class CaptionStoragePolicy {
    kMinimum = 0,
    kUnlimited = 1,
    kUpperLimitCount = 2,
    kUpperLimitDuration = 3,
};

enum class RenderStatus {
    kError = 0,
    kNoImage = 1,
    kGotImage = 2,
    kGotImageUnchanged = 3,
};

struct RenderResult {
    int64_t pts = 0;
    int64_t duration = 0;
    std::vector<Image> images;
};

class Renderer {
public:
    ARIBCC_API explicit Renderer(Context& context);
    ARIBCC_API ~Renderer();
    ARIBCC_API Renderer(Renderer&&) noexcept;
    ARIBCC_API Renderer& operator=(Renderer&&) noexcept;
public:
    ARIBCC_API bool Initialize(CaptionType caption_type = CaptionType::kCaption,
                               FontProviderType font_provider_type = FontProviderType::kAuto,
                               TextRendererType text_renderer_type = TextRendererType::kAuto);

    ARIBCC_API void SetStrokeWidth(float dots);
    ARIBCC_API void SetReplaceDRCS(bool replace);
    ARIBCC_API void SetForceStrokeText(bool force_stroke);
    ARIBCC_API void SetForceNoRuby(bool force_no_ruby);
    ARIBCC_API void SetForceNoBackground(bool force_no_background);

    ARIBCC_API bool SetDefaultFontFamily(const std::vector<std::string>& font_family, bool force_default);
    ARIBCC_API bool SetLanguageSpecificFontFamily(uint32_t language_code, const std::vector<std::string>& font_family);
    ARIBCC_API bool SetFrameSize(int frame_width, int frame_height);
    ARIBCC_API bool SetMargins(int top, int bottom, int left, int right);

    ARIBCC_API void SetStoragePolicy(CaptionStoragePolicy policy, std::optional<size_t> upper_limit = std::nullopt);

    ARIBCC_API bool AppendCaption(const Caption& caption);
    ARIBCC_API bool AppendCaption(Caption&& caption);

    ARIBCC_API RenderStatus Render(int64_t pts, RenderResult& out_result);
    ARIBCC_API bool Flush();
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
private:
    std::unique_ptr<internal::RendererImpl> pimpl_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_RENDERER_HPP
