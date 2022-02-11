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

#ifndef ARIBCAPTION_CAPTION_H
#define ARIBCAPTION_CAPTION_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "aribcc_export.h"
#include "color.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Helper macro for encoding ISO 639-2 3-char language code
 *
 * @param a,b,c char
 * @code
 * uint32_t language_code = ARIBCC_MAKE_LANG('j', 'p', 'n')
 * @endcode
 */
#define ARIBCC_MAKE_LANG(a,b,c) ((((a) & 0xff) << 16) | (((b) & 0xff) << 8) | ((c) & 0xff))

/**
 * Constant for marking the PTS is undefined.
 */
#define ARIBCC_PTS_NOPTS ((int64_t)UINT64_C(0x8000000000000000))

/**
 * Constant for marking the duration is indefinite.
 *
 * Some ARIB captions have indefinite duration which means the caption's end time is undetermined.
 * Captions with indefinite duration should be presented until the next caption's PTS.
 */
#define ARIBCC_DURATION_INDEFINITE ((int64_t)UINT64_C(0x7FFFFFFFFFFFFFFF))

/**
 * Per-CaptionChar character styles
 */
typedef enum aribcc_charstyle_t {
    ARIBCC_CHARSTYLE_DEFAULT = 0,
    ARIBCC_CHARSTYLE_BOLD = 1u << 0,
    ARIBCC_CHARSTYLE_ITALIC = 1u << 1,
    ARIBCC_CHARSTYLE_UNDERLINE = 1u << 2,
    ARIBCC_CHARSTYLE_STROKE = 1u << 3
} aribcc_charstyle_t;

/**
 * Per-CaptionChar enclosure styles
 */
typedef enum aribcc_enclosurestyle_t {
    ARIBCC_ENCLOSURESTYLE_NONE = 0,
    ARIBCC_ENCLOSURESTYLE_BOTTOM = 1u << 0,
    ARIBCC_ENCLOSURESTYLE_RIGHT = 1u << 1,
    ARIBCC_ENCLOSURESTYLE_TOP = 1u << 2,
    ARIBCC_ENCLOSURESTYLE_LEFT = 1u << 3,
    ARIBCC_ENCLOSURESTYLE_DEFAULT = ARIBCC_ENCLOSURESTYLE_NONE
} aribcc_enclosurestyle_t;

/**
 * Enums for caption type indication. Usually, kCaption should be used.
 *
 * kSuperimpose should be indicated if you are handling ARIB superimpose (a kind of emergency caption).
 */
typedef enum aribcc_captiontype_t {
    ARIBCC_CAPTIONTYPE_CAPTION = 0x80,
    ARIBCC_CAPTIONTYPE_SUPERIMPOSE = 0x81,
    ARIBCC_CAPTIONTYPE_DEFAULT = ARIBCC_CAPTIONTYPE_CAPTION
} aribcc_captiontype_t;

/**
 * Represents the type of CaptionChar.
 *
 * The type will be kDRCS or kDRCSReplaced if it's a DRCS character.
 */
typedef enum aribcc_chartype_t {
    ARIBCC_CHARTYPE_TEXT = 0,
    ARIBCC_CHARTYPE_DRCS = 1,
    ARIBCC_CHARTYPE_DRCS_REPLACED = 2,
    ARIBCC_CHARTYPE_DEFAULT = ARIBCC_CHARTYPE_TEXT
} aribcc_chartype_t;

/**
 * Represents a caption character.
 */
