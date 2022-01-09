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

#ifndef ARIBCAPTION_LOGGER_HPP
#define ARIBCAPTION_LOGGER_HPP

#include "aribcaption/context.hpp"

#if defined(__clang__) || defined(__GNUC__)
    #define ATTRIBUTE_FORMAT_PRINTF(a,b) __attribute__((__format__ (__printf__, a, b)))
    #define MSVC_FORMAT_CHECK(p) p
#elif defined(_MSC_VER)
    #include <sal.h>
    #define ATTRIBUTE_FORMAT_PRINTF(a,b)
    #define MSVC_FORMAT_CHECK(p) _Printf_format_string_ p
#else
    #define ATTRIBUTE_FORMAT_PRINTF(a,b)
    #define MSVC_FORMAT_CHECK(p) p
#endif

namespace aribcaption {

class Logger {
public:
    Logger() = default;

    void SetCallback(const LogcatCB& logcat_cb) {
        logcat_cb_ = logcat_cb;
    }

    void e(MSVC_FORMAT_CHECK(const char* format), ...) ATTRIBUTE_FORMAT_PRINTF(2, 3);

    void w(MSVC_FORMAT_CHECK(const char* format), ...) ATTRIBUTE_FORMAT_PRINTF(2, 3);

    void v(MSVC_FORMAT_CHECK(const char* format), ...) ATTRIBUTE_FORMAT_PRINTF(2, 3);
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
private:
    LogcatCB logcat_cb_;
};


}  // namespace aribcaption

#endif  // ARIBCAPTION_LOGGER_HPP
