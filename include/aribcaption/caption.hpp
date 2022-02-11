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

/**
 * Constant for marking the PTS is undefined.
 */
constexpr int64_t PTS_NOPTS = ((int64_t)UINT64_C(0x8000000000000000));            // -1

/**
 * Constant for marking the duration is indefinite.
 *
 * Some ARIB captions have indefinite duration which means the caption's end time is undetermined.
 * Captions with indefinite duration should be presented until the next caption's PTS.
 */
constexpr int64_t DURATION_INDEFINITE = ((int64_t)UINT64_C(0x7FFFFFFFFFFFFFFF));  // int64_t max value

/**
 * Helper function for encoding ISO 639-2 3-char language code
 *
 * @param str 3-character string literal, e.g. "jpn"
 * @return e.g. "jpn" => 6A 70 6E => 0x006A706E
 */
template <size_t N>
inline constexpr uint32_t ThreeCC(const char (&str)[N]) {
    static_assert(N == 4, "ISO 639-2 literals must be exactly 3 characters long");
    return  (static_cast<uint32_t>(str[0]) << 16) |
            (static_cast<uint32_t>(str[1]) <<  8) |
            (static_cast<uint32_t>(str[2]) <<  0);
}

/**
 * Per-CaptionChar character styles
 */
enum CharStyle {
    kCharStyleDefault = 0,
    kCharStyleBold = 1u << 0,
    kCharStyleItalic = 1u << 1,
    kCharStyleUnderline = 1u << 2,
    kCharStyleStroke = 1u << 3
};

/**
 * Per-CaptionChar enclosure styles
 */
enum EnclosureStyle {
    kEnclosureStyleNone = 0,
    kEnclosureStyleBottom = 1u << 0,
    kEnclosureStyleRight = 1u << 1,
    kEnclosureStyleTop = 1u << 2,
    kEnclosureStyleLeft = 1u << 3,
    kEnclosureStyleDefault = kEnclosureStyleNone
};

/**
 * Enums for caption type indication. Usually, kCaption should be used.
 *
 * kSuperimpose should be indicated if you are handling ARIB superimpose (a kind of emergency caption).
 */
enum class CaptionType : uint8_t {
    kCaption = 0x80,
    kSuperimpose = 0x81,
    kDefault = kCaption
};


/**
 * Represents the type of CaptionChar.
 *
 * The type will be kDRCS or kDRCSReplaced if it's a DRCS character.
 */
enum class CaptionCharType {
    kText = 0,
    kDRCS,
    kDRCSReplaced,    ///< DRCS has been replaced into Unicode codepoint
    kDefault = kText
};

/**
 * Represents a caption character.
 */
struct CaptionChar {
    CaptionCharType type = CaptionCharType::kDefault;

    /**
     * Character's Unicode codepoint (UCS4). This field will be 0 if CaptionCharType is kDRCS.
     */
    uint32_t codepoint = 0;

    /**
     * Character's PUA(Private Use Area) codepoint (UCS4)
     *
     * This field will be non-zero only if PUA code is defined for the character.
     *
     * Some ARIB additional symbols (i.e. Gaiji) are mapped into Private Use Area in Unicode BMP.
     * Though most of them could be mapped into characters introduced in Unicode 5.2,
     * Some fonts don't support Unicode 5.2 but support ARIB additional symbols through Private Use Area codes.
     */
    uint32_t pua_codepoint = 0;

    /**
     * DRCS character's code. Only appears if if CaptionCharType is kDRCS or kDRCSReplaced.
     * It is a private code which is unrelated to Unicode.
     *
     * DRCS data could be retrieved from Caption::drcs_map using drcs_code.
     */
    uint32_t drcs_code = 0;

    int x = 0;
    int y = 0;
    int char_width = 0;
    int char_height = 0;
    int char_horizontal_spacing = 0;
    int char_vertical_spacing = 0;
    float char_horizontal_scale = 0.0f;
    float char_vertical_scale = 0.0f;

    ColorRGBA text_color;    ///< Color of the text (foreground)
    ColorRGBA back_color;    ///< Color of the background
    ColorRGBA stroke_color;  ///< Color of the storke text

    CharStyle style = CharStyle::kCharStyleDefault;
    EnclosureStyle enclosure_style = EnclosureStyle::kEnclosureStyleDefault;

