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

#ifndef ARIBCAPTION_BITMAP_HPP
#define ARIBCAPTION_BITMAP_HPP

#include <cstdint>
#include <vector>
#include <type_traits>
#include "aligned_alloc.hpp"
#include "color.hpp"
#include "image.hpp"

namespace aribcaption {

class Bitmap {
public:
    static constexpr size_t kAlignedTo = 32;
public:
    static Image ToImage(Bitmap&&);
public:
    Bitmap(int width, int height, PixelFormat pixel_format);
    ~Bitmap() = default;
    Bitmap(const Bitmap& bmp) = default;
    Bitmap(Bitmap&& bmp) = default;
    Bitmap& operator=(const Bitmap&) = default;
    Bitmap& operator=(Bitmap&&) = default;
public:
    [[nodiscard]] inline uint8_t* data() { return pixels.data(); }

    [[nodiscard]] inline const uint8_t* data() const { return pixels.data(); }

    [[nodiscard]] inline ColorRGBA* GetPixels() {
        return reinterpret_cast<ColorRGBA*>(pixels.data());
    };

    [[nodiscard]] inline const ColorRGBA* GetPixels() const {
        return reinterpret_cast<const ColorRGBA*>(pixels.data());
    };

    [[nodiscard]] inline ColorRGBA* GetPixelAt(int x, int y) {
        auto ptr = reinterpret_cast<ColorRGBA*>(pixels.data());
        return ptr + y * stride_ + x;
    };

    [[nodiscard]] inline const ColorRGBA* GetPixelAt(int x, int y) const {
        auto ptr = reinterpret_cast<const ColorRGBA*>(pixels.data());
        return ptr + y * stride_ + x;
    };

    [[nodiscard]] inline size_t size() const { return pixels.size(); }

    [[nodiscard]] inline int width() const { return width_; }

    [[nodiscard]] inline int height() const { return height_; }

    [[nodiscard]] inline int stride() const { return stride_; }

    [[nodiscard]] inline PixelFormat pixel_format() const { return pixel_format_; }
private:
    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
    PixelFormat pixel_format_ = PixelFormat::kPixelFormatDefault;

    std::vector<uint8_t, AlignedAllocator<uint8_t, kAlignedTo>> pixels;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_BITMAP_HPP
