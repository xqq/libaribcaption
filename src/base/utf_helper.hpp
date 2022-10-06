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

inline size_t UTF8AppendCodePoint(char* buf, uint32_t ucs4) {
    size_t bytes = 0;

    if (ucs4 < 0x80) {
        buf[0] = static_cast<char>(ucs4);
        bytes = 1;
    } else if (ucs4 < 0x800) {
        buf[0] = static_cast<char>(0xC0 | (ucs4 >> 6));    // 110xxxxx
        buf[1] = static_cast<char>(0x80 | (ucs4 & 0x3F));  // 10xxxxxx
        bytes = 2;
    } else if (ucs4 < 0x10000) {
        buf[0] = static_cast<char>(0xE0 | (ucs4 >> 12));          // 1110xxxx
        buf[1] = static_cast<char>(0x80 | ((ucs4 >> 6) & 0x3F));  // 10xxxxxx
        buf[2] = static_cast<char>(0x80 | (ucs4 & 0x3F));         // 10xxxxxx
        bytes = 3;
    } else if (ucs4 < 0x110000) {
        buf[0] = static_cast<char>(0xF0 | (ucs4 >> 18)),           // 11110xxx
        buf[1] = static_cast<char>(0x80 | ((ucs4 >> 12) & 0x3F)),  // 10xxxxxx
        buf[2] = static_cast<char>(0x80 | ((ucs4 >> 6) & 0x3F)),   // 10xxxxxx
        buf[3] = static_cast<char>(0x80 | (ucs4 & 0x3F)),          // 10xxxxxx
        bytes = 4;
    }

    return bytes;
}

inline bool IsUTF16Surrogate(uint16_t u16) {
    return (u16 & 0xF800) == 0xD800;
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
    } else {
        *bytes_processed = 1;
    }

    return ucs4;
}

inline uint32_t DecodeUTF16BEToCodePoint(const uint16_t* str, size_t u16_available, size_t* u16_processed) {
    if (!u16_available) {
        *u16_processed = 0;
        return 0;
    }

    uint32_t ucs4 = 0xFFFD;

    uint16_t ch = ((str[0] & 0xff) << 8) | ((str[0] & 0xff00) >> 8);

    if (ch < 0xD800 || ch > 0xDFFF) {
        ucs4 = ch;
        *u16_processed = 1;
    } else if (ch >= 0xD800 && ch <= 0xDBFF) {
        // leading surrogate (high surrogate)
        if (u16_available < 2) {
            // Lack of data
            *u16_processed = 1;
        } else {
            uint16_t ch2 = ((str[1] & 0xff) << 8) | ((str[1] & 0xff00) >> 8);
            if (ch2 >= 0xDC00 && ch2 <= 0xDFFF) {
                // trailing surrogate (low surrogate)
                ucs4 = 0x10000 + ((ch - 0xD800) << 10) + (ch2 - 0xDC00);
                *u16_processed = 2;
            } else {
                // Invalid surrogate pair
                *u16_processed = 1;
            }
        }
    } else if (ch >= 0xDC00 && ch <= 0xDFFF) {
        // Invalid surrogate pair
        *u16_processed = 1;
    }

    return ucs4;
}

inline std::string ConvertUTF16BEToUTF8(const uint16_t* str, size_t u16_count) {
    std::string u8str;

    size_t u16_processed = 0;

    while (u16_processed < u16_count) {
        size_t processed = 0;
        uint32_t codepoint = DecodeUTF16BEToCodePoint(&str[u16_processed],
                                                      u16_count - u16_processed,
                                                      &processed);
        UTF8AppendCodePoint(u8str, codepoint);
        u16_processed += processed;
    }

    return u8str;
}

}  // namespace aribcaption::utf

#endif  // ARIBCAPTION_UTF_HELPER_HPP
