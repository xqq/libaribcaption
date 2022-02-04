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

#include <algorithm>
#include <iterator>
#include "renderer/region_image_rearranger.hpp"

namespace aribcaption {

void RegionImageRearranger::BeginRearrange(const std::vector<CaptionRegion>& regions) {
    for (auto& region : regions) {
        tracks_.try_emplace(tracks_.end(), region.y, CaptionTrack{region.y, region.height, -1, -1});
    }
}

void RegionImageRearranger::RearrangeImage(const CaptionRegion& region, Image& image) {
    auto iter = tracks_.find(region.y);
    if (iter == tracks_.end()) {
        return;
    } else if (iter == tracks_.begin()) {
        iter->second.scaled_y = image.dst_y;
        iter->second.scaled_height = std::max(iter->second.scaled_height, image.height);
        return;
    }

    CaptionTrack& track = iter->second;
    CaptionTrack& prev_track = std::prev(iter)->second;

    if (track.original_y == prev_track.original_y + prev_track.original_height) {
        // Current track is tightly connected to the previous track
        if (prev_track.scaled_y == -1 || prev_track.scaled_height == -1) {
            // Lack of prev_track's scaled size data, fill in scaled y / height and skip rearrange
            track.scaled_y = image.dst_y;
            track.scaled_height = std::max(track.scaled_height, image.height);
        } else {
            // Rearrange image based on previous caption track
            if (track.scaled_y == -1) {
                track.scaled_y = prev_track.scaled_y + prev_track.scaled_height;
            }
            track.scaled_height = std::max(track.scaled_height, image.height);
            image.dst_y = track.scaled_y;
        }
    }
}

void RegionImageRearranger::EndRearrange() {
    tracks_.clear();
}

}  // namespace aribcaption
