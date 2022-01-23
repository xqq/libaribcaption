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

#ifndef ARIBCAPTION_DECODER_H
#define ARIBCAPTION_DECODER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "aribcc_export.h"
#include "caption.h"
#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum aribcc_profile_t {
    ARIBCC_PROFILE_A = 0x0008,
    ARIBCC_PROFILE_C = 0x0012,
    ARIBCC_PROFILE_DEFAULT = ARIBCC_PROFILE_A
} aribcc_profile_t;

typedef enum aribcc_languageid_t {
    ARIBCC_LANGUAGEID_FIRST = 1,
    ARIBCC_LANGUAGEID_SECOND = 2,
    ARIBCC_LANGUAGEID_DEFAULT = ARIBCC_LANGUAGEID_FIRST,
    ARIBCC_LANGUAGEID_MAX = ARIBCC_LANGUAGEID_SECOND
} aribcc_languageid_t;

typedef enum aribcc_encoding_scheme_t {
    ARIBCC_ENCODING_SCHEME_AUTO = 0,
    ARIBCC_ENCODING_SCHEME_ARIB_STD_B24_JIS = 1,
    ARIBCC_ENCODING_SCHEME_ARIB_STD_B24_UTF8 = 2,
    ARIBCC_ENCODING_SCHEME_ABNT_NBR_15606_1_LATIN = 3,
    ARIBCC_ENCODING_SCHEME_ISDB_T_PHILIPPINES_UTF8 = ARIBCC_ENCODING_SCHEME_ARIB_STD_B24_UTF8  // alias
} aribcc_encoding_scheme_t;

typedef enum aribcc_decode_status_t {
    ARIBCC_DECODE_STATUS_ERROR = 0,
    ARIBCC_DECODE_STATUS_NO_CAPTION = 1,
    ARIBCC_DECODE_STATUS_GOT_CAPTION = 2
} aribcc_decode_status_t;

typedef struct aribcc_decoder_t aribcc_decoder_t;


ARIBCC_API aribcc_decoder_t* aribcc_decoder_alloc(aribcc_context_t* context);

ARIBCC_API void aribcc_decoder_free(aribcc_decoder_t* decoder);

ARIBCC_API bool aribcc_decoder_initialize(aribcc_decoder_t* decoder,
                                          aribcc_encoding_scheme_t encoding_scheme,
                                          aribcc_captiontype_t type,
                                          aribcc_profile_t profile,
                                          aribcc_languageid_t language_id);

ARIBCC_API void aribcc_decoder_set_encoding_scheme(aribcc_decoder_t* decoder,
                                                   aribcc_encoding_scheme_t encoding_scheme);

ARIBCC_API void aribcc_decoder_set_caption_type(aribcc_decoder_t* decoder, aribcc_captiontype_t type);

ARIBCC_API void aribcc_decoder_set_profile(aribcc_decoder_t* decoder, aribcc_profile_t profile);

ARIBCC_API void aribcc_decoder_switch_language(aribcc_decoder_t* decoder, aribcc_languageid_t language_id);

ARIBCC_API void aribcc_decoder_set_replace_msz_fullwidth_ascii(aribcc_decoder_t* decoder, bool replace);

ARIBCC_API uint32_t aribcc_decoder_query_iso6392_language_code(aribcc_decoder_t* decoder,
                                                               aribcc_languageid_t language_id);

ARIBCC_API aribcc_decode_status_t aribcc_decoder_decode(aribcc_decoder_t* decoder,
                                                        const uint8_t* pes_data,
                                                        size_t length,
                                                        int64_t pts,
                                                        aribcc_caption_t* out_caption);

ARIBCC_API void aribcc_decoder_flush(aribcc_decoder_t* decoder);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_DECODER_H
