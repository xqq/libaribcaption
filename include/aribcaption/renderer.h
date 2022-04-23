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

/**
 * Enums for FontProvider indication
 */
typedef enum aribcc_fontprovider_type_t {
    /**
     * Detect and select FontProvider automatically. Should be used in most cases.
     */
    ARIBCC_FONTPROVIDER_TYPE_AUTO = 0,

#if defined(ARIBCC_USE_CORETEXT)
    /**
     * FontProvider relies on Apple CoreText API. Available on macOS and iOS.
     */
    ARIBCC_FONTPROVIDER_TYPE_CORETEXT = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    /**
     * FontProvider relies on DirectWrite API. Available on Windows 7+.
     */
    ARIBCC_FONTPROVIDER_TYPE_DIRECTWRITE = 2,
#endif

#if defined(ARIBCC_USE_FONTCONFIG)
    /**
     * FontProvider using libfontconfig. Usually been used on Linux platforms.
     */
    ARIBCC_FONTPROVIDER_TYPE_FONTCONFIG = 3,
#endif

#if defined(ARIBCC_IS_ANDROID)
    /**
     * FontProvider for Android. Available on Android 2.x+.
     */
    ARIBCC_FONTPROVIDER_TYPE_ANDROID = 4,
#endif

#if defined(ARIBCC_USE_GDI_FONT)
    /**
     * FontProvder based on Win32 GDI API. Available on Windows 2000+.
     */
    ARIBCC_FONTPROVIDER_TYPE_GDI = 5
#endif
} aribcc_fontprovider_type_t;

/**
 * Enums for TextRenderer indication
 */
typedef enum aribcc_textrenderer_type_t {
    /**
     * Detect and select TextRenderer automatically. Should be used in most cases.
     */
    ARIBCC_TEXTRENDERER_TYPE_AUTO = 0,

#if defined(ARIBCC_USE_CORETEXT)
    /**
     * Apple CoreText API based TextRenderer. Available on macOS and iOS.
     */
    ARIBCC_TEXTRENDERER_TYPE_CORETEXT = 1,
#endif

#if defined(ARIBCC_USE_DIRECTWRITE)
    /**
     * DirectWrite API based TextRenderer. Available on Windows 7+.
     */
    ARIBCC_TEXTRENDERER_TYPE_DIRECTWRITE = 2,
#endif

#if defined(ARIBCC_USE_FREETYPE)
    /**
     * Freetype based TextRenderer. Available on all platforms.
     */
    ARIBCC_TEXTRENDERER_TYPE_FREETYPE = 3,
#endif
} aribcc_textrenderer_type_t;

/**
 * Enums for Renderer's caption storage policy indication
 */
typedef enum aribcc_caption_storage_policy_t {
    /**
     * The renderer will only keep minimal amount of captions for subsequent rendering if necessary.
     * This is the default behavior.
     */
    ARIBCC_CAPTION_STORAGE_POLICY_MINIMUM = 0,

    /**
     * The renderer will never evict appended captions unless you call the @aribcc_renderer_flush().
     * This may result in memory waste. Use at your own risk.
     */
    ARIBCC_CAPTION_STORAGE_POLICY_UNLIMITED = 1,

    /**
     * The renderer will keep appended captions at an upper limit of count.
     */
    ARIBCC_CAPTION_STORAGE_POLICY_UPPER_LIMIT_COUNT = 2,

    /**
     * The renderer will keep appended captions at an upper limit of duration, in milliseconds.
     */
    ARIBCC_CAPTION_STORAGE_POLICY_UPPER_LIMIT_DURATION = 3,
} aribcc_caption_storage_policy_t;

/**
 * Enums for reporting rendering status
 *
 * See @aribcc_renderer_render()
 */
typedef enum aribcc_render_status_t {
    ARIBCC_RENDER_STATUS_ERROR = 0,
    ARIBCC_RENDER_STATUS_NO_IMAGE = 1,
    ARIBCC_RENDER_STATUS_GOT_IMAGE = 2,
    ARIBCC_RENDER_STATUS_GOT_IMAGE_UNCHANGED = 3,
} aribcc_render_status_t;

/**
 * Structure for holding rendered caption images
 *
 * See @aribcc_renderer_render()
 */
typedef struct aribcc_render_result_t {
    int64_t pts;             ///< PTS of rendered caption
    int64_t duration;        ///< duration of rendered caption, may be ARIBCC_DURATION_INDEFINITE

    /**
     * Rendered images array, may be NULL if error occurred or ARIBCC_RENDER_STATUS_NO_IMAGE returned.
     *
     * Do not manually free this array if the render result is received from the aribcc_renderer_t,
     * instead, call @aribcc_render_result_cleanup().
     */
    aribcc_image_t* images;
    uint32_t image_count;    ///< element count of images array
} aribcc_render_result_t;

