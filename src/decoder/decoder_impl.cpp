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

#include <cassert>
#include <cstring>
#include <cmath>
#include "base/logger.hpp"
#include "base/md5_helper.hpp"
#include "base/utf_helper.hpp"
#include "decoder/b24_codesets.hpp"
#include "decoder/b24_colors.hpp"
#include "decoder/b24_controlsets.hpp"
#include "decoder/b24_conv_tables.hpp"
#include "decoder/b24_drcs_conv.hpp"
#include "decoder/b24_gaiji_table.hpp"
#include "decoder/b24_macros.hpp"
#include "decoder/decoder_impl.hpp"

namespace aribcaption::internal {

DecoderImpl::DecoderImpl(Context& context) : log_(GetContextLogger(context)) {}

DecoderImpl::~DecoderImpl() = default;

bool DecoderImpl::Initialize(EncodingScheme encoding_scheme,
                             CaptionType type, Profile profile, LanguageId language_id) {
    request_encoding_ = encoding_scheme;
    if (request_encoding_ != EncodingScheme::kAuto) {
        active_encoding_ = encoding_scheme;
    }
    type_ = type;
    profile_ = profile;
    language_id_ = language_id;
    ResetInternalState();
    return true;
}

void DecoderImpl::SetEncodingScheme(EncodingScheme encoding_scheme) {
    request_encoding_ = encoding_scheme;

    if (encoding_scheme == EncodingScheme::kAuto) {
        EncodingScheme detected_encoding = DetectEncodingScheme();
        if (active_encoding_ != detected_encoding) {
            active_encoding_ = detected_encoding;
            ResetInternalState();
        }
    } else {  // encoding_scheme != kAuto
        if (active_encoding_ != encoding_scheme) {
            active_encoding_ = encoding_scheme;
            ResetInternalState();
        }
    }
};

void DecoderImpl::SetProfile(Profile profile) {
    profile_ = profile;
    ResetWritingFormat();
}

void DecoderImpl::SwitchLanguage(LanguageId language_id) {
    if (language_id_ != language_id) {
        language_id_ = language_id;
        current_iso6392_language_code_ = QueryISO6392LanguageCode(language_id);
    }
}

void DecoderImpl::SetReplaceMSZFullWidthAlphanumeric(bool replace) {
    replace_msz_fullwidth_ascii_ = replace;
}

uint32_t DecoderImpl::QueryISO6392LanguageCode(LanguageId language_id) const {
    if (language_infos_.empty()) {
        return current_iso6392_language_code_;
    }

    size_t index = static_cast<size_t>(language_id) - 1;
    if (index >= language_infos_.size()) {
        // Language not exist
        return 0;
    }

    const LanguageInfo& info = language_infos_[index];
    return info.iso6392_language_code;
}

DecodeStatus DecoderImpl::Decode(const uint8_t* pes_data, size_t length, int64_t pts, DecodeResult& out_result) {
    if (pes_data == nullptr) {
        log_->e("DecoderImpl: pes_data is nullptr");
        assert(pes_data != nullptr);
        return DecodeStatus::kError;
    }

    if (length < 3) {
        log_->e("DecoderImpl: pes_data size < 3, cannot parse");
        return DecodeStatus::kError;
    }

    out_result.caption.reset();
    pts_ = pts;
    const uint8_t* data = pes_data;

    uint8_t data_identifier = data[0];
    uint8_t private_stream_id = data[1];
    size_t PES_data_packet_header_length = data[2] & 0x0F;

    if (data_identifier != 0x80 && data_identifier != 0x81) {
        log_->e("DecoderImpl: Invalid data_identifier: 0x%02X", data_identifier);
        return DecodeStatus::kError;
    } else if (data_identifier != static_cast<uint8_t>(type_)) {
        log_->e("DecoderImpl: data_identifier mismatch, found: 0x%02X, expected: 0x%02X",
                data_identifier,
                static_cast<uint8_t>(type_));
        return DecodeStatus::kError;
    }

    if (private_stream_id != 0xFF) {
        log_->e("DecoderImpl: Invalid private_stream_id: 0x%02X", private_stream_id);
        return DecodeStatus::kError;
    }

    size_t data_group_begin = 3 + PES_data_packet_header_length;
    if (data_group_begin + 5 > length) {
        log_->e("DecoderImpl: pes_data length does not enough for a whole data_group");
        return DecodeStatus::kError;
    }

    uint8_t data_group_id = (data[data_group_begin] & 0b11111100) >> 2;
    size_t data_group_size = ((size_t)data[data_group_begin + 3] << 8) |
                             ((size_t)data[data_group_begin + 4] << 0);

    if (data_group_size == 0) {
        return DecodeStatus::kNoCaption;
    }

    uint8_t dgi_id = data_group_id & 0x0F;
    int dgi_group = (data_group_id & 0xF0) >> 8;

    bool ret = false;

    caption_ = std::make_unique<Caption>();

    if (dgi_id == 0) {
        // Caption management data
        if (dgi_group == prev_dgi_group_) {
            /* According to ARIB TR-B14 4.2.4
             * For caption management data, if data_group_id group equals to previous management data's group
             * This packet could be considered as retransmission, ignore it
             */
            return DecodeStatus::kNoCaption;
        } else {
            // Handle caption management data
            prev_dgi_group_ = dgi_group;
            ret = ParseCaptionManagementData(data + data_group_begin + 5, data_group_size);
        }
    } else {
        // Caption statement data
        if (dgi_id != static_cast<uint8_t>(language_id_)) {
            // Non-expected language id, ignore it
            return DecodeStatus::kNoCaption;
        } else {
            // Handle caption statement data
            ret = ParseCaptionStatementData(data + data_group_begin + 5, data_group_size);
        }
    }

    if (!ret) {
        caption_.reset();
        return DecodeStatus::kError;
    }

    if (!caption_->regions.empty() || caption_->flags) {
        caption_->type = static_cast<CaptionType>(type_);
        caption_->iso6392_language_code = current_iso6392_language_code_;
        caption_->plane_width = caption_plane_width_;
        caption_->plane_height = caption_plane_height_;
        caption_->has_builtin_sound = has_builtin_sound_;
        caption_->builtin_sound_id = builtin_sound_id_;

        caption_->pts = pts_;

        if (caption_->wait_duration == 0) {
            caption_->wait_duration = DURATION_INDEFINITE;
        }

        out_result.caption = std::move(caption_);
        return DecodeStatus::kGotCaption;
    }

    return DecodeStatus::kNoCaption;
}

void DecoderImpl::Flush() {
    ResetInternalState();
}

auto DecoderImpl::DetectEncodingScheme() -> EncodingScheme {
    EncodingScheme encoding_scheme = EncodingScheme::kARIB_STD_B24_JIS;
    bool has_jpn = false, has_latin = false, has_eng = false, has_tgl = false;

    for (const auto& info : language_infos_) {
        if (info.iso6392_language_code == ThreeCC("jpn")) {
            has_jpn = true;
        } else if (info.iso6392_language_code == ThreeCC("por") || info.iso6392_language_code == ThreeCC("spa")) {
            has_latin = true;
        } else if (info.iso6392_language_code == ThreeCC("eng")) {
            has_eng = true;
        } else if (info.iso6392_language_code == ThreeCC("tgl")) {
            has_tgl = true;
        }
    }

    if (has_jpn) {
        encoding_scheme = EncodingScheme::kARIB_STD_B24_JIS;
    } else if (has_latin) {
        encoding_scheme = EncodingScheme::kABNT_NBR_15606_1_Latin;
    } else if (has_eng || has_tgl) {
        encoding_scheme = EncodingScheme::kARIB_STD_B24_UTF8;  // Same as kISDB_T_Philippines_UTF8
    }

    return encoding_scheme;
}

void DecoderImpl::ResetGraphicSets() {
    // Set default G1~G4 codesets
    if (active_encoding_ == EncodingScheme::kABNT_NBR_15606_1_Latin) {
        // Latin language, defined in ABNT NBR 15606-1
        GX_[0] = kAlphanumericEntry;
        GX_[1] = kAlphanumericEntry;
        GX_[2] = kLatinExtensionEntry;
        GX_[3] = kLatinSpecialEntry;
    } else if (profile_ == Profile::kProfileA) {
        // full-seg, Profile A
        GX_[0] = kKanjiEntry;
        GX_[1] = kAlphanumericEntry;
        GX_[2] = kHiraganaEntry;
        GX_[3] = kMacroEntry;
    } else if (profile_ == Profile::kProfileC) {
        // one-seg, Profile C
        GX_[0] = kDRCS1Entry;
        GX_[1] = kAlphanumericEntry;
        GX_[2] = kKanjiEntry;
        GX_[3] = kMacroEntry;
    }
    GL_ = &GX_[0];
    GR_ = &GX_[2];
}

void DecoderImpl::ResetWritingFormat() {
    if (profile_ == Profile::kProfileA) {
        switch (swf_) {
            case 5:   // 1920 x 1080 horizontal
                caption_plane_width_ = display_area_width_ = 1920;
                caption_plane_height_ = display_area_height_ = 1080;
                char_width_ = 36;
                char_height_ = 36;
                char_horizontal_spacing_ = 4;
                char_vertical_spacing_ = 24;
                break;
            case 8:   // 960 x 540 vertical
                caption_plane_width_ = display_area_width_ = 960;
                caption_plane_height_ = display_area_height_ = 540;
                char_width_ = 36;
                char_height_ = 36;
                char_horizontal_spacing_ = 12;
                char_vertical_spacing_ = 24;
                break;
            case 9:   // 720 x 480 horizontal
                caption_plane_width_ = display_area_width_ = 720;
                caption_plane_height_ = display_area_height_ = 480;
                char_width_ = 36;
                char_height_ = 36;
                char_horizontal_spacing_ = 4;
                char_vertical_spacing_ = 16;
                break;
            case 10:  // 720 x 480 vertical
                caption_plane_width_ = display_area_width_ = 720;
                caption_plane_height_ = display_area_height_ = 480;
                char_width_ = 36;
                char_height_ = 36;
                char_horizontal_spacing_ = 8;
                char_vertical_spacing_ = 24;
                break;
            case 7:   // 960 x 540 horizontal
            default:
                caption_plane_width_ = display_area_width_ = 960;
                caption_plane_height_ = display_area_height_ = 540;
                char_width_ = 36;
                char_height_ = 36;
                char_horizontal_spacing_ = 4;
                char_vertical_spacing_ = 24;
                break;
        }
    } else if (profile_ == Profile::kProfileC) {
        caption_plane_width_ = display_area_width_ = 320;
        caption_plane_height_ = display_area_height_ = 180;
        char_width_ = 18;
        char_height_ = 18;
        char_horizontal_spacing_ = 2;
        char_vertical_spacing_ = 6;
    }

    if (active_encoding_ == EncodingScheme::kABNT_NBR_15606_1_Latin) {
        char_horizontal_spacing_ = 2;
        char_vertical_spacing_ = 16;
    }
}

void DecoderImpl::ResetInternalState() {
    ResetGraphicSets();
    ResetWritingFormat();

    display_area_start_x_ = 0;
    display_area_start_y_ = 0;
    active_pos_inited_ = false;
    active_pos_x_ = 0;
    active_pos_y_ = 0;

    if (active_encoding_ == EncodingScheme::kABNT_NBR_15606_1_Latin) {
        // Latin language: Use 1/2 x 1 middle size (MSZ) as default
        char_horizontal_scale_ = 0.5f;
        char_vertical_scale_ = 1.0f;
    } else {
        // Japanese: Use normal size (NSZ) as default
        char_horizontal_scale_ = 1.0f;
        char_vertical_scale_ = 1.0f;
    }

    has_underline_ = false;
    has_bold_ = false;
    has_italic_ = false;
    has_stroke_ = false;
    stroke_color_ = ColorRGBA();
    enclosure_style_ = EnclosureStyle::kEnclosureStyleNone;

    has_builtin_sound_ = false;
    builtin_sound_id_ = 0;

    palette_ = 0;
    text_color_ = kB24ColorCLUT[palette_][7];
    back_color_ = kB24ColorCLUT[palette_][8];
}

bool DecoderImpl::ParseCaptionManagementData(const uint8_t* data, size_t length) {
    if (length < 10) {
        log_->e("DecoderImpl: Data not enough for parsing CaptionManagementData");
        return false;
    }

    uint8_t TMD = (data[0] & 0b11000000) >> 6;
    size_t offset = 1;

    if (TMD == 0b10) {
        offset += 5; // Skip OTM
    }

    uint8_t num_languages = data[offset];
    offset += 1;

    if (num_languages == 0 || num_languages > 2) {
        log_->e("DecoderImpl: Invalid num_languages: %u, maximum: 2", num_languages);
        return false;
    }
    language_infos_.resize(num_languages);

    for (uint8_t i = 0; i < num_languages; i++) {
        if (offset + 5 > length) {
            log_->e("DecoderImpl: Data not enough for parsing language specific info in CaptionManagementData");
            return false;
        }

        LanguageInfo language_info;
        uint32_t language_tag = ((data[offset] & 0b11100000) >> 5);
        language_info.language_id = static_cast<LanguageId>(language_tag + 1);
        uint8_t DMF = language_info.DMF = data[offset] & 0b00001111;
        offset += 1;

        if (DMF == 0b1100 || DMF == 0b1101 || DMF == 0b1110) {
            offset += 1;
        }

        language_info.iso6392_language_code = ((uint32_t)data[offset + 0] << 16) |
                                              ((uint32_t)data[offset + 1] <<  8) |
                                              ((uint32_t)data[offset + 2] <<  0);
        offset += 3;
        language_info.format = (data[offset] & 0b11110000) >> 4;
        language_info.TCS = (data[offset] & 0b00001100) >> 2;
        offset += 1;

        if (language_info.language_id == this->language_id_) {
            current_iso6392_language_code_ = language_info.iso6392_language_code;
            swf_ = language_info.format - 1;
            ResetGraphicSets();
            ResetWritingFormat();
        }

        language_infos_[language_tag] = language_info;
    }

    if (request_encoding_ == EncodingScheme::kAuto) {
        // Determine encoding scheme by languages exist in caption management data
        EncodingScheme detected_encoding = DetectEncodingScheme();
        if (active_encoding_ != detected_encoding) {
            active_encoding_ = detected_encoding;
            ResetInternalState();
        }
    }

    if (offset + 3 > length) {
        log_->e("DecoderImpl: Data not enough for parsing CaptionManagementData");
        return false;
    }

    size_t data_unit_loop_length = ((size_t)data[offset + 0] << 16) |
                                   ((size_t)data[offset + 1] <<  8) |
                                   ((size_t)data[offset + 2] <<  0);
    offset += 3;

    if (data_unit_loop_length == 0) {
        return true;
    } else if (offset + data_unit_loop_length > length) {
        log_->e("DecoderImpl: Data not enough for parsing CaptionManagementData");
        return false;
    }

    bool ret = ParseDataUnit(data + offset, data_unit_loop_length);
    return ret;
}

bool DecoderImpl::ParseCaptionStatementData(const uint8_t* data, size_t length) {
    if (length < 4) {
        log_->e("DecoderImpl: Data not enough for parsing CaptionStatementData");
        return false;
    }

    uint8_t TMD = (data[0] & 0b11000000) >> 6;
    size_t offset = 1;

    if (TMD == 0b01 || TMD == 0b10) {
        offset += 5;
    }

    if (offset + 4 > length) {
        log_->e("DecoderImpl: Data not enough for parsing CaptionStatementData");
        return false;
    }

    size_t data_unit_loop_length = ((size_t)data[offset + 0] << 16) |
                                   ((size_t)data[offset + 1] <<  8) |
                                   ((size_t)data[offset + 2] <<  0);
    offset += 3;

    if (data_unit_loop_length == 0) {
        return true;
    } else if (offset + data_unit_loop_length > length) {
        log_->e("DecoderImpl: Data not enough for parsing CaptionStatementData");
        return false;
    }

    bool ret = ParseDataUnit(data + offset, data_unit_loop_length);
    return ret;
}

bool DecoderImpl::ParseDataUnit(const uint8_t* data, size_t length) {
    if (length < 5) {
        log_->e("DecoderImpl: Data not enough for parsing DataUnit");
        return false;
    }

    size_t offset = 0;

    while (offset < length) {
        uint8_t unit_separator = data[offset];
        uint8_t data_unit_parameter = data[offset + 1];
        size_t data_unit_size = ((size_t)data[offset + 2] << 16) |
                                ((size_t)data[offset + 3] <<  8) |
                                ((size_t)data[offset + 4] <<  0);

        if (unit_separator != 0x1F) {
            log_->e("DecoderImpl: Invalid unit_separator: 0x%02X", unit_separator);
            return false;
        }

        if (data_unit_size == 0) {
            return true;
        } else if (offset + 5 + data_unit_size > length) {
            log_->e("DecoderImpl: Data not enough for parsing DataUnit");
            return false;
        }

        if (data_unit_parameter == 0x20) {
            ParseStatementBody(data + offset + 5, data_unit_size);
        } else if (data_unit_parameter == 0x30) {
            ParseDRCS(data + offset + 5, data_unit_size, 1);
        } else if (data_unit_parameter == 0x31) {
            ParseDRCS(data + offset + 5, data_unit_size, 2);
        }

        offset += 5 + data_unit_size;
    }

    return true;
}

bool DecoderImpl::ParseStatementBody(const uint8_t* data, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        uint8_t ch = data[offset];
        bool ret = false;
        size_t bytes_processed = 0;

        if (active_encoding_ == EncodingScheme::kARIB_STD_B24_UTF8) {
            if (ch <= 0x1F) {
                ret = HandleC0(data + offset, length - offset, &bytes_processed);
            } else if (ch == 0x7F) {
                ret = HandleC1(data + offset, length - offset, &bytes_processed);
            } else if (ch == 0xC2) {
                if (offset + 1 < length && data[offset + 1] >= 0x80 && data[offset + 1] <= 0x9F) {
                    ret = HandleC1(data + offset + 1, length - offset - 1, &bytes_processed);
                    bytes_processed += 1;
                } else {
                    ret = HandleUTF8(data + offset, length - offset, &bytes_processed);
                }
            } else {
                ret = HandleUTF8(data + offset, length - offset, &bytes_processed);
            }
        } else {
            if (ch <= 0x20) {
                ret = HandleC0(data + offset, length - offset, &bytes_processed);
            } else if (ch < 0x7F) {
                ret = HandleGLGR(data + offset, length - offset, &bytes_processed, GL_);
            } else if (ch <= 0xA0) {
                ret = HandleC1(data + offset, length - offset, &bytes_processed);
            } else if (ch < 0xFF) {
                ret = HandleGLGR(data + offset, length - offset, &bytes_processed, GR_);
            }
        }

        if (!ret) {
            log_->e("DecoderImpl: Parse character 0x%02X failed near 0x%04zX", data[offset], offset);
            return false;
        }
        offset += bytes_processed;
    }

