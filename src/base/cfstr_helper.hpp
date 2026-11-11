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

#ifndef ARIBCAPTION_CFSTR_HELPER_HPP
#define ARIBCAPTION_CFSTR_HELPER_HPP

#include <CoreFoundation/CFBase.h>
#include <string>
#include <string_view>
#include "base/scoped_cfref.hpp"

namespace aribcaption::cfstr {

inline std::string CFStringToStdString(CFStringRef cfstr) {
    std::string str;

    if (!cfstr) {
        return str;
    }

    const char* cstring_ptr = CFStringGetCStringPtr(cfstr, kCFStringEncodingUTF8);

    if (cstring_ptr) {
        str = cstring_ptr;
    } else {  // cstring_ptr == nullptr
        // +1 for the NUL terminator written by CFStringGetCString(), which is not counted
        // by CFStringGetMaximumSizeForEncoding()
        CFIndex buf_size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8) + 1;
        str.resize(buf_size);
        if (!CFStringGetCString(cfstr, str.data(), buf_size, kCFStringEncodingUTF8)) {
            str.clear();
            return str;
        }
        size_t len = strlen(str.data());
        str.resize(len);
    }

    return str;
}

inline ScopedCFRef<CFStringRef> StdStringToCFString(std::string_view str) {
    ScopedCFRef<CFStringRef> cfstr(CFStringCreateWithBytes(nullptr,
                                                           reinterpret_cast<const UInt8*>(str.data()),
                                                           static_cast<CFIndex>(str.size()),
                                                           kCFStringEncodingUTF8,
                                                           false));
    return cfstr;
}

}  // namespace aribcaption::cfstr

#endif  // ARIBCAPTION_CFSTR_HELPER_HPP
