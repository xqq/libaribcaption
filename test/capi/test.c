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

#ifdef _WIN32
    #include <windows.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include "aribcaption/aribcaption.h"
#include "png_writer.h"
#include "sample_data.h"

const int frame_area_width = 3840;
const int frame_area_height = 2160;
const int margin_left = 0;
const int margin_top = 0;
const int margin_right = 0;
const int margin_bottom = 0;

void logcat_callback(aribcc_loglevel_t level, const char* message, void* userdata) {
    if (level == ARIBCC_LOGLEVEL_ERROR || level == ARIBCC_LOGLEVEL_WARNING) {
        fprintf(stderr, "%s\n", message);
        fflush(stderr);
    } else {
        printf("%s\n", message);
        fflush(stdout);
    }
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    UINT old_codepage = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);
#endif

    aribcc_context_t* ctx = aribcc_context_alloc();
    aribcc_context_set_logcat_callback(ctx, logcat_callback, NULL);

    aribcc_decoder_t* decoder = aribcc_decoder_alloc(ctx);

    aribcc_decoder_initialize(decoder,
                              ARIBCC_ENCODING_SCHEME_ARIB_STD_B24_JIS,
                              ARIBCC_CAPTIONTYPE_CAPTION,
                              ARIBCC_PROFILE_A,
                              ARIBCC_LANGUAGEID_FIRST);

    aribcc_caption_t caption = {0};
    aribcc_decode_status_t decode_status = aribcc_decoder_decode(decoder,
                                                                 sample_data_drcs_1,
                                                                 sizeof(sample_data_drcs_1),
                                                                 0,
                                                                 &caption);
    printf("DecodeStatus: %d\n", decode_status);
    if (decode_status == ARIBCC_DECODE_STATUS_GOT_CAPTION) {
        caption.iso6392_language_code = ARIBCC_MAKE_LANG('j', 'p', 'n');
        printf("%s\n", caption.text);
    } else if (decode_status == ARIBCC_DECODE_STATUS_NO_CAPTION) {
        printf("aribcc_decoder_decode() returned with no caption\n");
    } else {
        fprintf(stderr, "aribcc_decoder_decode() failed\n");
    }

    aribcc_renderer_t* renderer = aribcc_renderer_alloc(ctx);

    aribcc_renderer_initialize(renderer,
                               ARIBCC_CAPTIONTYPE_CAPTION,
                               ARIBCC_FONTPROVIDER_TYPE_AUTO,
                               ARIBCC_TEXTRENDERER_TYPE_AUTO);
    aribcc_renderer_set_storage_policy(renderer, ARIBCC_CAPTION_STORAGE_POLICY_MINIMUM, 0);
    aribcc_renderer_set_frame_size(renderer, frame_area_width, frame_area_height);
    aribcc_renderer_set_margins(renderer, margin_top, margin_bottom, margin_left, margin_right);
    aribcc_renderer_set_force_stroke_text(renderer, true);

    aribcc_renderer_append_caption(renderer, &caption);
    aribcc_caption_cleanup(&caption);

    aribcc_render_result_t render_result = {0};
    aribcc_render_status_t render_status = aribcc_renderer_render(renderer, 0, &render_result);
    printf("RenderStatus: %d\n", render_status);
    if (render_status == ARIBCC_RENDER_STATUS_ERROR) {
        fprintf(stderr, "aribcc_renderer_render() failed\n");
    } else if (render_status == ARIBCC_RENDER_STATUS_NO_IMAGE) {
        printf("aribcc_renderer_render() returned with no image\n");
    } else if (render_status == ARIBCC_RENDER_STATUS_GOT_IMAGE ||
               render_status == ARIBCC_RENDER_STATUS_GOT_IMAGE_UNCHANGED) {
        printf("ImageCount: %u\n", render_result.image_count);
    }

    for (uint32_t i = 0; i < render_result.image_count; i++) {
        aribcc_image_t* image = &render_result.images[i];
        char filename[32] = {0};
        snprintf(filename, sizeof(filename), "capi_test_image_%u.png", i);
        png_writer_write_image_c(filename, image);
    }

    aribcc_render_result_cleanup(&render_result);

    aribcc_renderer_free(renderer);
    aribcc_decoder_free(decoder);
    aribcc_context_free(ctx);


#ifdef _WIN32
    SetConsoleOutputCP(old_codepage);
#endif

    return 0;
}
