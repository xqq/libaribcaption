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

#include <memory>
#include "aribcc_config.h"
#include "renderer/font_provider.hpp"

#if defined(ARIBCC_USE_CORETEXT)
    #include "renderer/font_provider_coretext.hpp"
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    #include "renderer/font_provider_directwrite.hpp"
#endif

#if defined(ARIBCC_USE_FONTCONFIG)
    #include "renderer/font_provider_fontconfig.hpp"
#endif

#if defined(ARIBCC_IS_ANDROID)
    #include "renderer/font_provider_android.hpp"
#endif

#if defined(ARIBCC_USE_GDI_FONT)
    #include "renderer/font_provider_gdi.hpp"
#endif

namespace aribcaption {

std::unique_ptr<FontProvider> FontProvider::Create(FontProviderType type, Context& context) {
    switch (type) {
#if defined(ARIBCC_USE_CORETEXT)
        case FontProviderType::kCoreText:
            return std::make_unique<FontProviderCoreText>(context);
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
        case FontProviderType::kDirectWrite:
            return std::make_unique<FontProviderDirectWrite>(context);
#endif

#if defined(ARIBCC_USE_FONTCONFIG)
        case FontProviderType::kFontconfig:
            return std::make_unique<FontProviderFontconfig>(context);
#endif

#if defined(ARIBCC_IS_ANDROID)
        case FontProviderType::kAndroid:
            return std::make_unique<FontProviderAndroid>(context);
#endif

#if defined(ARIBCC_USE_GDI_FONT)
        case FontProviderType::kGDI:
            return std::make_unique<FontProviderGDI>(context);
#endif

        case FontProviderType::kAuto:
        default:
#if defined(_WIN32) && defined(ARIBCC_USE_DIRECTWRITE)
            return std::make_unique<FontProviderDirectWrite>(context);
#elif defined(_WIN32) && defined(ARIBCC_USE_GDI_FONT)
            return std::make_unique<FontProviderGDI>(context);
#elif defined(__APPLE__) && defined(ARIBCC_USE_CORETEXT)
            return std::make_unique<FontProviderCoreText>(context);
#elif defined(ARIBCC_IS_ANDROID)
            return std::make_unique<FontProviderAndroid>(context);
#elif defined(ARIBCC_USE_FONTCONFIG)
            return std::make_unique<FontProviderFontconfig>(context);
#else
            static_assert(false, "No available auto-select FontProvider!");
#endif
    }
}

}  // namespace aribcaption
