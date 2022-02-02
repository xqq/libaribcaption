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

#ifndef ARIBCAPTION_WCHAR_HELPER_HPP
#define ARIBCAPTION_WCHAR_HELPER_HPP

#include <windows.h>
#include <cstring>
#include <string>
#include "base/always_inline.hpp"

namespace aribcaption::wchar {

namespace internal {

ALWAYS_INLINE std::wstring u8_to_wide(const char* u8, size_t len) {
    std::wstring wide_result;
    int required_length = MultiByteToWideChar(CP_UTF8,
                                              0,
                                              u8,
                                              static_cast<int>(len),
                                              nullptr,
                                              0);
    if (required_length > 0) {
        wide_result.resize(required_length);
        MultiByteToWideChar(CP_UTF8,
                            0,
                            u8,
                            static_cast<int>(len),
                            wide_result.data(),
                            required_length);
    }
    return wide_result;
}

ALWAYS_INLINE std::string wide_to_u8(const wchar_t* wide, size_t len) {
    std::string u8_result;
    int required_length = WideCharToMultiByte(CP_UTF8,
                                              0,
                                              wide,
                                              static_cast<int>(len),
                                              nullptr,
                                              0,
                                              nullptr,
                                              nullptr);
    if (required_length > 0) {
        u8_result.resize(required_length);
        WideCharToMultiByte(CP_UTF8,
                            0,
                            wide,
                            static_cast<int>(len),
                            u8_result.data(),
                            required_length,
                            nullptr,
                            nullptr);
    }
    return u8_result;
}

}  // namespace internal

inline std::wstring UTF8ToWideString(const char* u8) {
    size_t input_length = strlen(u8);
    return internal::u8_to_wide(u8, input_length);
}

inline std::wstring UTF8ToWideString(const std::string& u8) {
    return internal::u8_to_wide(u8.c_str(), u8.length());
}

inline std::string WideStringToUTF8(const wchar_t* wide) {
    size_t input_length = wcslen(wide);
    return internal::wide_to_u8(wide, input_length);
}

inline std::string WideStringToUTF8(const std::wstring& wide) {
    return internal::wide_to_u8(wide.c_str(), wide.length());
}

}  // namespace aribcaption::wchar


#endif  // ARIBCAPTION_WCHAR_HELPER_HPP