/**
 * Cleanup the aribcc_render_result_t structure.
 *
 * Call this function only if if you received the render result from aribcc API.
 * Otherwise it may cause a crash.
 *
 * This function doesn't release the memory of the @aribcc_render_result_t itself.
 */
ARIBCC_API void aribcc_render_result_cleanup(aribcc_render_result_t* render_result);


/**
 * ARIB STD-B24 caption renderer
 *
 * Opaque type
 */
typedef struct aribcc_renderer_t aribcc_renderer_t;

/**
 * A context is needed for allocating the Renderer.
 *
 * The context shouldn't be freed before any other object constructed from the context has been freed.
 */
ARIBCC_API aribcc_renderer_t* aribcc_renderer_alloc(aribcc_context_t* context);

/**
 * Free the renderer and all related resources
 */
ARIBCC_API void aribcc_renderer_free(aribcc_renderer_t* renderer);

/**
 * Initialize function must be called before calling any other member functions.
 *
 * @param renderer            @aribcc_renderer_t
 * @param caption_type        Indicate caption type (kCaption / kSuperimpose)
 * @param font_provider_type  Indicate @FontProviderType. Use kAuto in most cases.
 * @param text_renderer_type  Indicate @TextRendererType. Use kAuto in most cases.
 * @return true on success
 */
ARIBCC_API bool aribcc_renderer_initialize(aribcc_renderer_t* renderer,
                                           aribcc_captiontype_t caption_type,
                                           aribcc_fontprovider_type_t font_provider_type,
                                           aribcc_textrenderer_type_t text_renderer_type);

/**
 * Indicate stroke width for stroke text, in dots (relative)
 *
 * @param renderer  @aribcc_renderer_t
 * @param dots      must >= 0.0f
 */
ARIBCC_API void aribcc_renderer_set_stroke_width(aribcc_renderer_t* renderer, float dots);

/**
 * Indicate whether render replaced DRCS characters as Unicode characters
 *
 * @param renderer  @aribcc_renderer_t
 * @param replace   default as true
 */
ARIBCC_API void aribcc_renderer_set_replace_drcs(aribcc_renderer_t* renderer, bool replace);

/**
 * Indicate whether always render stroke text for all characters regardless of the indication by CharStyle
 *
 * @param renderer      @aribcc_renderer_t
 * @param force_stroke  default as false
 */
ARIBCC_API void aribcc_renderer_set_force_stroke_text(aribcc_renderer_t* renderer, bool force_stroke);

/**
 * Indicate whether ignore rendering for ruby-like (furigana) characters
 *
 * @param renderer       @aribcc_renderer_t
 * @param force_no_ruby  default as false
 */
ARIBCC_API void aribcc_renderer_set_force_no_ruby(aribcc_renderer_t* renderer, bool force_no_ruby);

/**
 * Indicate whether ignore background color rendering
 *
 * @param renderer             @aribcc_renderer_t
 * @param force_no_background  default as false
 */
ARIBCC_API void aribcc_renderer_set_force_no_background(aribcc_renderer_t* renderer, bool force_no_background);

/**
 * Merge rendered region images into one big image on aribcc_renderer_render() call.
 * @param renderer  @aribcc_renderer_t
 * @param merge     default as false
 */
ARIBCC_API void aribcc_renderer_set_merge_region_images(aribcc_renderer_t* renderer, bool merge);

/**
 * Indicate font families (an array of font family names) for default usage
 *
 * Default font family will be used only if captions' language is unknown (iso6392_language_code == 0).
 * Indicate force_default = true to force use these fonts always.
 *
 * The renderer contains an auto-fallback mechanism among indicated font families.
 * The renderer also contains predefined font indications for Windows / macOS / Linux / Android.
 *
 * @param font_family    Array of font family names
 * @param family_count   Element count of font_family array
 * @param force_default  Whether force use these font families for all languages
 * @return true on success
 *
 * @code
 * const char* font_families[] = {
 *     "Rounded M+ 1m for ARIB",
 *     "Hiragino Maru Gothic ProN",
 *     "sans-serif"
 * }
 * aribcc_renderer_set_default_font_family(renderer, font_families, 3, false);
 * @endcode
 */
ARIBCC_API bool aribcc_renderer_set_default_font_family(aribcc_renderer_t* renderer,
                                                        const char * const * font_family,
                                                        size_t family_count,
                                                        bool force_default);

