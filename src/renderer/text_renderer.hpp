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

#include <optional>
#include <caption.hpp>
#include "base/result.hpp"
#include "renderer/bitmap.hpp"

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

class TextRenderer {
public:
    TextRenderer() = default;
    virtual ~TextRenderer() = default;
public:
    virtual bool Initialize() = 0;
    virtual bool SetFontFamily(const std::vector<std::string>& font_family) = 0;
    virtual auto DrawChar(uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color, int stroke_width,
                          int char_width,
                          int char_height,
                          Bitmap& target_bmp,
                          int x,
                          int y,
                          std::optional<UnderlineInfo> underline_info) -> TextRenderStatus = 0;
public:
    // Disallow copy and assign
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_TEXT_RENDERER_HPP