    return true;
}

bool DecoderImpl::ParseDRCS(const uint8_t* data, size_t length, size_t byte_count) {
    size_t offset = 0;
    uint8_t number_of_code = data[offset];
    offset += 1;

    for (uint8_t i = 0; i < number_of_code; i++) {
        if (offset + 3 > length) {
            log_->e("DecoderImpl: Data not enough for parsing DRCS");
            return false;
        }

        uint16_t character_code = ((uint16_t)data[offset] << 8) | data[offset + 1];
        uint8_t number_of_font = data[offset + 2];
        offset += 3;

        for (uint8_t j = 0; j < number_of_font; j++) {
            if (offset + 4 > length) {
                log_->e("DecoderImpl: Data not enough for parsing DRCS");
                return false;
            }

            [[maybe_unused]] uint8_t font_id = (data[offset] & 0xF0) >> 4;
            uint8_t mode = data[offset] & 0x0F;
            offset += 1;

            if (mode == 0b0000 || mode == 0b0001) {
                uint8_t depth = data[offset] + 2;
                uint8_t width = data[offset + 1];
                uint8_t height = data[offset + 2];
                offset += 3;

                uint8_t depth_bits = ([](uint8_t n) -> uint8_t {
                    uint8_t count = 0;
                    while (n) {
                        if ((n & 1) == 0) count++;
                        n >>= 1;
                    }
                    return count;
                })(depth);
                size_t bitmap_size = width * height * depth_bits / 8;

                if (offset + bitmap_size > length) {
                    log_->e("DecoderImpl: Data not enough for parsing DRCS");
                    return false;
                }

                DRCS drcs;
                drcs.width = static_cast<int>(width);
                drcs.height = static_cast<int>(height);
                drcs.depth = static_cast<int>(depth);
                drcs.depth_bits = static_cast<int>(depth_bits);
                drcs.pixels.resize(bitmap_size);
                memcpy(&drcs.pixels[0], &data[offset], bitmap_size);
                offset += bitmap_size;

                drcs.md5 = md5::GetDigest(&drcs.pixels[0], bitmap_size);

                // Find alternative replacement
                auto iter = kDRCSReplacementMap.find(drcs.md5);
                if (iter != kDRCSReplacementMap.end()) {
                    drcs.alternative_ucs4 = iter->second;
                    utf::UTF8AppendCodePoint(drcs.alternative_text, iter->second);
                } else {
                    log_->w("DecoderImpl: Cannot convert unrecognized DRCS pattern with MD5 %s to Unicode", drcs.md5.c_str());
                }

                if (byte_count == 1) {
                    uint8_t index = ((character_code & 0x0F00) >> 8) + 0x40;
                    uint16_t ch = (character_code & 0x00FF) & 0x7F;
                    CodesetEntry entry = kDRCSCodesetByF.at(index);
                    size_t map_index = static_cast<uint8_t>(entry.graphics_set) -
                                       static_cast<uint8_t>(GraphicSet::kDRCS_0);
                    drcs_maps_[map_index].insert_or_assign(ch, std::move(drcs));
                } else if (byte_count == 2) {
                    uint16_t ch = character_code & 0x7F7F;
                    drcs_maps_[0].insert_or_assign(ch, std::move(drcs));
                }
            } else {
                [[maybe_unused]] uint8_t region_x = data[offset];
                [[maybe_unused]] uint8_t region_y = data[offset + 1];
                size_t geometric_data_length = ((size_t)data[offset + 2] << 8) |
                                               ((size_t)data[offset + 3] << 0);
                offset += 4 + geometric_data_length;
            }
        }
    }

    return true;
}


