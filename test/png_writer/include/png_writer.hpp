//
// Created by magicxqq on 2021/10/15.
//

#ifndef LIBARIBCAPTION_TEST_PNG_WRITER_HPP
#define LIBARIBCAPTION_TEST_PNG_WRITER_HPP

#include "aribcaption/image.hpp"
#include "renderer/bitmap.hpp"

bool png_writer_write_image(const char* filename, const aribcaption::Image& image);
bool png_writer_write_bitmap(const char* filename, const aribcaption::Bitmap& bitmap);

#endif  // LIBARIBCAPTION_TEST_PNG_WRITER_HPP
