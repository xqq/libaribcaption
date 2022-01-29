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
#include "caption.hpp"
#include "context.hpp"

namespace aribcaption {

namespace internal { class DecoderImpl; }

/**
 * Component tag IDs of ARIB caption profiles
 *
 * Defined in ARIB STD-B10, part 2, Annex J
 */
enum class Profile : uint8_t {
    /**
     * ARIB-Subtitle & teletext coding (A-Profile)
     */
    kProfileA = 0x0008,

    /**
     * Subtitle coding for digital terrestrial broadcasting (C profile)
     * Used in 1seg broadcasting.
     */
    kProfileC = 0x0012,
    kDefault = kProfileA
};

/**
 * Enums for language indication.
 *
 * ARIB captions supports multiple languages, with up to 8 languages per 1 ES (STD-B24, Chapter 4, Table 4-1)
 * But the operating rules have limited the maximum number of languages transmitted in 1 ES to be 2 languages
 * according to ARIB TR-B14, Fascicle 1 2/2, 4.2.1 (6) and 4.2.2.
 */
enum class LanguageId : uint8_t {
    kFirst = 1,
    kSecond = 2,
    kDefault = kFirst,
    kMax = kSecond
};

/**
 * Enums for encoding scheme indication.
 *
 * Indicate kAuto for automatic detecting.
 *
 * For handling Japan (Japanese) ISDB captions, indicate @kARIB_STD_B24_JIS which uses 8-char JIS encoding
 *
 * For handling SBTVD / ISDB-Tb captions used in South America, indicate @kABNT_NBR_15606_1_Latin
 * which is modified from @kARIB_STD_B24_JIS for encoding Latin characters.
 *
 * For handling Philippines ISDB-T captions which is modified to use UTF-8 encoding, indicate @kISDB_T_Philippines_UTF8.
 * This constant is identical to @kARIB_STD_B24_UTF8.
 */
enum class EncodingScheme {
    kAuto = 0,
    kARIB_STD_B24_JIS = 1,
    kARIB_STD_B24_UTF8 = 2,
    kABNT_NBR_15606_1_Latin = 3,
    kISDB_T_Philippines_UTF8 = kARIB_STD_B24_UTF8,  // alias
};

/**
 * Enums for reporting decoding status
 *
 * See @Decoder::Decode()
 */
enum class DecodeStatus {
    kError = 0,
    kNoCaption = 1,
    kGotCaption = 2
};

/**
 * Structure for holding decoded caption
 */
struct DecodeResult {
    std::unique_ptr<Caption> caption;
};

/**
 * ARIB STD-B24 caption decoder
 */
class Decoder {
public:
    /**
     * A context is needed for constructing the Decoder.
     *
     * The context shouldn't be destructed before any other object constructed from the context has been destructed.
     */
    ARIBCC_API explicit Decoder(Context& context);
    ARIBCC_API ~Decoder();
    ARIBCC_API Decoder(Decoder&&) noexcept;
    ARIBCC_API Decoder& operator=(Decoder&&) noexcept;
public:
    /**
     * Initialize function must be called before calling any other member functions.
     *
     * @param encoding_scheme Indicate encoding scheme, see @EncodingScheme
     * @param type            Indicate caption type (kCaption / kSuperimpose)
     * @param profile         Indicate caption profile (kProfileA / kProfileC)
     * @param language_id     Indicate caption language (kFirst / kSecond)
     * @return true on success
     */
    ARIBCC_API bool Initialize(EncodingScheme encoding_scheme = EncodingScheme::kAuto,
                               CaptionType type = CaptionType::kDefault,
                               Profile profile = Profile::kDefault,
                               LanguageId language_id = LanguageId::kDefault);
    /**
     * Indicate encoding scheme
     * @param encoding_scheme see @EncodingScheme
     */
    ARIBCC_API void SetEncodingScheme(EncodingScheme encoding_scheme);

    /**
     * Indicate caption type
     * @param type see @CaptionType
     */
    ARIBCC_API void SetCaptionType(CaptionType type);

    /**
     * Indicate caption profile
     * @param profile see @Profile
     */
    ARIBCC_API void SetProfile(Profile profile);

    /**
     * Switch caption language
     * @param language_id  see @LanguageId
     */
    ARIBCC_API void SwitchLanguage(LanguageId language_id);

    /**
     * Set whether to replace MSZ (Middle Size, half width) fullwidth alphanumerics with halfwidth alphanumerics
     * @param replace bool
     */
    ARIBCC_API void SetReplaceMSZFullWidthAlphanumeric(bool replace);

    /**
     * Query ISO639-2 Language Code for specific language id
     * @param language_id See @LanguageId
     * @return uint32_t, e.g. "jpn" => 6A 70 6E => 0x006A706E. May be 0 if language is unknown.
     */
    [[nodiscard]]
    ARIBCC_API uint32_t QueryISO6392LanguageCode(LanguageId language_id) const;

    /**
     * Decode caption PES data
     *
     * @param pes_data   pointer pointed to PES data, must be non-null
     * @param length     PES data length, must be greater than 0
     * @param pts        PES packet PTS, in milliseconds
     * @param out_result Write back parameter for passing decoded caption, only valid if DecodeStatus is kGotCaption
     * @return           kError on failure, kNoCaption if nothing obtained, kGotCaption if got a caption
     */
    ARIBCC_API DecodeStatus Decode(const uint8_t* pes_data, size_t length, int64_t pts, DecodeResult& out_result);

    /**
     * Reset decoder internal states
     */
    ARIBCC_API void Flush();
public:
    Decoder(const Decoder&) = delete;
    Decoder& operator=(const Decoder&) = delete;
private:
    std::unique_ptr<internal::DecoderImpl> pimpl_;
};

}  // namespace aribcaption

#endif  // ARIBCAPTION_B24_DECODER_HPP
