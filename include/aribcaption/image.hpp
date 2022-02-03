/*
 * Copyright (C) 2021 magicxqq <xqq@xqq.im>. All rights reserved.
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

#ifndef ARIBCAPTION_IMAGE_HPP
#define ARIBCAPTION_IMAGE_HPP

#include <cstdint>
#include <vector>
#include "aligned_alloc.hpp"

namespace aribcaption {

/**
 * enums for pixel format used by aribcc api.
 *
 * Only @kRGBA8888 is used for now.
 */
enum class PixelFormat {
    kRGBA8888 = 0,
    kDefault = kRGBA8888,
};

/**
 * Structure represents a rendered caption image produced by the renderer
 */
struct Image {
public:
    static constexpr size_t kAlignedTo = 32;
public:
    int width = 0;     ///< bitmap width
    int height = 0;    ///< bitmap height
    int stride = 0;    ///< bytes in a line, include margins for memory alignment

    int dst_x = 0;     ///< x coordinate of bitmap's top-left corner inside the player's renderer frame
    int dst_y = 0;     ///< y coordinate of bitmap's top-left corner inside the player's renderer frame

    PixelFormat pixel_format = PixelFormat::kDefault;    ///< pixel format, always be kRGBA8888

    std::vector<uint8_t, AlignedAllocator<uint8_t, kAlignedTo>> bitmap;
public:
    Image() = default;
    Image(const Image&) = default;
    Image(Image&&) noexcept = default;
    Image& operator=(const Image&) = default;
    Image& operator=(Image&&) noexcept = default;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_IMAGE_HPP
