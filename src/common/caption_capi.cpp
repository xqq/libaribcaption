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

#include <cstddef>
#include <cstdint>
#include <cstring>
#include "aribcaption/caption.h"
#include "aribcaption/caption.hpp"
#include "base/utf_helper.hpp"

using namespace aribcaption;

extern "C" {

// aribcc_caption_char_t related function implementations
int aribcc_caption_char_get_section_width(aribcc_caption_char_t* ch) {
    float width = (float)(ch->char_width + ch->char_horizontal_spacing) * ch->char_horizontal_scale;
    return static_cast<int>(std::floor(width));
}

int aribcc_caption_char_get_section_height(aribcc_caption_char_t* ch) {
    float height = (float)(ch->char_height + ch->char_vertical_spacing) * ch->char_vertical_scale;
    return static_cast<int>(std::floor(height));
}


// aribcc_drcs_t related function implementations
aribcc_drcs_t* aribcc_drcs_alloc() {
    auto drcs = new(std::nothrow) DRCS();
    return reinterpret_cast<aribcc_drcs_t*>(drcs);
}

void aribcc_drcs_free(aribcc_drcs_t* drcs) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    delete drcspp;
}

aribcc_drcs_t* aribcc_drcs_clone(aribcc_drcs_t* drcs) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    DRCS* cloned = new(std::nothrow) DRCS(*drcspp);
    return reinterpret_cast<aribcc_drcs_t*>(cloned);
}

void aribcc_drcs_set_size(aribcc_drcs_t* drcs, int width, int height) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    drcspp->width = width;
    drcspp->height = height;
}

void aribcc_drcs_get_size(aribcc_drcs_t* drcs, int* width, int* height) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    *width = drcspp->width;
    *height = drcspp->height;
}

void aribcc_drcs_set_depth(aribcc_drcs_t* drcs, int depth, int depth_bits) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    drcspp->depth = depth;
    drcspp->depth_bits = depth_bits;
}

void aribcc_drcs_get_depth(aribcc_drcs_t* drcs, int* depth, int* depth_bits) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    *depth = drcspp->depth;
    *depth_bits = drcspp->depth_bits;
}

void aribcc_drcs_import_pixels(aribcc_drcs_t* drcs, const uint8_t* pixels, size_t size) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    drcspp->pixels.resize(size);
    memcpy(drcspp->pixels.data(), pixels, size);
}

void aribcc_drcs_get_pixels(aribcc_drcs_t* drcs, uint8_t** ppixels, size_t* psize) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    *ppixels = drcspp->pixels.data();
    *psize = drcspp->pixels.size();
}

void aribcc_drcs_set_md5(aribcc_drcs_t* drcs, const char* md5) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    drcspp->md5 = md5;
}

const char* aribcc_drcs_get_md5(aribcc_drcs_t* drcs) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    return drcspp->md5.c_str();
}

void aribcc_drcs_set_alternative_ucs4(aribcc_drcs_t* drcs, uint32_t ucs4) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    drcspp->alternative_ucs4 = ucs4;
    drcspp->alternative_text.clear();
    utf::UTF8AppendCodePoint(drcspp->alternative_text, ucs4);
}

uint32_t aribcc_drcs_get_alternative_ucs4(aribcc_drcs_t* drcs) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    return drcspp->alternative_ucs4;
}

const char* aribcc_drcs_get_alternative_text(aribcc_drcs_t* drcs) {
    auto drcspp = reinterpret_cast<DRCS*>(drcs);
    return drcspp->alternative_text.c_str();
}


// aribcc_drcsmap_t related function implementations
aribcc_drcsmap_t* aribcc_drcsmap_alloc() {
    auto map = new(std::nothrow) std::unordered_map<uint32_t, DRCS>();
    return reinterpret_cast<aribcc_drcsmap_t*>(map);
}

void aribcc_drcsmap_free(aribcc_drcsmap_t* drcs_map) {
    auto map = reinterpret_cast<std::unordered_map<uint32_t, DRCS>*>(drcs_map);
    delete map;
}

void aribcc_drcsmap_erase(aribcc_drcsmap_t* drcs_map, uint32_t key) {
    auto map = reinterpret_cast<std::unordered_map<uint32_t, DRCS>*>(drcs_map);
    map->erase(key);
}

void aribcc_drcsmap_put(aribcc_drcsmap_t* drcs_map, uint32_t key, const aribcc_drcs_t* drcs) {
    auto map = reinterpret_cast<std::unordered_map<uint32_t, DRCS>*>(drcs_map);
    auto drcspp = reinterpret_cast<const DRCS*>(drcs);
    map->insert_or_assign(key, *drcspp);
}

aribcc_drcs_t* aribcc_drcsmap_get(aribcc_drcsmap_t* drcs_map, uint32_t key) {
    auto map = reinterpret_cast<std::unordered_map<uint32_t, DRCS>*>(drcs_map);
    auto iter = map->find(key);
    if (iter != map->end()) {
        return reinterpret_cast<aribcc_drcs_t*>(&iter->second);
    }
    return nullptr;
}

void aribcc_drcsmap_clear(aribcc_drcsmap_t* drcs_map) {
    auto map = reinterpret_cast<std::unordered_map<uint32_t, DRCS>*>(drcs_map);
    map->clear();
}


// aribcc_caption_region_t related function implementations
void aribcc_caption_region_cleanup(aribcc_caption_region_t* region) {
    if (region->chars) {
        free(region->chars);
        region->chars = nullptr;
        region->char_count = 0;
    }
}


// aribcc_caption_t related function implementations
void aribcc_caption_cleanup(aribcc_caption_t* caption) {
    if (caption->text) {
        free(caption->text);
        caption->text = nullptr;
    }

    if (caption->regions) {
        for (uint32_t i = 0; i < caption->region_count; i++) {
            aribcc_caption_region_cleanup(&caption->regions[i]);
        }
        free(caption->regions);
        caption->regions = nullptr;
        caption->region_count = 0;
    }

    if (caption->drcs_map) {
        aribcc_drcsmap_free(caption->drcs_map);
        caption->drcs_map = nullptr;
    }
}


}  // extern "C"
