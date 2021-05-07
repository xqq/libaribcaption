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

static constexpr CodesetEntry kKanjiEntry(GraphicSet::kKanji, 2);
static constexpr CodesetEntry kAlphanumericEntry(GraphicSet::kAlphanumeric, 1);
static constexpr CodesetEntry kHiraganaEntry(GraphicSet::kHiragana, 1);
static constexpr CodesetEntry kKatakanaEntry(GraphicSet::kKatakana, 1);
static constexpr CodesetEntry kMosaicAEntry(GraphicSet::kMosaicA, 1);
static constexpr CodesetEntry kMosaicBEntry(GraphicSet::kMosaicB, 1);
static constexpr CodesetEntry kMosaicCEntry(GraphicSet::kMosaicC, 1);
static constexpr CodesetEntry kMosaicDEntry(GraphicSet::kMosaicD, 1);
static constexpr CodesetEntry kProportionalAlphanumericEntry(GraphicSet::kProportionalAlphanumeric, 1);
static constexpr CodesetEntry kProportionalHiraganaEntry(GraphicSet::kProportionalHiragana, 1);
static constexpr CodesetEntry kProportionalKatakanaEntry(GraphicSet::kProportionalKatakana, 1);
static constexpr CodesetEntry kJIS_X0201_Katakana_Entry(GraphicSet::kJIS_X0201_Katakana, 1);
static constexpr CodesetEntry kJIS_X0213_2004_Kanji_1_Entry(GraphicSet::kJIS_X0213_2004_Kanji_1, 2);
static constexpr CodesetEntry kJIS_X0213_2004_Kanji_2_Entry(GraphicSet::kJIS_X0213_2004_Kanji_2, 2);
static constexpr CodesetEntry kAdditionalSymbolsEntry(GraphicSet::kAdditionalSymbols, 2);

static const std::unordered_map<uint8_t, CodesetEntry> kGCodesetByF = {
    {0x42, kKanjiEntry},
    {0x4a, kAlphanumericEntry},
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

static constexpr CodesetEntry kDRCS0Entry(GraphicSet::kDRCS_0, 2);
static constexpr CodesetEntry kDRCS1Entry(GraphicSet::kDRCS_1, 1);
static constexpr CodesetEntry kDRCS2Entry(GraphicSet::kDRCS_2, 1);
static constexpr CodesetEntry kDRCS3Entry(GraphicSet::kDRCS_3, 1);
static constexpr CodesetEntry kDRCS4Entry(GraphicSet::kDRCS_4, 1);
static constexpr CodesetEntry kDRCS5Entry(GraphicSet::kDRCS_5, 1);
static constexpr CodesetEntry kDRCS6Entry(GraphicSet::kDRCS_6, 1);
static constexpr CodesetEntry kDRCS7Entry(GraphicSet::kDRCS_7, 1);
static constexpr CodesetEntry kDRCS8Entry(GraphicSet::kDRCS_8, 1);
static constexpr CodesetEntry kDRCS9Entry(GraphicSet::kDRCS_9, 1);
static constexpr CodesetEntry kDRCS10Entry(GraphicSet::kDRCS_10, 1);
static constexpr CodesetEntry kDRCS11Entry(GraphicSet::kDRCS_11, 1);
static constexpr CodesetEntry kDRCS12Entry(GraphicSet::kDRCS_12, 1);
static constexpr CodesetEntry kDRCS13Entry(GraphicSet::kDRCS_13, 1);
static constexpr CodesetEntry kDRCS14Entry(GraphicSet::kDRCS_14, 1);
static constexpr CodesetEntry kDRCS15Entry(GraphicSet::kDRCS_15, 1);
static constexpr CodesetEntry kMacroEntry(GraphicSet::kMacro, 1);

static const std::unordered_map<uint8_t, CodesetEntry> kDRCSCodesetByF = {
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

#endif  // ARIBCAPTION_B24_CODESETS_HPP
