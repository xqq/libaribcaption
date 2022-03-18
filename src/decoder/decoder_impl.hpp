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

#ifndef ARIBCAPTION_DECODER_IMPL_HPP
#define ARIBCAPTION_DECODER_IMPL_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <unordered_map>
#include "aribcaption/caption.hpp"
#include "aribcaption/context.hpp"
#include "aribcaption/decoder.hpp"
#include "base/logger.hpp"
#include "decoder/b24_codesets.hpp"

namespace aribcaption::internal {

class DecoderImpl {
public:
    explicit DecoderImpl(Context& context);
    ~DecoderImpl();
public:
    bool Initialize(EncodingScheme encoding_scheme = EncodingScheme::kAuto,
                    CaptionType type = CaptionType::kDefault,
                    Profile profile = Profile::kDefault,
                    LanguageId language_id = LanguageId::kDefault);
    void SetEncodingScheme(EncodingScheme encoding_scheme);
    void SetCaptionType(CaptionType type) { type_ = type; }
    void SetProfile(Profile profile);
    void SwitchLanguage(LanguageId language_id);
    void SetReplaceMSZFullWidthAlphanumeric(bool replace);
    [[nodiscard]]
    uint32_t QueryISO6392LanguageCode(LanguageId language_id) const;
    DecodeStatus Decode(const uint8_t* pes_data, size_t length, int64_t pts, DecodeResult& out_result);
    void Flush();
private:
    auto DetectEncodingScheme() -> EncodingScheme;
    void ResetGraphicSets();
    void ResetWritingFormat();
    void ResetInternalState();
    bool ParseCaptionManagementData(const uint8_t* data, size_t length);
    bool ParseCaptionStatementData(const uint8_t* data, size_t length);
    bool ParseDataUnit(const uint8_t* data, size_t length);
    bool ParseStatementBody(const uint8_t* data, size_t length);
    bool ParseDRCS(const uint8_t* data, size_t length, size_t byte_count);
    bool HandleC0(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed);
    bool HandleESC(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed);
    bool HandleC1(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed);
    bool HandleCSI(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed);
    bool HandleGLGR(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed, CodesetEntry* entry);
    bool HandleUTF8(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed);
    void PushCharacter(uint32_t ucs4, uint32_t pua = 0);
    void PushDRCSCharacter(uint32_t code, DRCS& drcs);
    void PushCaptionChar(const CaptionChar& caption_char);
    void ApplyCaptionCharCommonProperties(CaptionChar& caption_char);
    bool NeedNewCaptionRegion();
    void MakeNewCaptionRegion();
    [[nodiscard]]
    bool IsRubyMode() const;
    [[nodiscard]]
    int section_width() const;
    [[nodiscard]]
    int section_height() const;
    void SetAbsoluteActivePos(int x, int y);
    void SetAbsoluteActiveCoordinateDot(int x, int y);
    void MoveRelativeActivePos(int x, int y);
    void MoveActivePosToNewline();
public:
    DecoderImpl(const DecoderImpl&) = delete;
    DecoderImpl(DecoderImpl&&) = delete;
    DecoderImpl& operator=(const DecoderImpl&) = delete;
    DecoderImpl& operator=(DecoderImpl&&) = delete;
private:
    struct LanguageInfo {
        LanguageId language_id = LanguageId::kFirst;
        uint8_t DMF = 0;
        uint8_t format = 0;
        uint8_t TCS = 0;
        uint32_t iso6392_language_code = 0;
    };
private:
    std::shared_ptr<Logger> log_;

    EncodingScheme request_encoding_ = EncodingScheme::kAuto;
    EncodingScheme active_encoding_ = EncodingScheme::kARIB_STD_B24_JIS;

    CaptionType type_ = CaptionType::kDefault;
    Profile profile_ = Profile::kDefault;
    LanguageId language_id_ = LanguageId::kDefault;

    bool replace_msz_fullwidth_ascii_ = false;

    std::vector<LanguageInfo> language_infos_;
    uint32_t current_iso6392_language_code_ = 0;
    int prev_dgi_group_ = -1;

    std::unique_ptr<Caption> caption_;

    CodesetEntry* GL_ = nullptr;
    CodesetEntry* GR_ = nullptr;
    std::array<CodesetEntry, 4> GX_ = {
        kKanjiEntry,         // G0
        kAlphanumericEntry,  // G1
        kHiraganaEntry,      // G2
        kMacroEntry          // G3
    };
    std::vector<std::unordered_map<uint16_t, DRCS>> drcs_maps_{16};

    int64_t pts_ = PTS_NOPTS;  // in milliseconds

    uint8_t swf_ = 7;

    int caption_plane_width_ = 960;  // indicated by SWF
    int caption_plane_height_ = 540;
    int display_area_width_ = 960;   // indicated by SDF
    int display_area_height_ = 540;
    int display_area_start_x_ = 0;   // indicated by SDP
    int display_area_start_y_ = 0;
    bool active_pos_inited_ = false; // Active position is inited
    int active_pos_x_ = 0;           // Active position base point x
    int active_pos_y_ = 0;           // Active position base point y (section's bottom left corner + 1 dot)

    int char_width_ = 36;            // indicated by SSM
    int char_height_ = 36;           // indicated by SSM
    int char_horizontal_spacing_ = 4;  // indicated by SHS
    int char_vertical_spacing_ = 24;   // indicated by SVS
    float char_horizontal_scale_ = 1.0f;
    float char_vertical_scale_ = 1.0f;

    bool has_underline_ = false;  // STL / SPL
    bool has_bold_ = false;       // MDF
    bool has_italic_ = false;     // MDF
    bool has_stroke_ = false;     // ORN
    ColorRGBA stroke_color_;      // ORN
    EnclosureStyle enclosure_style_ = EnclosureStyle::kEnclosureStyleDefault;  // HLC

    bool has_builtin_sound_ = false;
    uint8_t builtin_sound_id_ = 0;

    uint8_t palette_ = 0;
    ColorRGBA text_color_;
    ColorRGBA back_color_;
};

}  // namespace aribcaption::internal

#endif  // ARIBCAPTION_DECODER_IMPL_HPP
