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

#ifndef ARIBCAPTION_FONT_PROVIDER_DIRECTWRITE_HPP
#define ARIBCAPTION_FONT_PROVIDER_DIRECTWRITE_HPP

#include <wrl/client.h>
#include <dwrite.h>
#include <memory>
#include "aribcaption/context.hpp"
#include "base/logger.hpp"
#include "renderer/font_provider.hpp"

using Microsoft::WRL::ComPtr;

namespace aribcaption {

struct FontfaceInfoPrivateDirectWrite : public FontfaceInfoPrivate {
public:
    FontfaceInfoPrivateDirectWrite() = default;
    ~FontfaceInfoPrivateDirectWrite() override = default;
public:
    ComPtr<IDWriteFont> font;
    ComPtr<IDWriteFontFace> fontface;
};

class FontProviderDirectWrite : public FontProvider {
public:
    explicit FontProviderDirectWrite(Context& context);
    ~FontProviderDirectWrite() override = default;
public:
    FontProviderType GetType() override;
    bool Initialize() override;
    void SetLanguage(uint32_t iso6392_language_code) override;
    Result<FontfaceInfo, FontProviderError> GetFontFace(const std::string& font_name,
                                                        std::optional<uint32_t> ucs4) override;
public:
    ComPtr<IDWriteFactory> GetDWriteFactory();
private:
    std::shared_ptr<Logger> log_;

    uint32_t iso6392_language_code_ = 0;

    ComPtr<IDWriteFactory> dwrite_factory_;
    ComPtr<IDWriteGdiInterop> dwrite_gdi_interop_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_FONT_PROVIDER_DIRECTWRITE_HPP
