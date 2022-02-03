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
#include "aribcaption/aligned_alloc.hpp"
#include "aribcaption/color.hpp"
#include "aribcaption/image.hpp"
#include "base/always_inline.hpp"
#include "renderer/rect.hpp"

namespace aribcaption {

class Bitmap {
public:
    static constexpr size_t kAlignedTo = 32;
public:
    static Image ToImage(Bitmap&& bitmap);
    static Bitmap FromImage(Image&& image);
private:
    Bitmap() = default;
public:
    Bitmap(int width, int height, PixelFormat pixel_format);
    ~Bitmap() = default;
    Bitmap(const Bitmap& bmp) = default;
    Bitmap(Bitmap&& bmp) noexcept = default;
    Bitmap& operator=(const Bitmap&) = default;
    Bitmap& operator=(Bitmap&&) noexcept = default;
public:
    [[nodiscard]]
    ALWAYS_INLINE uint8_t* data() { return pixels.data(); }

    [[nodiscard]]
    ALWAYS_INLINE const uint8_t* data() const { return pixels.data(); }

    [[nodiscard]]
    ALWAYS_INLINE ColorRGBA* GetPixels() {
        return reinterpret_cast<ColorRGBA*>(pixels.data());
    };

    [[nodiscard]]
    ALWAYS_INLINE const ColorRGBA* GetPixels() const {
        return reinterpret_cast<const ColorRGBA*>(pixels.data());
    };

    [[nodiscard]]
    ALWAYS_INLINE ColorRGBA* GetPixelAt(int x, int y) {
        uint8_t* ptr = pixels.data() + y * (size_t)stride_ + x * sizeof(ColorRGBA);
        return reinterpret_cast<ColorRGBA*>(ptr);
    };

    [[nodiscard]]
    ALWAYS_INLINE const ColorRGBA* GetPixelAt(int x, int y) const {
        const uint8_t* ptr = pixels.data() + y * (size_t)stride_ + x * sizeof(ColorRGBA);
        return reinterpret_cast<const ColorRGBA*>(ptr);
    };

    [[nodiscard]]
    ALWAYS_INLINE Rect GetRect() const {
        return {0, 0, width_, height_};
    }

    [[nodiscard]]
    ALWAYS_INLINE size_t size() const { return pixels.size(); }

    [[nodiscard]]
    ALWAYS_INLINE int width() const { return width_; }

    [[nodiscard]]
    ALWAYS_INLINE int height() const { return height_; }

    [[nodiscard]]
    ALWAYS_INLINE int stride() const { return stride_; }

    [[nodiscard]]
    ALWAYS_INLINE PixelFormat pixel_format() const { return pixel_format_; }
private:
    int width_ = 0;
    int height_ = 0;
    int stride_ = 0;
    PixelFormat pixel_format_ = PixelFormat::kDefault;

    std::vector<uint8_t, AlignedAllocator<uint8_t, kAlignedTo>> pixels;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_BITMAP_HPP