bool DecoderImpl::HandleC0(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed) {
    size_t bytes = 0;

    switch (data[0]) {
        case C0::NUL:  // Null
        case C0::BEL:  // Bell
            bytes = 1;
            break;
        case C0::APB:  // Active position backward
            MoveRelativeActivePos(-1, 0);
            bytes = 1;
            break;
        case C0::APF:  // Active position forward
            MoveRelativeActivePos(1, 0);
            bytes = 1;
            break;
        case C0::APD:  // Active position down
            MoveRelativeActivePos(0, 1);
            bytes = 1;
            break;
        case C0::APU:  // Active position up
            MoveRelativeActivePos(0, -1);
            bytes = 1;
            break;
        case C0::CS: { // Clear screen
            ResetInternalState();
            caption_->flags = static_cast<CaptionFlags>(caption_->flags | CaptionFlags::kCaptionFlagsClearScreen);
            bytes = 1;
            break;
        }
        case C0::APR:  // Active position return
            utf::UTF8AppendCodePoint(caption_->text, 0x000A);  // \n
            MoveActivePosToNewline();
            bytes = 1;
            break;
        case C0::LS1:  // Locking shift 1
            GL_ = &GX_[1];
            bytes = 1;
            break;
        case C0::LS0:  // Locking shift 0
            GL_ = &GX_[0];
            bytes = 1;
            break;
        case C0::PAPF: { // Parameterized active position forward
            if (remain_bytes < 2)
                return false;
            uint8_t step = data[1] & 0b00111111;
            MoveRelativeActivePos(static_cast<int>(step), 0);
            bytes = 2;
            break;
        }
        case C0::CAN:  // Cancel
            bytes = 1;
            break;
        case C0::SS2: { // Single shift 2
            if (remain_bytes < 2)
                return false;
            size_t glgr_bytes = 0;
            if (!HandleGLGR(data + 1, remain_bytes - 1, &glgr_bytes, &GX_[2]))
                return false;
            bytes = 1 + glgr_bytes;
            break;
        }
        case C0::ESC: { // Escape
            if (remain_bytes < 2)
                return false;
            size_t esc_bytes = 0;
            if (!HandleESC(data + 1, remain_bytes - 1, &esc_bytes))
                return false;
            bytes = 1 + esc_bytes;
            break;
        }
        case C0::APS: { // Active position set
            if (remain_bytes < 3)
                return false;
            uint8_t y = data[1] & 0b00111111;
            uint8_t x = data[2] & 0b00111111;
            SetAbsoluteActivePos(static_cast<int>(x), static_cast<int>(y));
            bytes = 3;
            break;
        }
        case C0::SS3: { // Single shift 3
            if (remain_bytes < 2)
                return false;
            size_t glgr_bytes = 0;
            if (!HandleGLGR(data + 1, remain_bytes - 1, &glgr_bytes, &GX_[3]))
                return false;
            bytes = 1 + glgr_bytes;
            break;
        }
        case C0::RS:   // Record separator
        case C0::US:   // Unit separator
            bytes = 1;
            break;
        case C0::SP:   // Space character
            if (active_encoding_ == EncodingScheme::kABNT_NBR_15606_1_Latin ||
                    active_encoding_ == EncodingScheme::kARIB_STD_B24_UTF8) {
                PushCharacter(0x0020);  // Space (Basic Latin)
            } else {
                PushCharacter(0x3000);  // Ideographic Space (CJK)
            }
            MoveRelativeActivePos(1, 0);
            bytes = 1;
            break;
        default:
            bytes = 1;
            break;
    }

    *bytes_processed = bytes;
    return true;
}

