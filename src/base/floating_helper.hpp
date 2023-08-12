/*
 * Copyright (C) 2023 magicxqq <xqq@xqq.im>. All rights reserved.
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

#ifndef ARIBCAPTION_FLOATING_HELPER_HPP
#define ARIBCAPTION_FLOATING_HELPER_HPP

#include <cmath>
#include <type_traits>

namespace aribcaption::floating {

template <typename T,
          typename = std::enable_if_t<std::is_floating_point_v<T>>>
inline bool AlmostEquals(T lhs, T rhs, T epsilon) {
    T diff = std::fabs(lhs - rhs);
    T lhs_abs = std::fabs(lhs);
    T rhs_abs = std::fabs(rhs);

    T largest = lhs_abs > rhs_abs ? lhs_abs : rhs_abs;
    return diff <= largest * epsilon;
}

}  // namespace aribcaption::floating

#endif  // ARIBCAPTION_FLOATING_HELPER_HPP
