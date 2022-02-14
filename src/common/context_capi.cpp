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

#include "aribcaption/context.h"
#include "aribcaption/context.hpp"

using namespace aribcaption;

extern "C" {

aribcc_context_t* aribcc_context_alloc() {
    auto ctx = new(std::nothrow) Context;
    return reinterpret_cast<aribcc_context_t*>(ctx);
}

void aribcc_context_set_logcat_callback(aribcc_context_t* context, aribcc_logcat_callback_t callback, void* userdata) {
    auto ctx = reinterpret_cast<Context*>(context);
    if (callback) {
        ctx->SetLogcatCallback([callback, userdata] (LogLevel level, const char* message) {
            callback(static_cast<aribcc_loglevel_t>(level), message, userdata);
        });
    } else {
        ctx->SetLogcatCallback(nullptr);
    }
}

void aribcc_context_free(aribcc_context_t* context) {
    auto ctx = reinterpret_cast<Context*>(context);
    delete ctx;
}

}  // extern "C"
