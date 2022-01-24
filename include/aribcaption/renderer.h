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

#ifndef ARIBCAPTION_RENDERER_H
#define ARIBCAPTION_RENDERER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "aribcc_config.h"
#include "aribcc_export.h"
#include "context.h"
#include "caption.h"
#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum aribcc_fontprovider_type_t {
    ARIBCC_FONTPROVIDER_TYPE_AUTO = 0,
#if defined(ARIBCC_USE_CORETEXT)
    ARIBCC_FONTPROVIDER_TYPE_CORETEXT = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    ARIBCC_FONTPROVIDER_TYPE_DIRECTWRITE = 2,
#endif

#if defined(ARIBCC_USE_FONTCONFIG)
    ARIBCC_FONTPROVIDER_TYPE_FONTCONFIG = 3,
#endif

#if defined(ARIBCC_IS_ANDROID)
    ARIBCC_FONTPROVIDER_TYPE_ANDROID = 4,
#endif
} aribcc_fontprovider_type_t;

typedef enum aribcc_textrenderer_type_t {
    ARIBCC_TEXTRENDERER_TYPE_AUTO = 0,
#if defined(ARIBCC_USE_CORETEXT)
    ARIBCC_TEXTRENDERER_TYPE_CORETEXT = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    ARIBCC_TEXTRENDERER_TYPE_DIRECTWRITE = 2,
#endif

#if defined(ARIBCC_USE_FREETYPE)
    ARIBCC_TEXTRENDERER_TYPE_FREETYPE = 3,
#endif
} aribcc_textrenderer_type_t;

typedef enum aribcc_caption_storage_policy_t {
    ARIBCC_CAPTION_STORAGE_POLICY_MINIMUM = 0,
    ARIBCC_CAPTION_STORAGE_POLICY_UNLIMITED = 1,
    ARIBCC_CAPTION_STORAGE_POLICY_UPPER_LIMIT_COUNT = 2,
    ARIBCC_CAPTION_STORAGE_POLICY_UPPER_LIMIT_DURATION = 3,
} aribcc_caption_storage_policy_t;

typedef enum aribcc_render_status_t {
    ARIBCC_RENDER_STATUS_ERROR = 0,
    ARIBCC_RENDER_STATUS_NO_IMAGE = 1,
    ARIBCC_RENDER_STATUS_GOT_IMAGE = 2,
    ARIBCC_RENDER_STATUS_GOT_IMAGE_UNCHANGED = 3,
} aribcc_render_status_t;

typedef struct aribcc_render_result_t {
    int64_t pts;
    int64_t duration;

    aribcc_image_t* images;
    uint32_t image_count;
} aribcc_render_result_t;

ARIBCC_API void aribcc_render_result_cleanup(aribcc_render_result_t* render_result);


// Opaque type
typedef struct aribcc_renderer_t aribcc_renderer_t;


ARIBCC_API aribcc_renderer_t* aribcc_renderer_alloc(aribcc_context_t* context);

ARIBCC_API void aribcc_renderer_free(aribcc_renderer_t* renderer);

ARIBCC_API bool aribcc_renderer_initialize(aribcc_renderer_t* renderer,
                                           aribcc_captiontype_t caption_type,
                                           aribcc_fontprovider_type_t font_provider_type,
                                           aribcc_textrenderer_type_t text_renderer_type);

ARIBCC_API void aribcc_renderer_set_stroke_width(aribcc_renderer_t* renderer, float dots);

ARIBCC_API void aribcc_renderer_set_replace_drcs(aribcc_renderer_t* renderer, bool replace);

ARIBCC_API void aribcc_renderer_set_force_stroke_text(aribcc_renderer_t* renderer, bool force_stroke);

ARIBCC_API void aribcc_renderer_set_force_no_ruby(aribcc_renderer_t* renderer, bool force_no_ruby);

ARIBCC_API void aribcc_renderer_set_force_no_background(aribcc_renderer_t* renderer, bool force_no_background);

ARIBCC_API bool aribcc_renderer_set_default_font_family(aribcc_renderer_t* renderer,
                                                        const char** font_family,
                                                        size_t family_count,
                                                        bool force_default);

ARIBCC_API bool aribcc_renderer_set_language_specific_font_family(aribcc_renderer_t* renderer,
                                                                  uint32_t language_code,
                                                                  const char** font_family,
                                                                  size_t family_count);

ARIBCC_API bool aribcc_renderer_set_frame_size(aribcc_renderer_t* renderer, int frame_width, int frame_height);

ARIBCC_API bool aribcc_renderer_set_margins(aribcc_renderer_t* renderer, int top, int bottom, int left, int right);

ARIBCC_API void aribcc_renderer_set_storage_policy(aribcc_renderer_t* renderer,
                                                   aribcc_caption_storage_policy_t storage_policy,
                                                   size_t upper_limit);

ARIBCC_API bool aribcc_renderer_append_caption(aribcc_renderer_t* renderer, const aribcc_caption_t* caption);

ARIBCC_API aribcc_render_status_t aribcc_renderer_render(aribcc_renderer_t* renderer,
                                                         int64_t pts,
                                                         aribcc_render_result_t* out_result);

ARIBCC_API void aribcc_renderer_flush(aribcc_renderer_t* renderer);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_RENDERER_H
