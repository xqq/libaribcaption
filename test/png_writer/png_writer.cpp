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
#include <cstdlib>
#include <cstdio>
#include <png.h>
#include "aribcaption/image.h"
#include "aribcaption/image.hpp"
#include "renderer/bitmap.hpp"
#include "png_writer.h"
#include "png_writer.hpp"

using namespace aribcaption;

extern "C" bool png_writer_write_image_c(const char* filename, const aribcc_image_t* image) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "fopen() failed\n");
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fprintf(stderr, "png_create_write_struct() failed\n");
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "png_create_info_struct() failed\n");
        return false;
    }

    if (setjmp(png_jmpbuf(png)))
        return false;

    png_init_io(png, fp);

    // Output is 8bit depth, RGBA format.
    png_set_IHDR(
        png,
        info,
        image->width, image->height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    std::vector<png_bytep> row_pointers;

    for (int y = 0; y < image->height ; y++) {
        auto base = reinterpret_cast<const uint8_t*>(image->bitmap);
        uint8_t* ptr = const_cast<uint8_t*>(base) + y * image->stride;
        row_pointers.push_back(ptr);
    }

    png_write_image(png, row_pointers.data());
    png_write_end(png, nullptr);

    fclose(fp);

    png_destroy_write_struct(&png, &info);
    return true;
}

bool png_writer_write_image(const char* filename, const Image& image) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "fopen() failed\n");
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fprintf(stderr, "png_create_write_struct() failed\n");
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "png_create_info_struct() failed\n");
        return false;
    }

    if (setjmp(png_jmpbuf(png)))
        return false;

    png_init_io(png, fp);

    // Output is 8bit depth, RGBA format.
    png_set_IHDR(
        png,
        info,
        image.width, image.height,
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    std::vector<png_bytep> row_pointers;

    for (int y = 0; y < image.height ; y++) {
        auto base = reinterpret_cast<const uint8_t*>(image.bitmap.data());
        uint8_t* ptr = const_cast<uint8_t*>(base) + y * image.stride;
        row_pointers.push_back(ptr);
    }

    png_write_image(png, row_pointers.data());
    png_write_end(png, nullptr);

    fclose(fp);

    png_destroy_write_struct(&png, &info);
    return true;
}


bool png_writer_write_bitmap(const char* filename, const Bitmap& bitmap) {
    FILE *fp = fopen(filename, "wb");
    if (!fp) {
        fprintf(stderr, "fopen() failed\n");
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        fprintf(stderr, "png_create_write_struct() failed\n");
        return false;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        fprintf(stderr, "png_create_info_struct() failed\n");
        return false;
    }

    if (setjmp(png_jmpbuf(png)))
        return false;

    png_init_io(png, fp);

    // Output is 8bit depth, RGBA format.
    png_set_IHDR(
        png,
        info,
        bitmap.width(), bitmap.height(),
        8,
        PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT
    );
    png_write_info(png, info);

    std::vector<png_bytep> row_pointers;

    for (int y = 0; y < bitmap.height() ; y++) {
        auto addr = reinterpret_cast<png_bytep>(const_cast<ColorRGBA*>(bitmap.GetPixelAt(0, y)));
        row_pointers.push_back(addr);
    }

    png_write_image(png, row_pointers.data());
    png_write_end(png, nullptr);

    fclose(fp);

    png_destroy_write_struct(&png, &info);
    return true;
}
