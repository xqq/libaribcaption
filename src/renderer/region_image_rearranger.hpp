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

#ifndef ARIBCAPTION_REGION_IMAGE_REARRANGER_HPP
#define ARIBCAPTION_REGION_IMAGE_REARRANGER_HPP

#include <vector>
#include <map>
#include "aribcaption/caption.hpp"
#include "aribcaption/image.hpp"

namespace aribcaption {

class RegionImageRearranger {
public:
    RegionImageRearranger() = default;
    ~RegionImageRearranger() = default;
public:
    void BeginRearrange(const std::vector<CaptionRegion>& regions);
    void RearrangeImage(const CaptionRegion& region, Image& image);
    void EndRearrange();
private:
    struct CaptionTrack {
        int original_y = -1;
        int original_height = -1;
        int scaled_y = -1;
        int scaled_height = -1;
    };
private:
    // original_y => CaptionTrack, sorted by original_y
    std::map<int, CaptionTrack> tracks_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_REGION_IMAGE_REARRANGER_HPP