    /**
     * String representation of character encoded in UTF-8. This string is Null-terminated.
     * Will be empty string if CaptionCharType is kDRCS because alternative codepoint is unknown.
     */
    char u8str[8] = {0};
public:
    CaptionChar() = default;

    /**
     * Helper function for calculating the width of the character block
     */
    [[nodiscard]]
    int section_width() const {
        return (int)std::floor((float)(char_width + char_horizontal_spacing) * char_horizontal_scale);
    }

    /**
     * Helper function for calculating the height of the character block
     */
    [[nodiscard]]
    int section_height() const {
        return (int)std::floor((float)(char_height + char_vertical_spacing) * char_vertical_scale);
    }
};

/**
 * Structure contains DRCS data and related information.
 */
struct DRCS {
    int width = 0;
    int height = 0;
    int depth = 0;
    int depth_bits = 0;
    std::vector<uint8_t> pixels;
    std::string md5;                ///< MD5 digest for the pixels buffer
    std::string alternative_text;   ///< Appears only if alternative Unicode codepoint exists
    uint32_t alternative_ucs4 = 0;  ///< Will be non-zero only if alternative Unicode codepoint exists
public:
    DRCS() = default;
    DRCS(const DRCS&) = default;
    DRCS(DRCS&&) noexcept = default;
    DRCS& operator=(const DRCS&) = default;
    DRCS& operator=(DRCS&&) noexcept = default;
};

/**
 * Structure represents a caption region.
 */
struct CaptionRegion {
    std::vector<CaptionChar> chars;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    bool is_ruby = false;           ///< Will be true if the region is likely to be ruby text (furigana)
public:
    CaptionRegion() = default;
    CaptionRegion(const CaptionRegion&) = default;
    CaptionRegion(CaptionRegion&&) noexcept = default;
    CaptionRegion& operator=(const CaptionRegion&) = default;
    CaptionRegion& operator=(CaptionRegion&&) noexcept = default;
};

/**
 * Constants for flags contained in Caption.
 */
enum CaptionFlags : uint8_t {
    kCaptionFlagsDefault = 0,
    kCaptionFlagsClearScreen = 1u << 0,     ///< Screen should be cleared before the caption presentation
    kCaptionFlagsWaitDuration =  1u << 1    ///< The caption has a determined duration
};

/**
 * Structure represents a caption.
 */
struct Caption {
    CaptionType type = CaptionType::kDefault;
    CaptionFlags flags = CaptionFlags::kCaptionFlagsDefault;

    /**
     * ISO 639-2 3-char language code
     * e.g. "jpn" => 6A 70 6E => 0x006A706E
     */
    uint32_t iso6392_language_code = 0;

    /**
     * Caption statements represented in UTF-8 string.
     * Ruby text is excluded in this string.
     *
     * Pay attention to the UTF-8 encoding if you are under Windows.
     */
    std::string text;

    /**
     * Caption region array
     */
    std::vector<CaptionRegion> regions;

    /**
     * A hashmap that contains all DRCS characters transmitted in current caption.
     *
     * Use CaptionChar::drcs_code as key for retrieving DRCS.
     */
    std::unordered_map<uint32_t, DRCS> drcs_map;

    /**
     * Caption't presentation timestamp, in milliseconds
     *
     * Will be PTS_NOPTS if passed as PTS_NOPTS into decoder, otherwise in milliseconds.
     */
    int64_t pts = 0;

    /**
     * Caption's duration, in milliseconds
     *
     * Will be DURATION_INDEFINITE if undetermined, otherwise in milliseconds.
     */
    int64_t wait_duration = 0;

    /**
     * Width of the original(logical) caption plane. Usually has a value of 960 or 720.
     */
    int plane_width = 0;

    /**
     * Height of the original(logical) caption plane. Usually has a value of 540 or 480.
     */
    int plane_height = 0;

    /**
     * Represents whether the caption indicates a Built-in Sound Replay.
     */
    bool has_builtin_sound = false;

    /**
     * The ID of build-in sound for playback. Valid only if has_builtin_sound is true.
     */
    uint8_t builtin_sound_id = 0;
public:
    Caption() = default;
    Caption(const Caption&) = default;
    Caption(Caption&&) noexcept = default;
    Caption& operator=(const Caption&) = default;
    Caption& operator=(Caption&&) noexcept = default;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_CAPTION_HPP