bool DecoderImpl::HandleESC(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed) {
    size_t bytes = 0;

    switch (data[0]) {
        case ESC::LS2:
            GL_ = &GX_[2];
            bytes = 1;
            break;
        case ESC::LS3:
            GL_ = &GX_[3];
            bytes = 1;
            break;
        case ESC::LS1R:
            GR_ = &GX_[1];
            bytes = 1;
            break;
        case ESC::LS2R:
            GR_ = &GX_[2];
            bytes = 1;
            break;
        case ESC::LS3R:
            GR_ = &GX_[3];
            bytes = 1;
            break;
        default:
            if (data[0] == 0x24) {  // 2-byte G set or DRCS
                if (remain_bytes < 2)
                    return false;
                if (data[1] >= 0x28 && data[1] <= 0x2B) {
                    if (remain_bytes < 3)
                        return false;
                    uint8_t GX_index = data[1] - 0x28;
                    if (data[2] == 0x20) {  // 2-byte DRCS
                        if (remain_bytes < 4)
                            return false;
                        GX_[GX_index] = kDRCSCodesetByF.at(data[3]);
                        bytes = 4;
                    } else {  // 2-byte G set
                        GX_[GX_index] = kGCodesetByF.at(data[2]);
                        bytes = 3;
                    }
                } else {  // 2-byte G set
                    GX_[0] = kGCodesetByF.at(data[1]);
                    bytes = 2;
                }
            } else if (data[0] >= 0x28 && data[0] <= 0x2B) {  // 1-byte G set or DRCS
                if (remain_bytes < 2)
                    return false;
                uint8_t GX_index = data[0] - 0x28;
                if (data[1] == 0x20) {  // 1-byte DRCS
                    if (remain_bytes < 3)
                        return false;
                    GX_[GX_index] = kDRCSCodesetByF.at(data[2]);
                    bytes = 3;
                } else {  // 1-byte G set
                    GX_[GX_index] = kGCodesetByF.at(data[1]);
                    bytes = 2;
                }
            }
            break;
    }

    *bytes_processed = bytes;
    return true;
}

