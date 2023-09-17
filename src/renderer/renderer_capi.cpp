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

#include <cstdlib>
#include "aribcaption/aligned_alloc.hpp"
#include "aribcaption/renderer.h"
#include "aribcaption/renderer.hpp"
#include "renderer/renderer_impl.hpp"

using namespace aribcaption;
using namespace aribcaption::internal;

static CaptionRegion ConstructCaptionRegionFromCAPI(const aribcc_caption_region_t* src) {
    CaptionRegion region;
    region.x = src->x;
    region.y = src->y;
    region.width = src->width;
    region.height = src->height;
    region.is_ruby = src->is_ruby;

    if (src->chars) {
        region.chars.resize(src->char_count);
        memcpy(region.chars.data(), src->chars, src->char_count * sizeof(aribcc_caption_char_t));
    }

    return region;
}

static Caption ConstructCaptionFromCAPI(const aribcc_caption_t* src) {
    Caption caption;
    caption.type = static_cast<CaptionType>(src->type);
    caption.flags = static_cast<CaptionFlags>(src->flags);
    caption.iso6392_language_code = src->iso6392_language_code;
    caption.pts = src->pts;
    caption.wait_duration = src->wait_duration;
    caption.plane_width = src->plane_width;
    caption.plane_height = src->plane_height;
    caption.has_builtin_sound = src->has_builtin_sound;
    caption.builtin_sound_id = src->builtin_sound_id;

    if (src->text) {
        caption.text = src->text;
    }

    if (src->regions) {
        for (uint32_t i = 0; i < src->region_count; i++) {
            caption.regions.emplace_back(ConstructCaptionRegionFromCAPI(&src->regions[i]));
        }
    }

    if (src->drcs_map) {
        auto drcs_map = reinterpret_cast<std::unordered_map<uint32_t, DRCS>*>(src->drcs_map);
        caption.drcs_map = *drcs_map;
    }

    return caption;
}

extern "C" {

void aribcc_render_result_cleanup(aribcc_render_result_t* render_result) {
    if (render_result->images) {
        for (uint32_t i = 0; i < render_result->image_count; i++) {
            aribcc_image_t* image = &render_result->images[i];
            aribcc_image_cleanup(image);
        }
        free(render_result->images);
        render_result->images = nullptr;
        render_result->image_count = 0;
    }
}

aribcc_renderer_t* aribcc_renderer_alloc(aribcc_context_t* context) {
    auto ctx = reinterpret_cast<Context*>(context);
    auto impl = new(std::nothrow) RendererImpl(*ctx);
    return reinterpret_cast<aribcc_renderer_t*>(impl);
}

void aribcc_renderer_free(aribcc_renderer_t* renderer) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    delete impl;
}

bool aribcc_renderer_initialize(aribcc_renderer_t* renderer,
                                aribcc_captiontype_t caption_type,
                                aribcc_fontprovider_type_t font_provider_type,
                                aribcc_textrenderer_type_t text_renderer_type) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    return impl->Initialize(static_cast<CaptionType>(caption_type),
                            static_cast<FontProviderType>(font_provider_type),
                            static_cast<TextRendererType>(text_renderer_type));
}

void aribcc_renderer_set_stroke_width(aribcc_renderer_t* renderer, float dots) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetStrokeWidth(dots);
}

void aribcc_renderer_set_replace_drcs(aribcc_renderer_t* renderer, bool replace) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetReplaceDRCS(replace);
}

void aribcc_renderer_set_force_stroke_text(aribcc_renderer_t* renderer, bool force_stroke) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetForceStrokeText(force_stroke);
}

void aribcc_renderer_set_force_no_ruby(aribcc_renderer_t* renderer, bool force_no_ruby) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetForceNoRuby(force_no_ruby);
}

void aribcc_renderer_set_force_no_background(aribcc_renderer_t* renderer, bool force_no_background) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetForceNoBackground(force_no_background);
}

void aribcc_renderer_set_merge_region_images(aribcc_renderer_t* renderer, bool merge) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetMergeRegionImages(merge);
}

