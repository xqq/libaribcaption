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
#include "context.hpp"
#include "caption.hpp"
#include "image.hpp"

namespace aribcaption {

enum class FontProviderType {
    kAuto = 0,
    kCoreText,
    kDirectWrite,
    kFontconfig,
};

enum class TextRendererType {
    kAuto = 0,
    kCoreText,
    kDirectWrite,
    kFreetype,
};

class RendererImpl;

class Renderer {
public:
    using OutputCB = std::function<void(int64_t pts, int64_t duration, const std::vector<Image>& images)>;

    enum RenderStatus {
        kRenderStatusError = 0,
        kRenderStatusNoImage = 1,
        kRenderStatusGotImage = 2,
        kRenderStatusGotImageUnchanged = 3,
    };
public:
    explicit Renderer(Context& context);
    ~Renderer();
public:
    bool Initialize(CaptionType caption_type = CaptionType::kCaption,
                    FontProviderType font_provider_type = FontProviderType::kAuto,
                    TextRendererType text_renderer_type = TextRendererType::kAuto);
    bool SetFrameSize(int frame_width, int frame_height);
    bool SetMargins(int top, int bottom, int left, int right);

    bool AppendCaption(const Caption& caption);
    bool AppendCaption(Caption&& caption);

    Renderer::RenderStatus Render(int64_t pts, const Renderer::OutputCB& output_cb);
    bool Flush();
public:
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;
private:
    std::unique_ptr<RendererImpl> pimpl_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_RENDERER_HPP
