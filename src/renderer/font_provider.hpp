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

namespace aribcaption {

enum FontProviderType {
    kFontProviderFontconfig = 0,
    kFontProviderDirectWrite,
    kFontProviderCoreText,
    kFontProviderDefault = kFontProviderFontconfig,
};

struct FontProviderPrivate {
    int placeholder_ = 0;
public:
    FontProviderPrivate() = default;
    virtual ~FontProviderPrivate() = default;
};

struct FontfaceInfo {
    std::string filename;
    int face_index = 0;
    FontProviderType provider_type = kFontProviderDefault;
    std::unique_ptr<FontProviderPrivate> provider_priv;
};

class IFontProvider {
public:
    virtual ~IFontProvider() = 0;
public:
    virtual bool Initialize() = 0;
    virtual std::optional<FontfaceInfo> GetFontFace(const std::string& font_name, std::optional<uint32_t> ucs4) = 0;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_FONT_PROVIDER_HPP