bool DecoderImpl::HandleC1(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed) {
    size_t bytes = 0;

    switch (data[0]) {
        case C1::DEL:  // Delete character
            bytes = 1;
            break;
        case C1::BKF:  // Black Foreground
            text_color_ = kB24ColorCLUT[palette_][0];
            bytes = 1;
            break;
        case C1::RDF:  // Red Foreground
            text_color_ = kB24ColorCLUT[palette_][1];
            bytes = 1;
            break;
        case C1::GRF:  // Green Foreground
            text_color_ = kB24ColorCLUT[palette_][2];
            bytes = 1;
            break;
        case C1::YLF:  // Yellow Foreground
            text_color_ = kB24ColorCLUT[palette_][3];
            bytes = 1;
            break;
        case C1::BLF:  // Blue Foreground
            text_color_ = kB24ColorCLUT[palette_][4];
            bytes = 1;
            break;
        case C1::MGF:  // Magenta Foreground
            text_color_ = kB24ColorCLUT[palette_][5];
            bytes = 1;
            break;
        case C1::CNF:  // Cyan Foreground
            text_color_ = kB24ColorCLUT[palette_][6];
            bytes = 1;
            break;
        case C1::WHF:  // White Foreground
            text_color_ = kB24ColorCLUT[palette_][7];
            bytes = 1;
            break;
        case C1::COL:  // Colour Controls
            if (remain_bytes < 2)
                return false;
            if (data[1] == 0x20) {
                if (remain_bytes < 3)
                    return false;
                palette_ = data[2] & 0x0F;
                bytes = 3;
            } else if (data[1] >= 0x48 && data[1] <= 0x7F) {
                switch (data[1] & 0xF0) {
                    case 0x40:
                        text_color_ = kB24ColorCLUT[palette_][data[1] & 0x0F];
                        break;
                    case 0x50:
                        back_color_ = kB24ColorCLUT[palette_][data[1] & 0x0F];
                        break;
                    default:
                        break;
                }
                bytes = 2;
            } else {
                return false;
            }
            break;
        case C1::POL:  // Pattern Polarity Controls
            bytes = 2;
            break;
        case C1::SSZ:  // Small Size
            char_horizontal_scale_ = 0.5f;
            char_vertical_scale_ = 0.5f;
            bytes = 1;
            break;
        case C1::MSZ:  // Middle Size
            char_horizontal_scale_ = 0.5f;
            char_vertical_scale_ = 1.0f;
            bytes = 1;
            break;
        case C1::NSZ:  // Normal Size
            char_horizontal_scale_ = 1.0f;
            char_vertical_scale_ = 1.0f;
            bytes = 1;
            break;
        case C1::SZX:  // Character Size Controls
            if (remain_bytes < 2)
                return false;
            switch (data[1]) {
                case 0x41:  // double height
                    char_vertical_scale_ = 2.0f;
                    break;
                case 0x44:  // double width
                    char_horizontal_scale_ = 2.0f;
                    break;
                case 0x45:  // double height and width
                    char_horizontal_scale_ = 2.0f;
                    char_vertical_scale_ = 2.0f;
                    break;
                default:  // Other values is unused according to ARIB TR-B14
                    break;
            }
            bytes = 2;
            break;
        case C1::FLC:  // Flashing control
            bytes = 2;
            break;
        case C1::CDC:  // Conceal Display Controls
            if (remain_bytes < 2)
                return false;
            if (data[1] == 0x20) {
                if (remain_bytes < 3)
                    return false;
                bytes = 3;
            } else {
                bytes = 2;
            }
            break;
        case C1::WMM:  // Writing Mode Modification
            bytes = 2;
            break;
        case C1::TIME:  // Time Controls
            if (remain_bytes < 2)
                return false;
            if (data[1] == 0x20) {
                uint8_t p2 = data[2] & 0b00111111;
                caption_->wait_duration += static_cast<int64_t>(p2) * 100;
                caption_->flags = static_cast<CaptionFlags>(caption_->flags | CaptionFlags::kCaptionFlagsWaitDuration);
                bytes = 3;
            } else if (data[1] == 0x28) {
                // Not used according to ARIB TR-B14
                bytes = 3;
            } else {
                // Not used according to ARIB TR-B14
                return false;
            }
            break;
        case C1::MACRO:  // Macro Command
            // Not used according to ARIB TR-B14
            return false;
        case C1::RPC:  // Repeat Character
            // TODO
            if (remain_bytes < 2)
                return false;
            bytes = 2;
            break;
        case C1::STL:  // Start Lining
            has_underline_ = true;
            bytes = 1;
            break;
        case C1::SPL:  // Stop Lining
            has_underline_ = false;
            bytes = 1;
            break;
        case C1::HLC:  // Highlighting Character Block
            if (remain_bytes < 2)
                return false;
            enclosure_style_ = static_cast<EnclosureStyle>(data[1] & 0x0F);
            bytes = 2;
            break;
        case C1::CSI: {  // Control Sequence Introducer
            size_t csi_bytes = 0;
            if (!HandleCSI(data + 1, remain_bytes - 1, &csi_bytes))
                return false;
            bytes = 1 + csi_bytes;
            break;
        }
        default:
            bytes = 1;
            break;
    }

    *bytes_processed = bytes;
    return true;
}

