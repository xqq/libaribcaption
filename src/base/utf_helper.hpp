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

#ifndef ARIBCAPTION_UTF_HELPER_HPP
#define ARIBCAPTION_UTF_HELPER_HPP

#include <cstdint>
#include <string>

namespace aribcaption::utf {

inline size_t UTF8AppendCodePoint(std::string& u8str, uint32_t ucs4) {
    if (ucs4 < 0x80) {
        u8str.push_back(static_cast<char>(ucs4));
        return 1;
    } else if (ucs4 < 0x800) {
        u8str.append({
            static_cast<char>(0xC0 | (ucs4 >> 6)),   // 110xxxxx
            static_cast<char>(0x80 | (ucs4 & 0x3F))  // 10xxxxxx
        });
        return 2;
    } else if (ucs4 < 0x10000) {
        u8str.append({
            static_cast<char>(0xE0 | (ucs4 >> 12)),          // 1110xxxx
            static_cast<char>(0x80 | ((ucs4 >> 6) & 0x3F)),  // 10xxxxxx
            static_cast<char>(0x80 | (ucs4 & 0x3F))          // 10xxxxxx
        });
        return 3;
    } else if (ucs4 < 0x110000) {
        u8str.append({
            static_cast<char>(0xF0 | (ucs4 >> 18)),           // 11110xxx
            static_cast<char>(0x80 | ((ucs4 >> 12) & 0x3F)),  // 10xxxxxx
            static_cast<char>(0x80 | ((ucs4 >> 6) & 0x3F)),   // 10xxxxxx
            static_cast<char>(0x80 | (ucs4 & 0x3F)),          // 10xxxxxx
        });
        return 4;
    }

    // Invalid ucs4
    return 0;
}

inline size_t UTF16AppendCodePoint(std::u16string& u16str, uint32_t ucs4) {
    if (ucs4 < 0x10000) {
        u16str.push_back(static_cast<char16_t>(ucs4));
        return 1;
    } else if (ucs4 < 0x110000) {
        u16str.append({
            static_cast<char16_t>((0xD800 - 64) + (ucs4 >> 10)),
            static_cast<char16_t>(0xDC00 | (ucs4 & 0x3FF))
        });
        return 2;
    }

    // Invalid ucs4
    return 0;
}

}  // namespace aribcaption::utf

#endif  // ARIBCAPTION_UTF_HELPER_HPP
