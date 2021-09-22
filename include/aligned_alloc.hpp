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

#ifndef ARIBCAPTION_ALIGNED_ALLOC_HPP
#define ARIBCAPTION_ALIGNED_ALLOC_HPP

#include <cstddef>
#include <new>
#include <type_traits>

namespace aribcaption {

void* AlignedAlloc(size_t size, size_t alignment);

void AlignedFree(void* ptr);

template <class T, std::size_t N>
class AlignedAllocator {
    static_assert(N % 4 == 0);
public:
    using value_type = T;
    using size_type = std::size_t;
    using pointer = std::add_pointer_t<value_type>;
    using const_pointer = std::add_pointer_t<const value_type>;

    template <class U>
    struct rebind {
        using other = AlignedAllocator<U, N>;
    };
public:
    AlignedAllocator() noexcept = default;

    AlignedAllocator(const AlignedAllocator&) noexcept = default;

    template <class U>
    explicit AlignedAllocator(const AlignedAllocator<U, N>&) noexcept {}

    pointer allocate(size_type n, const_pointer hint = nullptr) const {
        size_t size = n * sizeof(T);
        void* ptr = AlignedAlloc(size, N);

        if (!ptr) {
            throw std::bad_alloc();
        }
        return reinterpret_cast<pointer>(ptr);
    }

    void deallocate(pointer p, size_type) const noexcept {
        void* ptr = p;
        AlignedFree(ptr);
    }
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_ALIGNED_ALLOC_HPP
