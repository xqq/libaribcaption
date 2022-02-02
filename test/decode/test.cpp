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

#ifdef _WIN32
    #include <windows.h>
#endif

#include <cstdint>
#include "aribcaption/context.hpp"
#include "aribcaption/caption.hpp"
#include "aribcaption/decoder.hpp"
#include "sample_data.h"

#ifdef _WIN32
class UTF8CodePage {
public:
    UTF8CodePage() : old_codepage_(GetConsoleOutputCP()) {
        SetConsoleOutputCP(CP_UTF8);
    }
    ~UTF8CodePage() {
        SetConsoleOutputCP(old_codepage_);
    }
private:
    UINT old_codepage_;
};
#endif

int main(int argc, const char* argv[]) {
#ifdef _WIN32
    UTF8CodePage enable_utf8_console;
#endif

    aribcaption::Context context;
    context.SetLogcatCallback([](aribcaption::LogLevel level, const char* message) {
        if (level == aribcaption::LogLevel::kError) {
            fprintf(stderr, "%s\n", message);
        } else {
            printf("%s\n", message);
        }
    });

    aribcaption::Decoder decoder(context);
    decoder.Initialize();

    aribcaption::DecodeResult result;

    auto status = decoder.Decode(sample_data_1, sizeof(sample_data_1), 0, result);
    printf("DecodeStatus: %d\n", status);
    if (status == aribcaption::DecodeStatus::kGotCaption) {
        printf("%s\n", result.caption->text.c_str());
    }

    status = decoder.Decode(sample_data_drcs_1, sizeof(sample_data_drcs_1), 0, result);
    printf("DecodeStatus: %d\n", status);
    if (status == aribcaption::DecodeStatus::kGotCaption) {
        printf("%s\n", result.caption->text.c_str());
    }

    return 0;
}
