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

#ifndef ARIBCAPTION_LANGUAGE_CODE_HPP
#define ARIBCAPTION_LANGUAGE_CODE_HPP

#include <cstdint>
#include "aribcaption/caption.hpp"

namespace aribcaption {

inline const char* ISO6392ToISO6391LanguageString(uint32_t iso6392_language_code) {
    switch (iso6392_language_code) {
        case ThreeCC("por"):
            return "pt";
        case ThreeCC("spa"):
            return "es";
        case ThreeCC("eng"):
            return "en";
        case ThreeCC("jpn"):
        default:
            return "ja";
    }
}

}  // namespace aribcaption

#endif  // ARIBCAPTION_LANGUAGE_CODE_HPP