bool aribcc_renderer_set_default_font_family(aribcc_renderer_t* renderer,
                                             const char * const * font_family,
                                             size_t family_count,
                                             bool force_default) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    std::vector<std::string> font_families;

    for (size_t i = 0; i < family_count; i++) {
        const char* font = font_family[i];
        font_families.emplace_back(font);
    }

    return impl->SetDefaultFontFamily(font_families, force_default);
}

bool aribcc_renderer_set_language_specific_font_family(aribcc_renderer_t* renderer,
                                                       uint32_t language_code,
                                                       const char * const * font_family,
                                                       size_t family_count) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    std::vector<std::string> font_families;

    for (size_t i = 0; i < family_count; i++) {
        const char* font = font_family[i];
        font_families.emplace_back(font);
    }

    return impl->SetLanguageSpecificFontFamily(language_code, font_families);
}

void aribcc_renderer_set_replace_msz_halfwidth_glyph(aribcc_renderer_t* renderer, bool replace) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetReplaceMSZHalfWidthGlyph(replace);
}

bool aribcc_renderer_set_frame_size(aribcc_renderer_t* renderer, int frame_width, int frame_height) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    return impl->SetFrameSize(frame_width, frame_height);
}

bool aribcc_renderer_set_margins(aribcc_renderer_t* renderer, int top, int bottom, int left, int right) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    return impl->SetMargins(top, bottom, left, right);
}

void aribcc_renderer_set_storage_policy(aribcc_renderer_t* renderer,
                                        aribcc_caption_storage_policy_t storage_policy,
                                        size_t upper_limit) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->SetStoragePolicy(static_cast<CaptionStoragePolicy>(storage_policy), upper_limit);
}

bool aribcc_renderer_append_caption(aribcc_renderer_t* renderer, const aribcc_caption_t* caption) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    Caption cap = ConstructCaptionFromCAPI(caption);
    return impl->AppendCaption(std::move(cap));
}

static void ConvertImageToCAPI(const Image& image, aribcc_image_t* out_image) {
    out_image->width = image.width;
    out_image->height = image.height;
    out_image->stride = image.stride;
    out_image->dst_x = image.dst_x;
    out_image->dst_y = image.dst_y;
    out_image->pixel_format = static_cast<aribcc_pixelformat_t>(image.pixel_format);

    if (!image.bitmap.empty()) {
        out_image->bitmap_size = static_cast<uint32_t>(image.bitmap.size());
        out_image->bitmap = reinterpret_cast<uint8_t*>(AlignedAlloc(out_image->bitmap_size, Image::kAlignedTo));
        memcpy(out_image->bitmap, image.bitmap.data(), out_image->bitmap_size);
    }
}

static void ConvertRenderResultToCAPI(const RenderResult& result, aribcc_render_result_t* out_result) {
    out_result->pts = result.pts;
    out_result->duration = result.duration;

    if (!result.images.empty()) {
        out_result->image_count = static_cast<uint32_t>(result.images.size());
        out_result->images = reinterpret_cast<aribcc_image_t*>(calloc(out_result->image_count, sizeof(aribcc_image_t)));

        for (uint32_t i = 0; i < out_result->image_count; i++) {
            const Image& src = result.images[i];
            aribcc_image_t* dst = &out_result->images[i];
            ConvertImageToCAPI(src, dst);
        }
    }
}

aribcc_render_status_t aribcc_renderer_try_render(aribcc_renderer_t* renderer, int64_t pts) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    RenderStatus status = impl->TryRender(pts);
    return static_cast<aribcc_render_status_t>(status);
}

aribcc_render_status_t aribcc_renderer_render(aribcc_renderer_t* renderer,
                                              int64_t pts,
                                              aribcc_render_result_t* out_result) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);

    RenderResult result;
    RenderStatus status = impl->Render(pts, result);

    memset(out_result, 0, sizeof(*out_result));

    if (status == RenderStatus::kGotImage || status == RenderStatus::kGotImageUnchanged) {
        ConvertRenderResultToCAPI(result, out_result);
    }

    return static_cast<aribcc_render_status_t>(status);
}

void aribcc_renderer_flush(aribcc_renderer_t* renderer) {
    auto impl = reinterpret_cast<RendererImpl*>(renderer);
    impl->Flush();
}

}  // extern "C"
