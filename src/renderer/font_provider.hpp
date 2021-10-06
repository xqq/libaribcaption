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
#include <memory>
#include <optional>
#include "context.hpp"
#include "base/result.hpp"

namespace aribcaption {

enum class FontProviderType {
    kFontProviderFontconfig = 0,
    kFontProviderDirectWrite,
    kFontProviderCoreText,
    kFontProviderDefault = kFontProviderFontconfig,
};

struct FontProviderPrivate {
public:
    FontProviderPrivate() = default;
    virtual ~FontProviderPrivate() = default;
private:
    int placeholder_ = 0;
};

struct FontfaceInfo {
    std::string filename;
    int face_index = 0;
    FontProviderType provider_type = FontProviderType::kFontProviderDefault;
    std::unique_ptr<FontProviderPrivate> provider_priv;
};

enum class FontProviderError {
    kFontNotFound,
    kCodePointNotFound,
    kOtherError,
};

class IFontProvider {
public:
    IFontProvider() = default;
    virtual ~IFontProvider() = default;
public:
    virtual bool Initialize() = 0;
    virtual Result<FontfaceInfo, FontProviderError> GetFontFace(const std::string& font_name,
                                                                std::optional<uint32_t> ucs4) = 0;
public:
    // Disallow copy and assign
    IFontProvider(const IFontProvider&) = delete;
    IFontProvider& operator=(const IFontProvider&) = delete;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_FONT_PROVIDER_HPP