/**
 * Indicate font families (an array of font family names) for specific language
 *
 * The renderer contains an auto-fallback mechanism among indicated font families.
 * The renderer also contains predefined font indications for Windows / macOS / Linux / Android.
 *
 * @param language_code  ISO639-2 Language Code, e.g. ARIBCC_MAKE_LANG('j', 'p', 'n')
 * @param font_family    Array of font family names
 * @param family_count   Element count of font_family array
 * @return true on success
 *
 * @code
 * const char* font_families[] = {
 *     "Rounded M+ 1m for ARIB",
 *     "Hiragino Maru Gothic ProN",
 *     "sans-serif"
 * }
 * aribcc_renderer_set_language_specific_font_family(renderer, ARIBCC_MAKE_LANG('j', 'p', 'n'), font_families, 3);
 * @endcode
 */
ARIBCC_API bool aribcc_renderer_set_language_specific_font_family(aribcc_renderer_t* renderer,
                                                                  uint32_t language_code,
                                                                  const char * const * font_family,
                                                                  size_t family_count);

/**
 * Set the renderer frame size in pixels, include margins. This function must be called before any render call.
 *
 * Usually rendered images will be inside this frame area, unless negative margin values are specified.
 *
 * @param renderer      @aribcc_renderer_t
 * @param frame_width   must be >= 0
 * @param frame_height  must be >= 0
 * @return true on success
 */
ARIBCC_API bool aribcc_renderer_set_frame_size(aribcc_renderer_t* renderer, int frame_width, int frame_height);

/**
 * Set the frame margins in pixels. This function must be called after calling to @aribcc_renderer_set_frame_size().
 * Call to this function is optional.
 *
 * Each value specifics the distance from the video rectangle to the renderer frame.
 * Positive margin value means there will be free space between renderer frame and video area,
 * while negative margin value lets renderer frame (visible area) inside the video, i.e. the video is cropped.
 *
 * If negative margin value has been indicated, rendered images may outside the renderer frame.
 *
 * @return true on success
 */
ARIBCC_API bool aribcc_renderer_set_margins(aribcc_renderer_t* renderer, int top, int bottom, int left, int right);

/**
 * Set storage policy for renderer's internal caption storage
 *
 * @param renderer       @aribcc_renderer_t
 * @param storage_policy See @aribcc_caption_storage_policy_t
 * @param upper_limit    Must be non-zero value for ARIBCC_CAPTION_STORAGE_POLICY_UPPER_LIMIT_COUNT or
 *                       ARIBCC_CAPTION_STORAGE_POLICY_UPPER_LIMIT_DURATION
 */
ARIBCC_API void aribcc_renderer_set_storage_policy(aribcc_renderer_t* renderer,
                                                   aribcc_caption_storage_policy_t storage_policy,
                                                   size_t upper_limit);

/**
 * Append a caption into renderer's internal storage for subsequent rendering
 *
 * If a caption with same PTS already exists in the storage, it will be replaced by the new one.
 *
 * @param renderer  @aribcc_renderer_t
 * @param caption   @aribcc_caption_t
 * @return true on success
 */
ARIBCC_API bool aribcc_renderer_append_caption(aribcc_renderer_t* renderer, const aribcc_caption_t* caption);

/**
 * Retrieve expected render status at specific PTS, rather than actually do rendering.
 *
 * Useful for detecting whether will got an identical image that is unchanged from the previous rendering.
 *
 * @param renderer    @aribcc_renderer_t
 * @param pts         Presentation timestamp, in milliseconds
 * @return            ARIBCC_RENDER_STATUS_GOT_IMAGE_UNCHANGED means a aribcc_renderer_render() call
 *                    at this PTS will return an image identical to the previous.
 */
ARIBCC_API aribcc_render_status_t aribcc_renderer_try_render(aribcc_renderer_t* renderer,
                                                             int64_t pts);

/**
 * Render caption at specific PTS
 *
 * @param renderer    @aribcc_renderer_t
 * @param pts         Presentation timestamp, in milliseconds
 * @param out_result  Write back parameter for passing rendered images, images will be NULL if status is kError / kNoImage
 * @return            ARIBCC_RENDER_STATUS_GOT_IMAGE / ARIBCC_RENDER_STATUS_GOT_IMAGE_UNCHANGED if rendered images provided
 *                    ARIBCC_RENDER_STATUS_GOT_IMAGE_UNCHANGED means this batch of images is completely identical to the previous call
 */
ARIBCC_API aribcc_render_status_t aribcc_renderer_render(aribcc_renderer_t* renderer,
                                                         int64_t pts,
                                                         aribcc_render_result_t* out_result);

/**
 * Clear caption storage inside the renderer. Will evict all the appended captions.
 *
 * Call this function if the stream has been seeked, or met non-monotonic PTS.
 */
ARIBCC_API void aribcc_renderer_flush(aribcc_renderer_t* renderer);


#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_RENDERER_H
