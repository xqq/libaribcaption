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
#include <vector>
#include <string>
#include <optional>
#include <utility>
#include "caption.hpp"
#include "color.hpp"
#include "context.hpp"
#include "base/logger.hpp"
#include "base/result.hpp"
#include "base/scoped_holder.hpp"
#include "renderer/bitmap.hpp"
#include "renderer/font_provider.hpp"
#include "renderer/text_renderer.hpp"

namespace aribcaption {

class TextRendererFreetype : public TextRenderer {
public:
    TextRendererFreetype(Context& context, FontProvider& font_provider);
    ~TextRendererFreetype() override;
public:
    bool Initialize() override;
    bool SetFontFamily(const std::vector<std::string>& font_family) override;
    auto DrawChar(uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color, int stroke_width,
                  int char_width,
                  int char_height,
                  Bitmap& target_bmp,
                  int x,
                  int y,
                  std::optional<UnderlineInfo> underline_info) -> TextRenderStatus override;
private:
    auto LoadFontFace(std::optional<uint32_t> codepoint = std::nullopt,
                      std::optional<size_t> begin_index = std::nullopt)
        -> Result<std::pair<FT_Face, size_t>, FontProviderError>;  // Result<Pair<face, font_index>, error>
private:
    static TextRenderStatus FontProviderErrorToStatus(FontProviderError error);
private:
    std::shared_ptr<Logger> log_;

    FontProvider& font_provider_;
    std::vector<std::string> font_family_;

    ScopedHolder<FT_Library> library_;
    ScopedHolder<FT_Face> main_face_;
    ScopedHolder<FT_Face> fallback_face_;
    size_t main_face_index_ = 0;
    size_t fallback_face_index_ = 0;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_TEXT_RENDERER_FREETYPE_HPP
