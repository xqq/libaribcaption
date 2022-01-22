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

#ifndef ARIBCAPTION_CAPTION_HPP
#define ARIBCAPTION_CAPTION_HPP

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <unordered_map>
#include "color.hpp"

namespace aribcaption {

constexpr int64_t PTS_NOPTS = ((int64_t)UINT64_C(0x8000000000000000));            // -1
constexpr int64_t DURATION_INDEFINITE = ((int64_t)UINT64_C(0x7FFFFFFFFFFFFFFF));  // int64_t max value

// For encoding ISO 639-2 3-char language codes
template <size_t N>
inline constexpr uint32_t ThreeCC(const char (&str)[N]) {
    static_assert(N == 4, "ISO 639-2 literals must be exactly 3 characters long");
    return  (static_cast<uint32_t>(str[0]) << 16) |
            (static_cast<uint32_t>(str[1]) <<  8) |
            (static_cast<uint32_t>(str[2]) <<  0);
}

enum CharStyle {
    kCharStyleDefault = 0,
    kCharStyleBold = 1u << 0,
    kCharStyleItalic = 1u << 1,
    kCharStyleUnderline = 1u << 2,
    kCharStyleStroke = 1u << 3
};

enum EnclosureStyle {
    kEnclosureStyleNone = 0,
    kEnclosureStyleTop = 1u << 0,
    kEnclosureStyleRight = 1u << 1,
    kEnclosureStyleBottom = 1u << 2,
    kEnclosureStyleLeft = 1u << 3,
    kEnclosureStyleDefault = kEnclosureStyleNone
};

enum class CaptionType : uint8_t {
    kCaption = 0x80,
    kSuperimpose = 0x81,
    kDefault = kCaption
};

enum class CaptionCharType {
    kText = 0,
    kDRCS,
    kDRCSReplaced,
    kDefault = kText
};

struct CaptionChar {
    CaptionCharType type = CaptionCharType::kDefault;

    // Character's Unicode codepoint (UCS4)
    uint32_t codepoint = 0;
    // Character's PUA(Private Use Area) codepoint, if exists
    uint32_t pua_codepoint = 0;
    // DRCS character's code
    uint32_t drcs_code = 0;

    int x = 0;
    int y = 0;
    int char_width = 0;
    int char_height = 0;
    int char_horizontal_spacing = 0;
    int char_vertical_spacing = 0;
    float char_horizontal_scale = 0.0f;
    float char_vertical_scale = 0.0f;

    ColorRGBA text_color;
    ColorRGBA back_color;
    ColorRGBA stroke_color;

    CharStyle style = CharStyle::kCharStyleDefault;
    EnclosureStyle enclosure_style = EnclosureStyle::kEnclosureStyleDefault;

    // Character encoded in UTF-8
    char u8str[8] = {0};
public:
    CaptionChar() = default;
    CaptionChar(const CaptionChar&) = default;
    CaptionChar(CaptionChar&&) = default;
    CaptionChar& operator=(const CaptionChar&) = default;
    CaptionChar& operator=(CaptionChar&&) = default;

    [[nodiscard]]
    int section_width() const {
        return (int)std::floor((float)(char_width + char_horizontal_spacing) * char_horizontal_scale);
    }

    [[nodiscard]]
    int section_height() const {
        return (int)std::floor((float)(char_height + char_vertical_spacing) * char_vertical_scale);
    }
};

struct DRCS {
    int width = 0;
    int height = 0;
    int depth = 0;
    int depth_bits = 0;
    std::vector<uint8_t> pixels;
    std::string md5;
    std::string alternative_text;
    uint32_t alternative_ucs4 = 0;
public:
    DRCS() = default;
    DRCS(const DRCS&) = default;
    DRCS(DRCS&&) = default;
    DRCS& operator=(const DRCS&) = default;
    DRCS& operator=(DRCS&&) = default;
};

struct CaptionRegion {
    std::vector<CaptionChar> chars;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool is_ruby = false;
public:
    CaptionRegion() = default;
    CaptionRegion(const CaptionRegion&) = default;
    CaptionRegion(CaptionRegion&&) = default;
    CaptionRegion& operator=(const CaptionRegion&) = default;
    CaptionRegion& operator=(CaptionRegion&&) = default;
};

enum CaptionFlags : uint8_t {
    kCaptionFlagsDefault = 0,
    kCaptionFlagsClearScreen = 1u << 0,
    kCaptionFlagsWaitDuration =  1u << 1
};

struct Caption {
    CaptionType type = CaptionType::kDefault;
    CaptionFlags flags = CaptionFlags::kCaptionFlagsDefault;

    // ISO 639-2 3-char language code, in Big Endian
    // e.g. "jpn" => 6A 70 6E => 0x006A706E
    uint32_t iso6392_language_code = 0;

    std::string text;
    std::vector<CaptionRegion> regions;
    std::unordered_map<uint32_t, DRCS> drcs_map;

    int64_t pts = 0;
    int64_t wait_duration = 0;
    int plane_width = 0;
    int plane_height = 0;

    bool has_builtin_sound = false;
    uint8_t builtin_sound_id = 0;
public:
    Caption() = default;
    Caption(const Caption&) = default;
    Caption(Caption&&) = default;
    Caption& operator=(const Caption&) = default;
    Caption& operator=(Caption&&) = default;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_CAPTION_HPP
