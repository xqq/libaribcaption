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
#include <cstdio>
#include "renderer/bitmap.hpp"
#include "renderer/canvas.hpp"
#include "stopwatch.hpp"

using namespace aribcaption;

int main(int argc, char** argv) {
    constexpr int count = 1000;

    auto stopwatch = StopWatch::Create();

    Bitmap background(3840, 2160, PixelFormat::kRGBA8888);
    Canvas canvas(background);
    canvas.ClearColor(ColorRGBA(255, 0, 0, 180));

    Bitmap foreground(3840, 2160, PixelFormat::kRGBA8888);
    Canvas fg_canvas(foreground);
    fg_canvas.ClearColor(ColorRGBA(0, 255, 0, 128));

    stopwatch->Start();

    for (int i = 0; i < count; i++) {
        canvas.DrawBitmap(foreground, 0, 0);
    }

    stopwatch->Stop();
    int64_t elapsed = stopwatch->GetMicroseconds();
    int64_t average = elapsed / count;

    printf("count = %d\ntotal = %lfms\naverage = %lfms\n",
           count,
           static_cast<double>(elapsed) / 1000.0f,
           static_cast<double>(average) / 1000.0f);

    return 0;
}