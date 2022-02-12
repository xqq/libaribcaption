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

#ifndef ARIBCAPTION_FONT_PROVIDER_HPP
#define ARIBCAPTION_FONT_PROVIDER_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include "aribcaption/context.hpp"
#include "aribcaption/renderer.hpp"
#include "base/result.hpp"

namespace aribcaption {

struct FontfaceInfoPrivate {
public:
    FontfaceInfoPrivate() = default;
    virtual ~FontfaceInfoPrivate() = default;
};

struct FontfaceInfo {
    std::string family_name;
    std::string postscript_name;
    std::string filename;
    int face_index = 0;
    std::vector<uint8_t> font_data;
    FontProviderType provider_type = FontProviderType::kAuto;
    std::unique_ptr<FontfaceInfoPrivate> provider_priv;
};

enum class FontProviderError {
    kFontNotFound,
    kCodePointNotFound,
    kOtherError,
};

class FontProvider {
public:
    static std::unique_ptr<FontProvider> Create(FontProviderType type, Context& context);
public:
    FontProvider() = default;
    virtual ~FontProvider() = default;
public:
    virtual FontProviderType GetType() = 0;
    virtual bool Initialize() = 0;
    virtual void SetLanguage(uint32_t iso6392_language_code) = 0;
    virtual Result<FontfaceInfo, FontProviderError> GetFontFace(const std::string& font_name,
                                                                std::optional<uint32_t> ucs4) = 0;
public:
    // Disallow copy and assign
    FontProvider(const FontProvider&) = delete;
    FontProvider& operator=(const FontProvider&) = delete;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_FONT_PROVIDER_HPP
