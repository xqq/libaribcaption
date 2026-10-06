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

#ifndef ARIBCAPTION_MD5_HELPER_HPP
#define ARIBCAPTION_MD5_HELPER_HPP

#include <cstdint>
#include <cstdio>
#include <vector>
#include <string>
#include "base/md5.h"

namespace aribcaption::md5 {

inline std::string GetDigest(const uint8_t* buffer, size_t length) {
    const uint8_t* ptr = buffer;
    aribcc_md5_ctx_t ctx;
    aribcc_md5_init(&ctx);

    while (length) {
        if (length > 64) {
            aribcc_md5_update(&ctx, ptr, 64);
            length -= 64;
            ptr += 64;
        } else {
            aribcc_md5_update(&ctx, ptr, static_cast<unsigned long>(length));
            length -= length;
            ptr += length;
        }
    }

    std::vector<uint8_t> digest(16, 0);
    aribcc_md5_final(&digest[0], &ctx);

    std::string digest_str(32, '\0');
    for (size_t i = 0; i < 16; i++) {
        snprintf(&digest_str[i * 2], 16 * 2, "%02x", static_cast<uint32_t>(digest[i]));
    }

    return digest_str;
}

}  // namespace aribcaption::md5

#endif  // ARIBCAPTION_MD5_HELPER_HPP
