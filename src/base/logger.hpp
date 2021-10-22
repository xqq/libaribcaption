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

#include <sstream>
#include "context.hpp"

namespace aribcaption {

class Logger {
public:
    Logger() = default;

    void SetCallback(const LogcatCB& logcat_cb) {
        logcat_cb_ = logcat_cb;
    }

    template <typename... Args>
    void e(const Args&... items) {
        std::ostringstream oss;
        WriteStream(oss, items...);
        logcat_cb_(LogLevel::kError, oss.str().c_str());
    }

    template <typename... Args>
    void w(const Args&... items) {
        std::ostringstream oss;
        WriteStream(oss, items...);
        logcat_cb_(LogLevel::kWarning, oss.str().c_str());
    }

    template <typename... Args>
    void v(const Args&... items) {
        std::ostringstream oss;
        WriteStream(oss, items...);
        logcat_cb_(LogLevel::kVerbose, oss.str().c_str());
    }
public:
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
private:
    template <typename T>
    void WriteStream(std::ostringstream& oss, const T& t) {
        oss << t;
    }

    template <typename T, typename... Args>
    void WriteStream(std::ostringstream& oss, const T& t, const Args&... items) {
        oss << t;
        WriteStream(oss, items...);
    }
private:
    LogcatCB logcat_cb_;
};


}  // namespace aribcaption

#endif  // ARIBCAPTION_LOGGER_HPP
