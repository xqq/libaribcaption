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

enum class DecodeStatus {
    kError = 0,
    kNoCaption = 1,
    kGotCaption = 2
};

struct DecodeResult {
    std::unique_ptr<Caption> caption;
};

class ARIBCC_API Decoder {
public:
    explicit Decoder(Context& context);
    ~Decoder();
public:
    bool Initialize(B24Type type = B24Type::kDefault,
                    B24Profile profile = B24Profile::kDefault,
                    B24LanguageId language_id = B24LanguageId::kDefault);
    void SetType(B24Type type);
    void SetProfile(B24Profile profile);
    void SetLanguageId(B24LanguageId language_id);
    void SetDefaultLanguage(uint32_t iso6392_language_code);
    void SetReplaceMSZFullWidthAlphanumeric(bool replace);
    [[nodiscard]]
    uint32_t QueryISO6392LanguageCode(B24LanguageId language_id) const;
    DecodeStatus Decode(const uint8_t* pes_data, size_t length, int64_t pts, DecodeResult& out_result);
    bool Flush();
public:
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
private:
    std::unique_ptr<internal::DecoderImpl> pimpl_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_B24_DECODER_HPP
