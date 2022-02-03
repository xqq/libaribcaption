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

#ifndef ARIBCAPTION_TEXT_RENDERER_HPP
#define ARIBCAPTION_TEXT_RENDERER_HPP

#include <memory>
#include <optional>
#include "aribcaption/caption.hpp"
#include "aribcaption/context.hpp"
#include "aribcaption/renderer.hpp"
#include "base/result.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/font_provider.hpp"

namespace aribcaption {

struct UnderlineInfo {
    int start_x = 0;
    int width = 0;
};

enum class TextRenderStatus {
    kOK,
    kFontNotFound,
    kCodePointNotFound,
    kOtherError
};

enum class TextRenderFallbackPolicy {
    kAutoFallback,
    kFailOnCodePointNotFound
};

class TextRenderContext {
public:
    struct ContextPrivate {
    public:
        ContextPrivate() = default;
        virtual ~ContextPrivate() = default;
    };
public:
    explicit TextRenderContext(Bitmap& bmp) : bitmap_(&bmp) {}
    TextRenderContext(Bitmap& bmp, std::unique_ptr<ContextPrivate> priv) : bitmap_(&bmp), priv_(std::move(priv)) {}
    ~TextRenderContext() = default;

    [[nodiscard]]
    Bitmap& GetBitmap() const {
        return *bitmap_;
    }

    [[nodiscard]]
    ContextPrivate* GetPrivate() const {
        return priv_.get();
    }
public:
    // Disallow copy and assign
    TextRenderContext(const TextRenderContext&) = delete;
    TextRenderContext& operator=(const TextRenderContext&) = delete;
    // Allow move construct / move assignment
    TextRenderContext(TextRenderContext&&) noexcept = default;
    TextRenderContext& operator=(TextRenderContext&&) noexcept = default;
private:
    Bitmap* bitmap_ = nullptr;  // Should be non-null
    std::unique_ptr<ContextPrivate> priv_;
};

class TextRenderer {
public:
    static std::unique_ptr<TextRenderer> Create(TextRendererType type, Context& context, FontProvider& font_provider);
protected:
    static auto FontProviderErrorToStatus(FontProviderError error) -> TextRenderStatus;
public:
    TextRenderer() = default;
    virtual ~TextRenderer() = default;
public:
    virtual bool Initialize() = 0;
    virtual void SetLanguage(uint32_t iso6392_language_code) = 0;
    virtual bool SetFontFamily(const std::vector<std::string>& font_family) = 0;
    virtual auto BeginDraw(Bitmap& target_bmp) -> TextRenderContext = 0;
    virtual void EndDraw(TextRenderContext& context) = 0;
    virtual auto DrawChar(TextRenderContext& render_ctx, int x, int y,
                          uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                          float stroke_width, int char_width, int char_height,
                          std::optional<UnderlineInfo> underline_info,
                          TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus = 0;
public:
    // Disallow copy and assign
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_TEXT_RENDERER_HPP
