/*
 * Copyright (C) 2023 otya <otya281@gmail.com>. All rights reserved.
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

#ifndef ARIBCAPTION_OPEN_TYPE_GSUB
#define ARIBCAPTION_OPEN_TYPE_GSUB
#include <cstdint>
#include <unordered_map>
#include <vector>
inline constexpr uint32_t FourCC(uint8_t a, uint8_t b, uint8_t c, uint8_t d) {
    return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(c) << 8) |
           (static_cast<uint32_t>(d) << 0);
}

inline constexpr uint32_t kOpenTypeFeatureHalfWidth = FourCC('h', 'w', 'i', 'd');
inline constexpr uint32_t kOpenTypeScriptHiraganaKatakana = FourCC('k', 'a', 'n', 'a');
inline constexpr uint32_t kOpenTypeLangSysJapanese = FourCC('J', 'A', 'N', ' ');
auto LoadSingleGSUBTable(const std::vector<uint8_t>& gsub,
                         uint32_t required_feature_tag,
                         uint32_t script_tag,
                         uint32_t lang_sys_tag) -> std::unordered_map<uint32_t, uint32_t>;
#endif
