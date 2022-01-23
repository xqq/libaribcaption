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

#ifndef ARIBCAPTION_COLOR_H
#define ARIBCAPTION_COLOR_H

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int aribcc_color_t;

#define ARIBCC_MAKE_RGBA(r,g,b,a) (((r) & 0xff) | (((g) << 8) & 0xff00) | (((b) << 16) & 0xff0000) | (((a) << 24) & 0xff000000))

#define ARIBCC_COLOR_R(color)  ((color) & 0xff)
#define ARIBCC_COLOR_G(color)  (((color) >> 8) & 0xff)
#define ARIBCC_COLOR_B(color)  (((color) >> 16) & 0xff)
#define ARIBCC_COLOR_A(color)  (((color) >> 24) & 0xff)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // ARIBCAPTION_COLOR_H
