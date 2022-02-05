/*
 * Copyright (C) 2022 magicxqq <xqq@xqq.im>. All rights reserved.
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

#include "decoder/b24_codesets.hpp"

namespace aribcaption {

extern const std::unordered_map<uint8_t, CodesetEntry> kGCodesetByF = {
    {0x42, kKanjiEntry},
    {0x4a, kAlphanumericEntry},
    {0x4b, kLatinExtensionEntry},
    {0x4c, kLatinSpecialEntry},
    {0x30, kHiraganaEntry},
    {0x31, kKatakanaEntry},
    {0x32, kMosaicAEntry},
    {0x33, kMosaicBEntry},
    {0x34, kMosaicCEntry},
    {0x35, kMosaicDEntry},
    {0x36, kProportionalAlphanumericEntry},
    {0x37, kProportionalHiraganaEntry},
    {0x38, kProportionalKatakanaEntry},
    {0x49, kJIS_X0201_Katakana_Entry},
    {0x39, kJIS_X0213_2004_Kanji_1_Entry},
    {0x3a, kJIS_X0213_2004_Kanji_2_Entry},
    {0x3b, kAdditionalSymbolsEntry}
};

extern const std::unordered_map<uint8_t, CodesetEntry> kDRCSCodesetByF = {
    {0x40, kDRCS0Entry},
    {0x41, kDRCS1Entry},
    {0x42, kDRCS2Entry},
    {0x43, kDRCS3Entry},
    {0x44, kDRCS4Entry},
    {0x45, kDRCS5Entry},
    {0x46, kDRCS6Entry},
    {0x47, kDRCS7Entry},
    {0x48, kDRCS8Entry},
    {0x49, kDRCS9Entry},
    {0x4a, kDRCS10Entry},
    {0x4b, kDRCS11Entry},
    {0x4c, kDRCS12Entry},
    {0x4d, kDRCS13Entry},
    {0x4e, kDRCS14Entry},
    {0x4f, kDRCS15Entry},
    {0x70, kMacroEntry},
};

}  // namespace aribcaption
