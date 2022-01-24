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

const int64_t ARIBCC_PTS_NOPTS = (int64_t)UINT64_C(0x8000000000000000);
const int64_t ARIBCC_DURATION_INDEFINITE = (int64_t)UINT64_C(0x7FFFFFFFFFFFFFFF);

typedef enum aribcc_charstyle_t {
    ARIBCC_CHARSTYLE_DEFAULT = 0,
    ARIBCC_CHARSTYLE_BOLD = 1u << 0,
    ARIBCC_CHARSTYLE_ITALIC = 1u << 1,
    ARIBCC_CHARSTYLE_UNDERLINE = 1u << 2,
    ARIBCC_CHARSTYLE_STROKE = 1u << 3
} aribcc_charstyle_t;

typedef enum aribcc_enclosurestyle_t {
    ARIBCC_ENCLOSURESTYLE_NONE = 0,
    ARIBCC_ENCLOSURESTYLE_TOP = 1u << 0,
    ARIBCC_ENCLOSURESTYLE_RIGHT = 1u << 1,
    ARIBCC_ENCLOSURESTYLE_BOTTOM = 1u << 2,
    ARIBCC_ENCLOSURESTYLE_LEFT = 1u << 3,
    ARIBCC_ENCLOSURESTYLE_DEFAULT = ARIBCC_ENCLOSURESTYLE_NONE
} aribcc_enclosurestyle_t;

typedef enum aribcc_captiontype_t {
    ARIBCC_CAPTIONTYPE_CAPTION = 0x80,
    ARIBCC_CAPTIONTYPE_SUPERIMPOSE = 0x81,
    ARIBCC_CAPTIONTYPE_DEFAULT = ARIBCC_CAPTIONTYPE_CAPTION
} aribcc_captiontype_t;

typedef enum aribcc_chartype_t {
    ARIBCC_CHARTYPE_TEXT = 0,
    ARIBCC_CHARTYPE_DRCS = 1,
    ARIBCC_CHARTYPE_DRCS_REPLACED = 2,
    ARIBCC_CHARTYPE_DEFAULT = ARIBCC_CHARTYPE_TEXT
} aribcc_chartype_t;

typedef struct aribcc_caption_char_t {
    aribcc_chartype_t type;

    // Character's Unicode codepoint (UCS4)
    uint32_t codepoint;
    // Character's PUA(Private Use Area) codepoint, if exists
    uint32_t pua_codepoint;
    // DRCS character's code
    uint32_t drcs_code;

    int x;
    int y;
    int char_width;
    int char_height;
    int char_horizontal_spacing;
    int char_vertical_spacing;
    float char_horizontal_scale;
    float char_vertical_scale;

    aribcc_color_t text_color;
    aribcc_color_t back_color;
    aribcc_color_t stroke_color;

    aribcc_charstyle_t style;
    aribcc_enclosurestyle_t enclosure_style;

    // Character encoded in UTF-8
    char u8str[8];
} aribcc_caption_char_t;


ARIBCC_API int aribcc_caption_char_get_section_width(aribcc_caption_char_t* caption_char);

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


typedef struct aribcc_caption_region_t {
    int x;
    int y;
    int width;
    int height;
    bool is_ruby;

    aribcc_caption_char_t* chars;
    uint32_t char_count;
} aribcc_caption_region_t;


ARIBCC_API void aribcc_caption_region_cleanup(aribcc_caption_region_t* region);


typedef enum aribcc_captionflags_t {
    ARIBCC_CAPTIONFLAGS_DEFAULT = 0,
    ARIBCC_CAPTIONFLAGS_CLEARSCREEN = 1u << 0,
    ARIBCC_CAPTIONFLAGS_WAITDURATION =  1u << 1
} aribcc_captionflags_t;

typedef struct aribcc_caption_t {
    aribcc_captiontype_t type;
    aribcc_captionflags_t flags;

    // ISO 639-2 3-char language code, in Big Endian
    // e.g. "jpn" => 6A 70 6E => 0x006A706E
    uint32_t iso6392_language_code;

    char* text;

    aribcc_caption_region_t* regions;
    uint32_t region_count;

    aribcc_drcsmap_t* drcs_map;

    int64_t pts;
    int64_t wait_duration;
    int plane_width;
    int plane_height;

    bool has_builtin_sound;
    uint8_t builtin_sound_id;
} aribcc_caption_t;


ARIBCC_API void aribcc_caption_cleanup(aribcc_caption_t* caption);



#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_CAPTION_H
