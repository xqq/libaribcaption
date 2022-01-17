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

#ifndef ARIBCAPTION_B24_DECODER_HPP
#define ARIBCAPTION_B24_DECODER_HPP

#include <cstdint>
#include <memory>
#include "aribcc_export.h"
#include "b24.hpp"
#include "caption.hpp"
#include "context.hpp"

namespace aribcaption {

namespace internal { class DecoderImpl; }

enum class EncodingScheme {
    kAuto = 0,
    kARIB_STD_B24_JIS = 1,
    kARIB_STD_B24_UTF8 = 2,
    kABNT_NBR_15606_1_Latin = 3,
    kISDB_T_Philippines_UTF8 = kARIB_STD_B24_UTF8,  // alias
};

enum class DecodeStatus {
    kError = 0,
    kNoCaption = 1,
    kGotCaption = 2
};

struct DecodeResult {
    std::unique_ptr<Caption> caption;
};

class Decoder {
public:
    ARIBCC_API explicit Decoder(Context& context);
    ARIBCC_API ~Decoder();
    ARIBCC_API Decoder(Decoder&&) noexcept;
    ARIBCC_API Decoder& operator=(Decoder&&) noexcept;
public:
    ARIBCC_API bool Initialize(EncodingScheme encoding_scheme = EncodingScheme::kAuto,
                               B24Type type = B24Type::kDefault,
                               B24Profile profile = B24Profile::kDefault,
                               B24LanguageId language_id = B24LanguageId::kDefault);
    ARIBCC_API void SetEncodingScheme(EncodingScheme encoding_scheme);
    ARIBCC_API void SetType(B24Type type);
    ARIBCC_API void SetProfile(B24Profile profile);
    ARIBCC_API void SetLanguageId(B24LanguageId language_id);
    ARIBCC_API void SetReplaceMSZFullWidthAlphanumeric(bool replace);
    [[nodiscard]]
    ARIBCC_API uint32_t QueryISO6392LanguageCode(B24LanguageId language_id) const;
    ARIBCC_API DecodeStatus Decode(const uint8_t* pes_data, size_t length, int64_t pts, DecodeResult& out_result);
    ARIBCC_API bool Flush();
public:
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
private:
    std::unique_ptr<internal::DecoderImpl> pimpl_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_B24_DECODER_HPP
