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

#ifndef ARIBCAPTION_SCOPED_CFREF_HPP
#define ARIBCAPTION_SCOPED_CFREF_HPP

#include <CoreFoundation/CoreFoundation.h>
#include <memory>
#include <type_traits>

namespace aribcaption {

template <typename T, T* P>
struct FunctorWrapper {
    template <typename... Args>
    auto operator()(Args&&... args) const -> decltype(P(std::forward<Args>(args)...)) {
        return P(std::forward<Args>(args)...);
    }
};

template <typename CFRef>
using ScopedCFRef = std::unique_ptr<std::remove_pointer_t<CFRef>, FunctorWrapper<decltype(CFRelease), CFRelease>>;

}  // namespace aribcaption

#endif  // ARIBCAPTION_SCOPED_CFREF_HPP
