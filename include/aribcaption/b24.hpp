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

#ifndef ARIBCAPTION_B24_HPP
#define ARIBCAPTION_B24_HPP

#include <cstdint>

namespace aribcaption {

enum class B24Type : uint8_t {
    kCaption = 0x80,
    kSuperimpose = 0x81,
    kDefault = kCaption
};

enum class B24Profile : uint8_t {
    kProfileA = 0x0008,
    kProfileC = 0x0012,
    kDefault = kProfileA
};

enum class B24LanguageId : uint8_t {
    kFirst = 1,
    kSecond = 2,
    kDefault = kFirst,
    kMax = kSecond
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_B24_HPP