bool DecoderImpl::HandleCSI(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed) {
    size_t offset = 0;

    uint16_t param1 = 0;
    uint16_t param2 = 0;
    size_t param_count = 0;

    while (offset < remain_bytes) {
        if (data[offset] >= 0x30 && data[offset] <= 0x39) {
            if (param_count <= 1) {
                param2 = param2 * 10 + (data[offset] & 0x0F);
            }
        } else if (data[offset] == 0x20) {  // I2 / In or I
            if (param_count == 0) {
                param1 = param2;
            }
            param_count++;
            break;
        } else if (data[offset] == 0x3B) {  // I1
            if (param_count == 0) {
                param1 = param2;
                param2 = 0;
            }
            param_count++;
        }
        offset++;
    }

    // move to F
    if (++offset >= remain_bytes) {
        log_->e("DecoderImpl: Data not enough for handling CSI control character");
        return false;
    }

    switch (data[offset]) {
        case CSI::GSM:  // Character deformation
            break;
        case CSI::SWF:  // Set Writing Format
            if (param_count == 1) {
                swf_ = static_cast<uint8_t>(param1);
            }
            ResetWritingFormat();
            break;
        case CSI::CCC:  // Composite Character Composition
            break;
        case CSI::SDF:  // Set Display Format
            display_area_width_ = static_cast<int>(param1);
            display_area_height_ = static_cast<int>(param2);
            break;
        case CSI::SSM:  // Character composition dot designation
            char_width_ = static_cast<int>(param1);
            char_height_ = static_cast<int>(param2);
            break;
        case CSI::SHS:  // Set Horizontal Spacing
            char_horizontal_spacing_ = static_cast<int>(param1);
            break;
        case CSI::SVS:  // Set Vertical Spacing
            char_vertical_spacing_ = static_cast<int>(param1);
            break;
        case CSI::PLD:  // Partially Line Down
        case CSI::PLU:  // Partially Line Up
        case CSI::GAA:  // Colouring block
        case CSI::SRC:  // Raster Colour Designation
            break;
        case CSI::SDP: { // Set Display Position
            display_area_start_x_ = static_cast<int>(param1);
            if (param_count >= 2) {
                display_area_start_y_ = static_cast<int>(param2);
            }
            if (!active_pos_inited_) {
                // Reset active position to top left corner of display area
                // APS(0, 0)
                SetAbsoluteActivePos(0, 0);
            }
            break;
        }
        case CSI::ACPS: // Active Coordinate Position Set
            SetAbsoluteActiveCoordinateDot(static_cast<int>(param1), static_cast<int>(param2));
            break;
        case CSI::TCC:  // Switch control
            break;
        case CSI::ORN:  // Ornament Control
            if (param1 == 0) {
                has_stroke_ = false;
            } else if (param1 == 1 && param_count >= 2) {
                uint16_t p2 = param2 / 100;
                uint16_t p3 = param2 % 100;
                if (p2 >= 8 || p3 >= 16)
                    return false;
                has_stroke_ = true;
                stroke_color_ = kB24ColorCLUT[p2][p3];
            }
            break;
        case CSI::MDF:  // Font
            if (param1 == 0) {
                has_bold_ = false;
                has_italic_ = false;
            } else if (param1 == 1) {
                has_bold_ = true;
            } else if (param1 == 2) {
                has_italic_ = true;
            } else if (param1 == 3) {
                has_bold_ = true;
                has_italic_ = true;
            }
            break;
        case CSI::CFS:  // Character Font Set
        case CSI::XCS:  // External Character Set
        case CSI::SCR:
            break;
        case CSI::PRA:  // Built-in sound replay
            has_builtin_sound_ = true;
            builtin_sound_id_ = static_cast<uint8_t>(param1);
            break;
        case CSI::ACS:  // Alternative Character Set
        case CSI::UED:  // Invisible dataEmbedded control
        case CSI::RCS:  // Raster Colour command
        case CSI::SCS:  // Skip Character Set
        default:
            break;
    }

    *bytes_processed = offset + 1;
    return true;
}

