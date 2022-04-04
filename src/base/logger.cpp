/*
 * Copyright (C) 2022 magicxqq <xqq@xqq.im>. All rights reserved.
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

#include <cstdio>
#include <cstddef>
#include <cstdarg>
#include <string>
#include "base/logger.hpp"

namespace aribcaption {

void Logger::e(const char* format, ...) {
    if (!logcat_cb_) {
        return;
    }

    va_list args;
    va_start(args, format);
    auto length = static_cast<size_t>(std::vsnprintf(nullptr, 0, format, args));
    va_end(args);

    std::string buffer(length, 0);

    va_start(args, format);
    std::vsnprintf(buffer.data(), length + 1, format, args);
    va_end(args);

    logcat_cb_(LogLevel::kError, buffer.c_str());
}

void Logger::w(const char* format, ...) {
    if (!logcat_cb_) {
        return;
    }

    va_list args;
    va_start(args, format);
    auto length = static_cast<size_t>(std::vsnprintf(nullptr, 0, format, args));
    va_end(args);

    std::string buffer(length, 0);

    va_start(args, format);
    std::vsnprintf(buffer.data(), length + 1, format, args);
    va_end(args);

    logcat_cb_(LogLevel::kWarning, buffer.c_str());
}

void Logger::v(const char* format, ...) {
    if (!logcat_cb_) {
        return;
    }

    va_list args;
    va_start(args, format);
    auto length = static_cast<size_t>(std::vsnprintf(nullptr, 0, format, args));
    va_end(args);

    std::string buffer(length, 0);

    va_start(args, format);
    std::vsnprintf(buffer.data(), length + 1, format, args);
    va_end(args);

    logcat_cb_(LogLevel::kVerbose, buffer.c_str());
}

}  // namespace aribcaption
