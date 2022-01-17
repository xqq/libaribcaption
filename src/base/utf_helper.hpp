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

#include <cstddef>
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

inline bool IsUTF8Continuation(uint8_t u8) {
    return (u8 & 0xC0) == 0x80;
}

inline uint32_t DecodeUTF8ToCodePoint(const uint8_t* str, size_t bytes_available, size_t* bytes_processed) {
    if (!bytes_available) {
        *bytes_processed = 0;
        return 0;
    }

    uint32_t ucs4 = 0xFFFD;

    if (str[0] < 0x80) {  // 1-byte UTF-8 character (ASCII)
        ucs4 = str[0];
        *bytes_processed = 1;
    } else if (str[0] < 0xC2) {  // Invalid UTF-8 character
        *bytes_processed = 1;
        // Invalid UTF-8, fallthrough
    } else if (str[0] < 0xE0) {  // 2-byte UTF-8 character
        if (bytes_available >= 2 && IsUTF8Continuation(str[1])) {
            ucs4 = (uint32_t)(str[0] & 0b11111) << 6 | (uint32_t)(str[1] & 0b111111);
            *bytes_processed = 2;
        } else {
            *bytes_processed = 1;
        }
    } else if (str[0] < 0xF0) {  // 3-byte UTF-8 character
        if (bytes_available >= 3 && IsUTF8Continuation(str[1]) && IsUTF8Continuation(str[2])) {
            ucs4 = (uint32_t)(str[0] & 0b001111) << 12 |
                   (uint32_t)(str[1] & 0b111111) <<  6 |
                   (uint32_t)(str[2] & 0b111111) <<  0;
            *bytes_processed = 3;
        } else {
            *bytes_processed = 1;
        }
    } else if (str[0] < 0xF8) {  // 4-byte UTF-8 character
        if (bytes_available >= 4 &&
                IsUTF8Continuation(str[1]) && IsUTF8Continuation(str[2]) && IsUTF8Continuation((str[3]))) {
            ucs4 = (uint32_t)(str[0] & 0b000111) << 18 |
                   (uint32_t)(str[1] & 0b111111) << 12 |
                   (uint32_t)(str[2] & 0b111111) <<  6 |
                   (uint32_t)(str[3] & 0b111111) <<  0;
            *bytes_processed = 4;
        } else {
            *bytes_processed = 1;
        }
    }

    return ucs4;
}

}  // namespace aribcaption::utf

#endif  // ARIBCAPTION_UTF_HELPER_HPP