typedef struct aribcc_caption_char_t {
    aribcc_chartype_t type;

    /**
     * Character's Unicode codepoint (UCS4). This field will be 0 if CaptionCharType is kDRCS.
     */
    uint32_t codepoint;

    /**
     * Character's PUA(Private Use Area) codepoint (UCS4)
     *
     * This field will be non-zero only if PUA code is defined for the character.
     *
     * Some ARIB additional symbols (i.e. Gaiji) are mapped into Private Use Area in Unicode BMP.
     * Though most of them could be mapped into characters introduced in Unicode 5.2,
     * Some fonts don't support Unicode 5.2 but support ARIB additional symbols through Private Use Area codes.
     */
    uint32_t pua_codepoint;

    /**
     * DRCS character's code. Only appears if if CaptionCharType is kDRCS or kDRCSReplaced.
     * It is a private code which is unrelated to Unicode.
     *
     * DRCS data could be retrieved from Caption::drcs_map using drcs_code.
     */
    uint32_t drcs_code;

    int x;
    int y;
    int char_width;
    int char_height;
    int char_horizontal_spacing;
    int char_vertical_spacing;
    float char_horizontal_scale;
    float char_vertical_scale;

    aribcc_color_t text_color;    ///< Color of the text (foreground)
    aribcc_color_t back_color;    ///< Color of the background
    aribcc_color_t stroke_color;  ///< Color of the storke text

    aribcc_charstyle_t style;
    aribcc_enclosurestyle_t enclosure_style;

    /**
     * String representation of character encoded in UTF-8. This string is Null-terminated.
     * Will be empty string if CaptionCharType is kDRCS because alternative codepoint is unknown.
     */
    char u8str[8];
} aribcc_caption_char_t;

/**
 * Calculate the width of the character block
 */
ARIBCC_API int aribcc_caption_char_get_section_width(aribcc_caption_char_t* caption_char);

/**
 * Calculate the height of the character block
 */
ARIBCC_API int aribcc_caption_char_get_section_height(aribcc_caption_char_t* caption_char);


// Opaque type
typedef struct aribcc_drcs_t aribcc_drcs_t;

ARIBCC_API aribcc_drcs_t* aribcc_drcs_alloc(void);

ARIBCC_API void aribcc_drcs_free(aribcc_drcs_t* drcs);

ARIBCC_API aribcc_drcs_t* aribcc_drcs_clone(aribcc_drcs_t* drcs);

ARIBCC_API void aribcc_drcs_set_size(aribcc_drcs_t* drcs, int width, int height);

ARIBCC_API void aribcc_drcs_get_size(aribcc_drcs_t* drcs, int* width, int* height);

ARIBCC_API void aribcc_drcs_set_depth(aribcc_drcs_t* drcs, int depth, int depth_bits);

ARIBCC_API void aribcc_drcs_get_depth(aribcc_drcs_t* drcs, int* depth, int* depth_bits);

ARIBCC_API void aribcc_drcs_import_pixels(aribcc_drcs_t* drcs, const uint8_t* pixels, size_t size);

ARIBCC_API void aribcc_drcs_get_pixels(aribcc_drcs_t* drcs, uint8_t** ppixels, size_t* psize);

ARIBCC_API void aribcc_drcs_set_md5(aribcc_drcs_t* drcs, const char* md5);

ARIBCC_API const char* aribcc_drcs_get_md5(aribcc_drcs_t* drcs);

ARIBCC_API void aribcc_drcs_set_alternative_ucs4(aribcc_drcs_t* drcs, uint32_t ucs4);

ARIBCC_API uint32_t aribcc_drcs_get_alternative_ucs4(aribcc_drcs_t* drcs);

ARIBCC_API const char* aribcc_drcs_get_alternative_text(aribcc_drcs_t* drcs);


// Opaque type
typedef struct aribcc_drcsmap_t aribcc_drcsmap_t;

ARIBCC_API aribcc_drcsmap_t* aribcc_drcsmap_alloc(void);

ARIBCC_API void aribcc_drcsmap_free(aribcc_drcsmap_t* drcs_map);

ARIBCC_API void aribcc_drcsmap_erase(aribcc_drcsmap_t* drcs_map, uint32_t key);

ARIBCC_API void aribcc_drcsmap_put(aribcc_drcsmap_t* drcs_map, uint32_t key, const aribcc_drcs_t* drcs);

ARIBCC_API aribcc_drcs_t* aribcc_drcsmap_get(aribcc_drcsmap_t* drcs_map, uint32_t key);

ARIBCC_API void aribcc_drcsmap_clear(aribcc_drcsmap_t* drcs_map);


/**
 * Structure represents a caption region.
 *
 * Call @aribcc_caption_region_cleanup() if you need to release a region which is received from aribcc API.
 */
