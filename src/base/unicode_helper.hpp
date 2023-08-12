/*
 * Copyright (C) 2023 magicxqq <xqq@xqq.im>. All rights reserved.
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

#ifndef ARIBCAPTION_UNICODE_HELPER_HPP
#define ARIBCAPTION_UNICODE_HELPER_HPP

#include <cstdint>

namespace unicode {

inline bool IsSpaceCharacter(uint32_t codepoint) {
    if (codepoint == 0x0009 || codepoint == 0x0020 ||
        codepoint == 0x00A0 || codepoint == 0x1680 ||
        codepoint == 0x3000 || codepoint == 0x202F ||
        codepoint == 0x205F || (codepoint >= 0x2000 && codepoint <= 0x200A)) {
        return true;
    }
    return false;
}

inline bool IsHalfwidthCharacter(uint32_t codepoint) {
    if ((codepoint != 0 && (codepoint & 0xFFFFFF00) == 0) ||
        (codepoint >= 0xFF61 && codepoint <= 0xFF9F) ||
        (codepoint >= 0xFFE8 && codepoint <= 0xFFEE)) {
        return true;
    }
    return false;
}

}  // namespace unicode

#endif  // ARIBCAPTION_UNICODE_HELPER_HPP