bool DecoderImpl::HandleGLGR(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed, CodesetEntry* entry) {
    uint8_t ch = data[0] & 0x7F;
    if (ch < 0x21 || ch >= 0x7F) {
        return false;
    }

    uint8_t ch2 = 0;
    if (entry->bytes == 2) {
        if (remain_bytes < 2) {
            return false;
        }
        ch2 = data[1] & 0x7F;
        if (ch2 < 0x21 || ch2 >= 0x7F) {
            return false;
        }
    }

    if (entry->graphics_set == GraphicSet::kHiragana ||
               entry->graphics_set == GraphicSet::kProportionalHiragana) {
        uint32_t index = (uint32_t)ch - 0x21;
        uint32_t ucs4 = kHiraganaTable[index];
        PushCharacter(ucs4);
        MoveRelativeActivePos(1, 0);
    } else if (entry->graphics_set == GraphicSet::kKatakana ||
               entry->graphics_set == GraphicSet::kProportionalKatakana) {
        uint32_t index = (uint32_t)ch - 0x21;
        uint32_t ucs4 = kKatakanaTable[index];
        PushCharacter(ucs4);
        MoveRelativeActivePos(1, 0);
    } else if (entry->graphics_set == GraphicSet::kJIS_X0201_Katakana) {
        uint32_t index = (uint32_t)ch - 0x21;
        uint32_t ucs4 = kJISX0201KatakanaTable[index];
        PushCharacter(ucs4);
        MoveRelativeActivePos(1, 0);
    } else if (entry->graphics_set == GraphicSet::kKanji ||
               entry->graphics_set == GraphicSet::kJIS_X0213_2004_Kanji_1 ||
               entry->graphics_set == GraphicSet::kJIS_X0213_2004_Kanji_2 ||
               entry->graphics_set == GraphicSet::kAdditionalSymbols) {
        constexpr uint32_t gaiji_begin_ku = 84;
        uint32_t ku = (uint32_t)ch - 0x21;
        uint32_t ten = (uint32_t)ch2 - 0x21;

        uint32_t ucs4 = 0;
        uint32_t pua = 0;

        if (ku < gaiji_begin_ku) {
            uint32_t index = ku * 94 + ten;
            ucs4 = kKanjiTable[index];
            // If [ucs4 is Fullwidth alphanumeric] && [request replace] && [under MSZ mode]
            if ((ucs4 >= 0xFF01 && ucs4 <= 0xFF5E) && replace_msz_fullwidth_ascii_ &&
                char_horizontal_scale_ * 2 == char_vertical_scale_) {
                // Replace Fullwidth alphanumerics with Halfwidth alphanumerics
                ucs4 = (ucs4 & 0xFF) + 0x20;
            }
        } else {  // ku >= 84
            // Additional Kanji + Additional Symbols
            uint32_t index = (ku - gaiji_begin_ku) * 94 + ten;
            ucs4 = kAdditionalSymbolsTable_Unicode[index];
            pua = kAdditionalSymbolsTable_PUA[index];
            if (pua == ucs4 || pua < 0xE000 || pua > 0xF8FF) {
                // Same as ucs4, or invalid PUA
                pua = 0;  // mark as non-existent
            }
        }

        PushCharacter(ucs4, pua);
        MoveRelativeActivePos(1, 0);
    } else if (entry->graphics_set == GraphicSet::kAlphanumeric ||
               entry->graphics_set == GraphicSet::kProportionalAlphanumeric) {
        uint32_t index = (uint32_t)ch - 0x21;
        uint32_t ucs4 = 0;
        if (active_encoding_ == EncodingScheme::kABNT_NBR_15606_1_Latin) {
            ucs4 = kAlphanumericTable_Latin[index];
        } else if (replace_msz_fullwidth_ascii_ && char_horizontal_scale_ * 2 == char_vertical_scale_) {
            ucs4 = kAlphanumericTable_Halfwidth[index];
        } else {
            ucs4 = kAlphanumericTable_Fullwidth[index];
        }
        PushCharacter(ucs4);
        MoveRelativeActivePos(1, 0);
    } else if (entry->graphics_set == GraphicSet::kLatinExtension) {
        uint32_t index = (uint32_t)ch - 0x21;
        uint32_t ucs4 = kLatinExtensionTable[index];
        PushCharacter(ucs4);
        MoveRelativeActivePos(1, 0);
    } else if (entry->graphics_set == GraphicSet::kLatinSpecial) {
        uint32_t index = (uint32_t)ch - 0x21;
        uint32_t ucs4 = kLatinSpecialTable[index];
        PushCharacter(ucs4);
        MoveRelativeActivePos(1, 0);
    } else if (entry->graphics_set == GraphicSet::kMacro) {
        uint8_t key = ch;
        if (key >= 0x60 && key <= 0x6F) {
            if (!ParseStatementBody(kDefaultMacros[key & 0x0F], sizeof(kDefaultMacros[0]))) {
                return false;
            }
        }
    } else if (entry->graphics_set >= GraphicSet::kDRCS_0 &&
               entry->graphics_set <= GraphicSet::kDRCS_15) {
        uint32_t map_index = static_cast<uint32_t>(entry->graphics_set) - static_cast<uint32_t>(GraphicSet::kDRCS_0);
        auto& drcs_map = drcs_maps_[map_index];
        uint16_t key = ch;
        if (entry->bytes == 2) {
            key = (key << 8) | ch2;
        }

        auto iter = drcs_map.find(key);
        if (iter == drcs_map.end()) {
            // Unfindable DRCS character, insert Geta Mark instead
            PushCharacter(0x3013);
        } else {
            DRCS& drcs = iter->second;
            uint32_t code = (map_index << 16) | key;
            PushDRCSCharacter(code, drcs);
        }

        MoveRelativeActivePos(1, 0);
    } // else: not supported, ignore

    *bytes_processed = entry->bytes;
    return true;
}

bool DecoderImpl::HandleUTF8(const uint8_t* data, size_t remain_bytes, size_t* bytes_processed) {
    if (!remain_bytes) {
        return false;
    }

    uint32_t ucs4 = utf::DecodeUTF8ToCodePoint(data, remain_bytes, bytes_processed);
    PushCharacter(ucs4);
    MoveRelativeActivePos(1, 0);

    return true;
}

void DecoderImpl::PushCharacter(uint32_t ucs4, uint32_t pua) {
    CaptionChar caption_char;
    caption_char.type = CaptionCharType::kText;
    caption_char.codepoint = ucs4;
    caption_char.pua_codepoint = pua;

    size_t u8count = utf::UTF8AppendCodePoint(caption_char.u8str, ucs4);
    caption_char.u8str[u8count] = '\0';

    if (!IsRubyMode()) {
        utf::UTF8AppendCodePoint(caption_->text, ucs4);
    }

    ApplyCaptionCharCommonProperties(caption_char);
    PushCaptionChar(caption_char);
}

