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

#include "aribcaption/context.hpp"
#include "base/logger.hpp"

namespace aribcaption {

Context::Context() : logger_(std::make_shared<Logger>()) {}

Context::~Context() = default;

Context::Context(Context&&) noexcept = default;

Context& Context::operator=(Context&&) noexcept = default;

void Context::SetLogcatCallback(const LogcatCB& logcat_cb) {
    logger_->SetCallback(logcat_cb);
}

std::shared_ptr<Logger> GetContextLogger(Context& context) {
    return context.logger_;
}

}  // namespace aribcaption
