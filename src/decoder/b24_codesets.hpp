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

#ifndef ARIBCAPTION_B24_CODESETS_HPP
#define ARIBCAPTION_B24_CODESETS_HPP

#include <cstdint>
#include <unordered_map>

namespace aribcaption {

enum class GraphicSet : uint8_t {
    kKanji,
    kAlphanumeric,
    kLatinExtension,
    kLatinSpecial,
    kHiragana,
    kKatakana,
    kMosaicA,
    kMosaicB,
    kMosaicC,
    kMosaicD,
    kProportionalAlphanumeric,
    kProportionalHiragana,
    kProportionalKatakana,
    kJIS_X0201_Katakana,
    kJIS_X0213_2004_Kanji_1,
    kJIS_X0213_2004_Kanji_2,
    kAdditionalSymbols,

    kDRCS_0,
    kDRCS_1,
    kDRCS_2,
    kDRCS_3,
    kDRCS_4,
    kDRCS_5,
    kDRCS_6,
    kDRCS_7,
    kDRCS_8,
    kDRCS_9,
    kDRCS_10,
    kDRCS_11,
    kDRCS_12,
    kDRCS_13,
    kDRCS_14,
    kDRCS_15,
    kMacro
};

struct CodesetEntry {
    GraphicSet graphics_set;
    uint8_t bytes;

    constexpr CodesetEntry(GraphicSet set, uint8_t byte_count) noexcept : graphics_set(set), bytes(byte_count) {}
};

inline constexpr CodesetEntry kKanjiEntry(GraphicSet::kKanji, 2);
inline constexpr CodesetEntry kAlphanumericEntry(GraphicSet::kAlphanumeric, 1);
inline constexpr CodesetEntry kLatinExtensionEntry(GraphicSet::kLatinExtension, 1);
inline constexpr CodesetEntry kLatinSpecialEntry(GraphicSet::kLatinSpecial, 1);
inline constexpr CodesetEntry kHiraganaEntry(GraphicSet::kHiragana, 1);
inline constexpr CodesetEntry kKatakanaEntry(GraphicSet::kKatakana, 1);
inline constexpr CodesetEntry kMosaicAEntry(GraphicSet::kMosaicA, 1);
inline constexpr CodesetEntry kMosaicBEntry(GraphicSet::kMosaicB, 1);
inline constexpr CodesetEntry kMosaicCEntry(GraphicSet::kMosaicC, 1);
inline constexpr CodesetEntry kMosaicDEntry(GraphicSet::kMosaicD, 1);
inline constexpr CodesetEntry kProportionalAlphanumericEntry(GraphicSet::kProportionalAlphanumeric, 1);
inline constexpr CodesetEntry kProportionalHiraganaEntry(GraphicSet::kProportionalHiragana, 1);
inline constexpr CodesetEntry kProportionalKatakanaEntry(GraphicSet::kProportionalKatakana, 1);
inline constexpr CodesetEntry kJIS_X0201_Katakana_Entry(GraphicSet::kJIS_X0201_Katakana, 1);
inline constexpr CodesetEntry kJIS_X0213_2004_Kanji_1_Entry(GraphicSet::kJIS_X0213_2004_Kanji_1, 2);
inline constexpr CodesetEntry kJIS_X0213_2004_Kanji_2_Entry(GraphicSet::kJIS_X0213_2004_Kanji_2, 2);
inline constexpr CodesetEntry kAdditionalSymbolsEntry(GraphicSet::kAdditionalSymbols, 2);

inline constexpr CodesetEntry kDRCS0Entry(GraphicSet::kDRCS_0, 2);
inline constexpr CodesetEntry kDRCS1Entry(GraphicSet::kDRCS_1, 1);
inline constexpr CodesetEntry kDRCS2Entry(GraphicSet::kDRCS_2, 1);
inline constexpr CodesetEntry kDRCS3Entry(GraphicSet::kDRCS_3, 1);
inline constexpr CodesetEntry kDRCS4Entry(GraphicSet::kDRCS_4, 1);
inline constexpr CodesetEntry kDRCS5Entry(GraphicSet::kDRCS_5, 1);
inline constexpr CodesetEntry kDRCS6Entry(GraphicSet::kDRCS_6, 1);
inline constexpr CodesetEntry kDRCS7Entry(GraphicSet::kDRCS_7, 1);
inline constexpr CodesetEntry kDRCS8Entry(GraphicSet::kDRCS_8, 1);
inline constexpr CodesetEntry kDRCS9Entry(GraphicSet::kDRCS_9, 1);
inline constexpr CodesetEntry kDRCS10Entry(GraphicSet::kDRCS_10, 1);
inline constexpr CodesetEntry kDRCS11Entry(GraphicSet::kDRCS_11, 1);
inline constexpr CodesetEntry kDRCS12Entry(GraphicSet::kDRCS_12, 1);
inline constexpr CodesetEntry kDRCS13Entry(GraphicSet::kDRCS_13, 1);
inline constexpr CodesetEntry kDRCS14Entry(GraphicSet::kDRCS_14, 1);
inline constexpr CodesetEntry kDRCS15Entry(GraphicSet::kDRCS_15, 1);
inline constexpr CodesetEntry kMacroEntry(GraphicSet::kMacro, 1);

// Definitions moved into b24_codesets.cpp due to VS2017 compiler bug
extern const std::unordered_map<uint8_t, CodesetEntry> kGCodesetByF;
extern const std::unordered_map<uint8_t, CodesetEntry> kDRCSCodesetByF;

}  // namespace aribcaption

#endif  // ARIBCAPTION_B24_CODESETS_HPP
