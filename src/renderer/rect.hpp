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

#ifndef ARIBCAPTION_RECT_HPP
#define ARIBCAPTION_RECT_HPP

#include <cstring>
#include <algorithm>

// Workaround Windows.h (minwindef.h) max/min macro definitions
#ifdef max
    #undef max
#endif

#ifdef min
    #undef min
#endif

namespace aribcaption {

struct Rect {
    int left = 0;    // x coordinate of the left side
    int top = 0;     // y coordinate of the top side
    int right = 0;   // x coordinate of the right side plus one
    int bottom = 0;  // y coordinate of the bottom side plus one
public:
    constexpr Rect() = default;
    constexpr Rect(int left, int top, int right, int bottom) : left(left), top(top), right(right), bottom(bottom) {}
    constexpr Rect(const Rect& rect) = default;
    constexpr Rect& operator=(const Rect& rect) = default;

    [[nodiscard]]
    constexpr int x() const { return left; }

    [[nodiscard]]
    constexpr int y() const { return top; }

    [[nodiscard]]
    constexpr int width() const { return right - left; }

    [[nodiscard]]
    constexpr int height() const { return bottom - top; }

    [[nodiscard]]
    constexpr bool Contains(int x, int y) const {
        return x >= left && x < right && y >= top && y < bottom;
    }

    constexpr void Include(int x, int y) {
        left = std::min(left, x);
        top = std::min(top, y);
        right = std::max(right, x + 1);
        bottom = std::max(bottom, y + 1);
    }

    friend bool operator==(const Rect& a, const Rect& b) {
        return !memcmp(&a, &b, sizeof(a));
    }

    friend bool operator!=(const Rect& a, const Rect& b) {
        return !(a == b);
    }
public:
    static inline constexpr Rect ClipRect(const Rect& a, const Rect& b) {
        Rect clipped;

        clipped.left = std::max(a.left, b.left);
        clipped.top = std::max(a.top, b.top);
        clipped.right = std::min(a.right, b.right);
        clipped.bottom = std::min(a.bottom, b.bottom);

        return clipped;
    }
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_RECT_HPP