void DecoderImpl::PushDRCSCharacter(uint32_t code, DRCS& drcs) {
    CaptionChar caption_char;

    if (drcs.alternative_text.empty()) {
        caption_char.type = CaptionCharType::kDRCS;
        utf::UTF8AppendCodePoint(caption_->text, 0x3013);  // Fill a Geta Mark here
    } else {
        caption_char.type = CaptionCharType::kDRCSReplaced;
        strcpy(caption_char.u8str, drcs.alternative_text.c_str());
        caption_char.codepoint = drcs.alternative_ucs4;
        if (!IsRubyMode())
            caption_->text.append(drcs.alternative_text);
    }

    auto iter = caption_->drcs_map.find(code);
    if (iter == caption_->drcs_map.end()) {
        caption_->drcs_map.insert({code, drcs});
    }

    caption_char.drcs_code = code;

    ApplyCaptionCharCommonProperties(caption_char);
    PushCaptionChar(caption_char);
}

void DecoderImpl::PushCaptionChar(const CaptionChar& caption_char) {
    if (NeedNewCaptionRegion()) {
        MakeNewCaptionRegion();
    }
    CaptionRegion& region = caption_->regions.back();
    region.width += caption_char.section_width();
    region.chars.push_back(caption_char);
}

void DecoderImpl::ApplyCaptionCharCommonProperties(CaptionChar& caption_char) {
    caption_char.x = active_pos_x_;
    caption_char.y = active_pos_y_ - section_height();
    caption_char.char_width = char_width_;
    caption_char.char_height = char_height_;
    caption_char.char_horizontal_spacing = char_horizontal_spacing_;
    caption_char.char_vertical_spacing = char_vertical_spacing_;
    caption_char.char_horizontal_scale = char_horizontal_scale_;
    caption_char.char_vertical_scale = char_vertical_scale_;
    caption_char.text_color = text_color_;
    caption_char.back_color = back_color_;

    if (has_underline_)
        caption_char.style = static_cast<CharStyle>(caption_char.style | CharStyle::kCharStyleUnderline);
    if (has_bold_)
        caption_char.style = static_cast<CharStyle>(caption_char.style | CharStyle::kCharStyleBold);
    if (has_italic_)
        caption_char.style = static_cast<CharStyle>(caption_char.style | CharStyle::kCharStyleItalic);
    if (has_stroke_) {
        caption_char.style = static_cast<CharStyle>(caption_char.style | CharStyle::kCharStyleStroke);
        caption_char.stroke_color = stroke_color_;
    }

    caption_char.enclosure_style = enclosure_style_;
}

bool DecoderImpl::NeedNewCaptionRegion() {
    if (caption_->regions.empty()) {
        // Need new caption region
        return true;
    }

    CaptionRegion& prev_region = caption_->regions.back();
    if (prev_region.chars.empty()) {
        // Region is empty, reuse
        return false;
    }

    CaptionChar& prev_char = prev_region.chars.back();

    if (active_pos_x_ != prev_char.x + prev_char.section_width()) {
        // Expected pos_x is mismatched, new region will be needed
        return true;
    } else if (active_pos_y_ - section_height() != prev_char.y){
        // Caption Line (pos_y) is different, new region will be needed
        return true;
    } else if (section_height() != prev_char.section_height()) {
        // Section height is different, new region will be needed
        return true;
    }

    return false;
}

void DecoderImpl::MakeNewCaptionRegion() {
    if (caption_->regions.empty() || !caption_->regions.back().chars.empty()) {
        caption_->regions.emplace_back();
    }

    CaptionRegion& region = caption_->regions.back();

    region.x = active_pos_x_;
    region.y = active_pos_y_ - section_height();
    region.height = section_height();

    if (IsRubyMode()) {
        region.is_ruby = true;
    }
}

bool DecoderImpl::IsRubyMode() const {
    if (active_encoding_ != EncodingScheme::kARIB_STD_B24_JIS) {
        return false;
    }
    if ((char_horizontal_scale_ == 0.5f && char_vertical_scale_ == 0.5f) ||
            (profile_ == Profile::kProfileA && char_width_ == 18 && char_height_ == 18)) {
        return true;
    }
    return false;
}

int DecoderImpl::section_width() const {
    return (int)std::floor((float)(char_width_ + char_horizontal_spacing_) * char_horizontal_scale_);
}

int DecoderImpl::section_height() const {
    return (int)std::floor((float)(char_height_ + char_vertical_spacing_) * char_vertical_scale_);
}

void DecoderImpl::SetAbsoluteActivePos(int x, int y) {
    active_pos_inited_ = true;
    active_pos_x_ = display_area_start_x_ + x * section_width();
    active_pos_y_ = display_area_start_y_ + (y + 1) * section_height();
}

void DecoderImpl::SetAbsoluteActiveCoordinateDot(int x, int y) {
    active_pos_inited_ = true;
    active_pos_x_ = x;
    active_pos_y_ = y;
}

void DecoderImpl::MoveRelativeActivePos(int x, int y) {
    if (active_pos_x_ < 0 || active_pos_y_ < 0) {
        SetAbsoluteActivePos(0, 0);
    }

    active_pos_inited_ = true;

    while (x < 0) {
        active_pos_x_ -= section_width();
        x++;
        if (active_pos_x_ < display_area_start_x_) {
            active_pos_x_ = display_area_start_x_ + display_area_width_ - section_width();
            y--;
        }
    }

    while (x > 0) {
        active_pos_x_ += section_width();
        x--;
        if (active_pos_x_ >= display_area_start_x_ + display_area_width_) {
            active_pos_x_ = display_area_start_x_;
            y++;
        }
    }

    while (y < 0) {
        active_pos_y_ -= section_height();
        y++;
        if (active_pos_y_ < display_area_start_y_) {
            active_pos_y_ = display_area_start_y_ + display_area_height_;
        }
    }

    while (y > 0) {
        active_pos_y_ += section_height();
        y--;
        if (active_pos_y_ > display_area_start_y_ + display_area_height_) {
            active_pos_y_ = display_area_start_y_ + section_height();
        }
    }
}

void DecoderImpl::MoveActivePosToNewline() {
    if (active_pos_x_ < 0 || active_pos_y_ < 0) {
        SetAbsoluteActivePos(0, 0);
    }

    active_pos_inited_ = true;
    active_pos_x_ = display_area_start_x_;
    active_pos_y_ += section_height();
}


}  // namespace aribcaption::internal
