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

#include <cstring>
#include <cstdlib>
#include "aribcaption/decoder.h"
#include "aribcaption/decoder.hpp"
#include "decoder/decoder_impl.hpp"

using namespace aribcaption;
using namespace aribcaption::internal;

extern "C" {

aribcc_decoder_t* aribcc_decoder_alloc(aribcc_context_t* context) {
    auto ctx = reinterpret_cast<Context*>(context);
    auto impl = new(std::nothrow) DecoderImpl(*ctx);
    return reinterpret_cast<aribcc_decoder_t*>(impl);
}

void aribcc_decoder_free(aribcc_decoder_t* decoder) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    delete impl;
}

bool aribcc_decoder_initialize(aribcc_decoder_t* decoder,
                               aribcc_encoding_scheme_t encoding_scheme,
                               aribcc_captiontype_t caption_type,
                               aribcc_profile_t profile,
                               aribcc_languageid_t language_id) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    return impl->Initialize(static_cast<EncodingScheme>(encoding_scheme),
                            static_cast<CaptionType>(caption_type),
                            static_cast<Profile>(profile),
                            static_cast<LanguageId>(language_id));
}

void aribcc_decoder_set_encoding_scheme(aribcc_decoder_t* decoder, aribcc_encoding_scheme_t encoding_scheme) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    impl->SetEncodingScheme(static_cast<EncodingScheme>(encoding_scheme));
}

void aribcc_decoder_set_caption_type(aribcc_decoder_t* decoder, aribcc_captiontype_t type) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    impl->SetCaptionType(static_cast<CaptionType>(type));
}

void aribcc_decoder_set_profile(aribcc_decoder_t* decoder, aribcc_profile_t profile) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    impl->SetProfile(static_cast<Profile>(profile));
}

void aribcc_decoder_switch_language(aribcc_decoder_t* decoder, aribcc_languageid_t language_id) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    impl->SwitchLanguage(static_cast<LanguageId>(language_id));
}

void aribcc_decoder_set_replace_msz_fullwidth_ascii(aribcc_decoder_t* decoder, bool replace) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    impl->SetReplaceMSZFullWidthAlphanumeric(replace);
}

void aribcc_decoder_set_replace_msz_fullwidth_japanese(aribcc_decoder_t* decoder, bool replace) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    impl->SetReplaceMSZFullWidthJapanese(replace);
}

uint32_t aribcc_decoder_query_iso6392_language_code(aribcc_decoder_t* decoder, aribcc_languageid_t language_id) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    return impl->QueryISO6392LanguageCode(static_cast<LanguageId>(language_id));
}

static void ConvertCaptionRegionToCAPI(const CaptionRegion& region, aribcc_caption_region_t* out_region) {
    out_region->x = region.x;
    out_region->y = region.y;
    out_region->width = region.width;
    out_region->height = region.height;
    out_region->is_ruby = region.is_ruby;

    out_region->char_count = static_cast<uint32_t>(region.chars.size());

    if (!region.chars.empty()) {
        out_region->chars = reinterpret_cast<aribcc_caption_char_t*>(
            calloc(out_region->char_count, sizeof(aribcc_caption_char_t))
        );
    }

    for (uint32_t i = 0; i < out_region->char_count; i++) {
        out_region->chars[i] = *reinterpret_cast<const aribcc_caption_char_t*>(&region.chars[i]);
    }
}

static void ConvertCaptionToCAPI(Caption&& caption, aribcc_caption_t* out_caption) {
    out_caption->type = static_cast<aribcc_captiontype_t>(caption.type);
    out_caption->flags = static_cast<aribcc_captionflags_t>(caption.flags);
    out_caption->iso6392_language_code = caption.iso6392_language_code;
    out_caption->pts = caption.pts;
    out_caption->wait_duration = caption.wait_duration;
    out_caption->plane_width = caption.plane_width;
    out_caption->plane_height = caption.plane_height;
    out_caption->has_builtin_sound = caption.has_builtin_sound;
    out_caption->builtin_sound_id = caption.builtin_sound_id;

    if (!caption.text.empty()) {
        out_caption->text = reinterpret_cast<char*>(malloc(caption.text.length() + 1));
        strcpy(out_caption->text, caption.text.c_str());
    }

    if (!caption.regions.empty()) {
        out_caption->region_count = static_cast<uint32_t>(caption.regions.size());
        out_caption->regions = reinterpret_cast<aribcc_caption_region_t*>(
            calloc(out_caption->region_count, sizeof(aribcc_caption_region_t))
        );
    }

    for (size_t i = 0; i < out_caption->region_count; i++) {
        auto& src = caption.regions[i];
        aribcc_caption_region_t* dst = &out_caption->regions[i];
        ConvertCaptionRegionToCAPI(src, dst);
    }

    if (!caption.drcs_map.empty()) {
        auto drcs_map = new(std::nothrow) std::unordered_map<uint32_t, DRCS>(std::move(caption.drcs_map));
        out_caption->drcs_map = reinterpret_cast<aribcc_drcsmap_t*>(drcs_map);
    }
}

aribcc_decode_status_t aribcc_decoder_decode(aribcc_decoder_t* decoder,
                                             const uint8_t* pes_data,
                                             size_t length,
                                             int64_t pts,
                                             aribcc_caption_t* out_caption) {
    static_assert(sizeof(aribcc_caption_char_t) == sizeof(CaptionChar));

    auto impl = reinterpret_cast<DecoderImpl*>(decoder);

    DecodeResult result;
    auto status = impl->Decode(pes_data, length, pts, result);

    memset(out_caption, 0, sizeof(*out_caption));

    if (status == DecodeStatus::kGotCaption) {
        Caption* caption = result.caption.get();
        ConvertCaptionToCAPI(std::move(*caption), out_caption);
    }

    return static_cast<aribcc_decode_status_t>(status);
}

void aribcc_decoder_flush(aribcc_decoder_t* decoder) {
    auto impl = reinterpret_cast<DecoderImpl*>(decoder);
    impl->Flush();
}

}  // extern "C"
