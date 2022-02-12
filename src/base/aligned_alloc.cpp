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

#include <cstdint>
#include <cstdlib>
#include "aribcaption/aligned_alloc.hpp"

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

#if !(defined(_MSC_VER) || defined(__MINGW32__)) && !HAS_POSIX_MEMALIGN

static void* aligned_malloc_generic(size_t size, size_t alignment) {
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

static void aligned_free_generic(void* ptr) {
    if (ptr) {
        // Retrieve original address
        void* original_addr = *(reinterpret_cast<void**>(ptr) - 1);
        free(original_addr);
    }
}

#endif

void* AlignedAlloc(size_t size, size_t alignment) {
    void* ptr = nullptr;

#if defined(_MSC_VER) || defined(__MINGW32__)
    ptr = _aligned_malloc(size, alignment);
#elif HAS_POSIX_MEMALIGN
    if (posix_memalign(&ptr, alignment, size)) return nullptr;
#else
    ptr = aligned_malloc_generic(size, alignment);
#endif

    return ptr;
}

void AlignedFree(void* ptr) {
#if defined(_MSC_VER) || defined(__MINGW32__)
    _aligned_free(ptr);
#elif HAS_POSIX_MEMALIGN
    free(ptr);
#else
    aligned_free_generic(ptr);
#endif
}

}  // namespace aribcaption