typedef struct aribcc_caption_region_t {
    int x;
    int y;
    int width;
    int height;
    bool is_ruby;           ///< Will be true if the region is likely to be ruby text (furigana)

    /**
     * Caption char array. Element count is indicated by char_count.
     *
     * Do not manually free this array if the region is received from aribcc API,
     * instead, call @aribcc_caption_region_cleanup().
     */
    aribcc_caption_char_t* chars;
    uint32_t char_count;
} aribcc_caption_region_t;

/**
 * Caption region cleanup function.
 *
 * Call this function only if if you received the region from aribcc API.
 * Otherwise it may cause a crash.
 *
 * This function doesn't release the memory of the region itself.
 */
ARIBCC_API void aribcc_caption_region_cleanup(aribcc_caption_region_t* region);

/**
 * Constants for flags contained in Caption.
 */
typedef enum aribcc_captionflags_t {
    ARIBCC_CAPTIONFLAGS_DEFAULT = 0,
    ARIBCC_CAPTIONFLAGS_CLEARSCREEN = 1u << 0,     ///< Screen should be cleared before the caption presentation
    ARIBCC_CAPTIONFLAGS_WAITDURATION =  1u << 1    ///< The caption has a determined duration
} aribcc_captionflags_t;

/**
 * Structure represents a caption.
 *
 * Call @aribcc_caption_cleanup() if you need to release a caption which is received from the decoder.
 */
typedef struct aribcc_caption_t {
    aribcc_captiontype_t type;
    aribcc_captionflags_t flags;

    /**
     * ISO 639-2 3-char language code
     * e.g. "jpn" => 6A 70 6E => 0x006A706E
     */
    uint32_t iso6392_language_code;

    /**
     * Caption statements represented in UTF-8 string.
     * Ruby text is excluded in this string.
     *
     * Pay attention to the UTF-8 encoding if you are under Windows.
     *
     * Do not manually free this pointer if the caption is received from the decoder,
     * instead, call @aribcc_caption_cleanup().
     */
    char* text;

    /**
     * Caption region array. Element count is indicated by region_count.
     *
     * Do not manually free this array if the caption is received from the decoder,
     * instead, call @aribcc_caption_cleanup().
     */
    aribcc_caption_region_t* regions;
    uint32_t region_count;             ///< element count of regions array

    /**
     * DRCS hashmap, may be NULL if DRCS not exists
     *
     * Do not manually free this pointer if the caption is received from the decoder,
     * instead, call @aribcc_caption_cleanup().
     */
    aribcc_drcsmap_t* drcs_map;

    /**
     * Caption't presentation timestamp, in milliseconds
     *
     * Will be @ARIBCC_PTS_NOPTS if passed as @ARIBCC_PTS_NOPTS into decoder, otherwise in milliseconds.
     */
    int64_t pts;

    /**
     * Caption's duration, in milliseconds
     *
     * Will be @ARIBCC_DURATION_INDEFINITE if undetermined, otherwise in milliseconds.
     */
    int64_t wait_duration;

    /**
     * Width of the original(logical) caption plane. Usually has a value of 960 or 720.
     */
    int plane_width;

    /**
     * Height of the original(logical) caption plane. Usually has a value of 540 or 480.
     */
    int plane_height;

    /**
     * Represents whether the caption indicates a Built-in Sound Replay.
     */
    bool has_builtin_sound;

    /**
     * The ID of build-in sound for playback. Valid only if has_builtin_sound is true.
     */
    uint8_t builtin_sound_id;
} aribcc_caption_t;


/**
 * Release all the dynamic-allocated inner fields of a caption.
 *
 * If you received a caption from the decoder, you should always call this function for cleanup
 * rather than manually free the pointers inside the struct, otherwise it may cause a crash.
 *
 * On the other hand, if you constructed an aribcc_caption_t structure on your own,
 * you should always manually free the pointers inside which is allocated by yourself.
 * Call this function on your self-allocated aribcc_caption_t structure may cause a crash.
 *
 * This function doesn't release the memory of the caption itself.
 */
ARIBCC_API void aribcc_caption_cleanup(aribcc_caption_t* caption);



#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_CAPTION_H
