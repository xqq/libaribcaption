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

#ifndef ARIBCAPTION_ALIGNED_ALLOCATOR_HPP
#define ARIBCAPTION_ALIGNED_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <type_traits>

#if defined(_MSC_VER) || defined(__MINGW32__)
    #include <malloc.h>
#endif

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
    #include <unistd.h>
    #if (_POSIX_VERSION >= 200112L)
        #define HAS_POSIX_MEMALIGN 1
    #else
        #define HAS_POSIX_MEMALIGN 0
    #endif
#endif

namespace aribcaption {

namespace internal {

inline void* aligned_malloc(size_t size, size_t alignment) {
    void* ptr = malloc(size + (alignment - 1) + sizeof(void*));
    if (!ptr) {
        return nullptr;
    }

    // Reserve at least a pointer size for saving original ptr
    uintptr_t aligned = reinterpret_cast<uintptr_t>(ptr) + sizeof(void*);
    uintptr_t misalign = aligned & (alignment - 1);
    if (misalign) {
        aligned += alignment - misalign;
    }

    // Save original address in front of aligned ptr
    *(reinterpret_cast<void**>(aligned) - 1) = ptr;

    return reinterpret_cast<void*>(aligned);
}

inline void aligned_free(void* ptr) {
    if (ptr) {
        // Retrieve original address
        void* original_addr = *(reinterpret_cast<void**>(ptr) - 1);
        free(original_addr);
    }
}

}  // namespace internal


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
        void* ptr = nullptr;

#if defined(_MSC_VER) || defined(__MINGW32__)
        ptr = _aligned_malloc(size, N);
#elif HAS_POSIX_MEMALIGN
        if (posix_memalign(&ptr, N, size)) throw std::bad_alloc();
#else
        ptr = internal::aligned_malloc(size, N);
#endif

        if (!ptr) {
            throw std::bad_alloc();
        }
        return ptr;
    }

    void deallocate(pointer p, size_type) const noexcept {
        void* ptr = p;
#if defined(_MSC_VER) || defined(__MINGW32__)
        _aligned_free(ptr);
#elif HAS_POSIX_MEMALIGN
        free(ptr);
#else
        internal::aligned_free(ptr);
#endif
    }

};

}  // namespace aribcaption

#endif  // ARIBCAPTION_ALIGNED_ALLOCATOR_HPP
