/*
 * Copyright (C) 2023 otya <otya281@gmail.com>. All rights reserved.
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

#include "renderer/open_type_gsub.hpp"
#include <cstdint>
#include <optional>
#include <vector>

static uint16_t GetUint16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>((static_cast<uint16_t>(data.at(offset)) << 8) |
                                 static_cast<uint16_t>(data.at(offset + 1)));
}

static uint32_t GetUint32(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(
        (static_cast<uint32_t>(data.at(offset)) << 24) | (static_cast<uint32_t>(data.at(offset + 1)) << 16) |
        (static_cast<uint32_t>(data.at(offset + 2)) << 8) | static_cast<uint32_t>(data.at(offset + 3)));
}

static size_t GetOffset16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<size_t>(GetUint16(data, offset));
}

static size_t GetOffset32(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<size_t>(GetUint32(data, offset));
}

static int16_t GetInt16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<int16_t>(GetUint16(data, offset));
}

static uint32_t GetTag(const std::vector<uint8_t>& data, size_t offset) {
    return FourCC(data[offset], data[offset + 1], data[offset + 2], data[offset + 3]);
}

static auto ReadCoverageTable(const std::vector<uint8_t>& gsub, size_t offset) -> std::optional<std::vector<uint16_t>> {
    if (gsub.size() < offset + 2) {
        return std::nullopt;
    }
    uint16_t coverage_format = GetUint16(gsub, offset);
    if (coverage_format == 1) {
        // Coverage Format 1:
        // uint16       coverageFormat
        // uint16       glyphCount
        // uint16       glyphArray[glyphCount]
        if (gsub.size() < offset + 4) {
            return std::nullopt;
        }
        std::vector<uint16_t> coverage{};
        size_t glyph_count = GetUint16(gsub, offset + 2);
        size_t glyph_array_offset = offset + 4;
        for (size_t coverage_index = 0; coverage_index < glyph_count; coverage_index++) {
            if (gsub.size() < glyph_array_offset + coverage_index * 2 + 2) {
                return std::nullopt;
            }
            uint16_t glyph_id = GetUint16(gsub, glyph_array_offset + coverage_index * 2);
            coverage.push_back(glyph_id);
        }
        return std::make_optional(std::move(coverage));
    } else if (coverage_format == 2) {
        // Coverage Format 2:
        // uint16       coverageFormat
        // uint16       rangeCount
        // RangeRecord  rangeRecords[rangeCount]
        //
        // RangeRecord:
        // uint16       startGlyphID
        // uint16       endGlyphID
        // uint16       startCoverageIndex
        if (gsub.size() < offset + 4) {
            return std::nullopt;
        }
        std::vector<uint16_t> coverage{};
        size_t range_count = GetUint16(gsub, offset + 2);
        size_t range_records_offset = offset + 4;
        uint32_t coverage_index = 0;
        for (size_t range_index = 0; range_index < range_count; range_index++) {
            constexpr size_t kRangeRecordSize = 6;
            if (gsub.size() < range_records_offset + range_index * kRangeRecordSize + kRangeRecordSize) {
                return std::nullopt;
            }
            uint16_t start_glyph_id = GetUint16(gsub, range_records_offset + range_index * kRangeRecordSize);
            uint16_t end_glyph_id = GetUint16(gsub, range_records_offset + range_index * kRangeRecordSize + 2);
            uint16_t start_coverage_index = GetUint16(gsub, range_records_offset + range_index * kRangeRecordSize + 4);
            if (start_glyph_id > end_glyph_id || start_coverage_index != coverage_index) {
                return std::nullopt;
            }
            coverage_index += end_glyph_id - start_glyph_id;
            coverage_index += 1;
            for (uint16_t glyph_id = start_glyph_id; glyph_id <= end_glyph_id; glyph_id++) {
                coverage.push_back(glyph_id);
            }
        }
        return std::make_optional(std::move(coverage));
    }
    return std::nullopt;
}

static auto ReadScriptFeatureIndices(const std::vector<uint8_t>& gsub,
                                     size_t script_list_offset,
                                     uint32_t required_script_tag,
                                     uint32_t required_lang_sys_tag) -> std::vector<uint16_t> {
    std::vector<uint16_t> feature_indices{};
    if (gsub.size() < script_list_offset + 2) {
        return {};
    }
    size_t script_count = GetUint16(gsub, script_list_offset);
    size_t script_records_offset = script_list_offset + 2;
    // ScriptList table:
    // uint16           scriptCount
    // ScriptRecord     scriptRecords[scriptCount]
    //
    // ScriptRecord:
    // Tag              scriptTag
    // Offset16         scriptOffset
    //
    // Script table:
    // Offset16         defaultLangSysOffset
    // uint16           langSysCount
    // LangSysRecord    langSysRecords[langSysCount]
    //
    // LangSysRecord:
    // Tag              langSysTag
    // Offset16         langSysOffset
    //
    // LangSys table:
    // Offset16 lookupOrderOffset
    // uint16   requiredFeatureIndex
    // uint16   featureIndexCount
    // uint16   featureIndices[featureIndexCount]
    for (size_t script_index = 0; script_index < script_count; script_index++) {
        constexpr size_t kScriptRecordSize = 6;
        size_t script_record_offset = script_records_offset + script_index * kScriptRecordSize;
        if (gsub.size() < script_record_offset + kScriptRecordSize) {
            return {};
        }
        uint32_t script_tag = GetTag(gsub, script_record_offset);
        if (script_tag != required_script_tag) {
            continue;
        }
        size_t script_offset = script_list_offset + GetOffset16(gsub, script_record_offset + 4);
        if (gsub.size() < script_offset + 4) {
            return {};
        }
        size_t default_lang_sys_offset = script_offset + GetOffset16(gsub, script_offset);
        size_t lang_sys_count = GetUint16(gsub, script_offset + 2);
        size_t lang_sys_offset = default_lang_sys_offset;
        size_t lang_sys_records_offset = script_offset + 4;
        for (size_t lang_sys_index = 0; lang_sys_index < lang_sys_count; lang_sys_index++) {
            constexpr size_t kLangSysRecordSize = 6;
            size_t lang_sys_record_offset = lang_sys_records_offset + lang_sys_index * kLangSysRecordSize;
            if (gsub.size() < lang_sys_record_offset + kLangSysRecordSize) {
                return {};
            }
            uint32_t lang_sys_tag = GetTag(gsub, lang_sys_record_offset);
            if (lang_sys_tag == required_lang_sys_tag) {
                lang_sys_offset = script_offset + GetUint16(gsub, lang_sys_record_offset + 4);
                break;
            }
        }
        if (lang_sys_offset == script_offset) {
            continue;
        }
        if (gsub.size() < lang_sys_offset + 6) {
            return {};
        }
        uint16_t required_feature_index = GetUint16(gsub, lang_sys_offset + 2);
        if (required_feature_index != 0xffff) {
            feature_indices.push_back(required_feature_index);
        }
        uint16_t feature_index_count = GetUint16(gsub, lang_sys_offset + 4);
        size_t feature_indices_offset = lang_sys_offset + 6;
        for (size_t i = 0; i < feature_index_count; i++) {
            if (gsub.size() < feature_indices_offset + i * 2 + 2) {
                return {};
            }
            uint16_t feature_index = GetUint16(gsub, feature_indices_offset + i * 2);
            feature_indices.push_back(feature_index);
        }
        break;
    }
    return feature_indices;
}

auto LoadSingleGSUBTable(const std::vector<uint8_t>& gsub,
                         uint32_t required_feature_tag,
                         uint32_t script_tag,
                         uint32_t lang_sys_tag) -> std::unordered_map<uint32_t, uint32_t> {
    std::unordered_map<uint32_t, uint32_t> subst_map{};
    size_t gsub_size = gsub.size();
    // GSUB Header:
    // uint16           majorVersion
    // uint16           minorVersion
    // Offset16         scriptListOffset
    // Offset16         featureListOffset
    // Offset16         lookupListOffset
    // Offset16         featureVariationsOffset if majorVersion = 1 and minorVersion = 1
    if (gsub_size < 10) {
        return {};
    }
    size_t script_list_offset = GetOffset16(gsub, 4);
    auto feature_indices = ReadScriptFeatureIndices(gsub, script_list_offset, script_tag, lang_sys_tag);
    // FeatureList table:
    // uint16           featureCount
    // FeatureRecord    featureRecords[featureCount]
    //
    // FeatureRecord:
    // Tag              featureTag
    // Offset16         featureOffset
    //
    // LookupList table:
    // uint16           lookupCount
    // Offset16         lookupOffsets[lookupCount]
    size_t feature_list_offset = GetOffset16(gsub, 6);
    size_t lookup_list_offset = GetOffset16(gsub, 8);
    if (gsub_size < lookup_list_offset + 2) {
        return {};
    }
    uint16_t lookup_count = GetUint16(gsub, lookup_list_offset);
    size_t lookup_offsets_offset = lookup_list_offset + 2;
    if (gsub_size < feature_list_offset + 2) {
        return {};
    }
    uint16_t feature_count = GetUint16(gsub, feature_list_offset);
    size_t feature_records_offset = feature_list_offset + 2;
    for (auto feature_index : feature_indices) {
        constexpr size_t kFeatureRecordSize = 6;
        size_t feature_record_offset = feature_records_offset + feature_index * kFeatureRecordSize;
        if (feature_index >= feature_count || gsub_size < feature_record_offset + kFeatureRecordSize) {
            return {};
        }
        uint32_t feature_tag = GetTag(gsub, feature_record_offset);
        if (feature_tag != required_feature_tag) {
            continue;
        }
        size_t feature_offset = feature_list_offset + GetOffset16(gsub, feature_record_offset + 4);
        // Feature table:
        // Offset16     featureParamsOffset
        // uint16       lookupIndexCount
        // uint16       lookupListIndices[lookupIndexCount]
        if (gsub_size < feature_offset + 4) {
            return {};
        }
        size_t feature_params_offset = GetOffset16(gsub, feature_offset);
        if (feature_params_offset != 0) {
            // FeatureParams tables are defined only for 'cv01'-'cv99', 'size', and 'ss01'-'ss20'.
            return {};
        }
        uint16_t lookup_index_count = GetUint16(gsub, feature_offset + 2);
        size_t lookup_list_indices_offset = feature_offset + 4;
        for (size_t lookup_index = 0; lookup_index < lookup_index_count; lookup_index++) {
            if (gsub_size < lookup_list_indices_offset + lookup_index * 2 + 2) {
                return {};
            }
            size_t lookup_list_index = GetUint16(gsub, lookup_list_indices_offset + lookup_index * 2);
            if (lookup_list_index >= feature_count || gsub_size < lookup_offsets_offset + lookup_list_index * 2 + 2) {
                return {};
            }
            size_t lookup_offset =
                lookup_list_offset + GetOffset16(gsub, lookup_offsets_offset + lookup_list_index * 2);
            if (gsub_size < lookup_offset + 6) {
                return {};
            }
            // Lookup table:
            // uint16           lookupType
            // uint16           lookupFlag
            // uint16           subTableCount
            // Offset16         subtableOffsets[subTableCount]
            // uint16           markFilteringSet if lookupFlag & USE_MARK_FILTERING_SET
            uint16_t lookup_type = GetUint16(gsub, lookup_offset);
            uint16_t lookup_flag = GetUint16(gsub, lookup_offset + 2);
            size_t subtable_count = GetUint16(gsub, lookup_offset + 4);
            bool is_extension = lookup_type == 7;
            size_t subtable_offsets_offset = lookup_offset + 6;
            for (size_t subtable_index = 0; subtable_index < subtable_count; subtable_index++) {
                if (gsub_size < subtable_offsets_offset + subtable_index * 2 + 2) {
                    return {};
                }
                size_t subtable_offset =
                    lookup_offset + GetOffset16(gsub, subtable_offsets_offset + subtable_index * 2);
                if (gsub_size < subtable_offset + 2) {
                    return {};
                }
                uint16_t subst_format = GetUint16(gsub, subtable_offset);
                if (is_extension) {
                    // Extension Substitution Subtable Format 1:
                    // uint16       substFormat
                    // uint16       extensionLookupType
                    // Offset32     extensionOffset
                    if (subst_format != 1) {
                        continue;
                    }
                    if (gsub_size < subtable_offset + 8) {
                        return {};
                    }
                    uint16_t extension_lookup_type = GetUint16(gsub, subtable_offset + 2);
                    size_t extension_offset = GetOffset32(gsub, subtable_offset + 4);
                    lookup_type = extension_lookup_type;
                    subtable_offset += extension_offset;
                    if (gsub_size < subtable_offset + 2) {
                        return {};
                    }
                    subst_format = GetUint16(gsub, subtable_offset);
                }
                if (lookup_type == 1) {
                    // LookupType 1: Single Substitution Subtable
                    if (gsub_size < subtable_offset + 4) {
                        return {};
                    }
                    size_t coverage_offset = subtable_offset + GetOffset16(gsub, subtable_offset + 2);
                    auto coverage = ReadCoverageTable(gsub, coverage_offset);
                    if (!coverage) {
                        return {};
                    }
                    if (subst_format == 1) {
                        // Single Substitution Format 1:
                        // uint16   substFormat
                        // Offset16 coverageOffset
                        // int16    deltaGlyphID
                        if (gsub_size < subtable_offset + 6) {
                            return {};
                        }
                        int16_t delta_glyph_id = GetInt16(gsub, subtable_offset + 4);
                        for (auto&& glyph_id : coverage.value()) {
                            subst_map[static_cast<uint32_t>(glyph_id)] =
                                static_cast<uint32_t>(static_cast<uint16_t>(glyph_id + delta_glyph_id));
                        }
                    } else if (subst_format == 2) {
                        // Single Substitution Format 2:
                        // uint16   substFormat
                        // Offset16 coverageOffset
                        // uint16   glyphCount
                        // uint16   substituteGlyphIDs[glyphCount]
                        uint16_t glyph_count = GetUint16(gsub, subtable_offset + 4);
                        if (gsub_size < subtable_offset + 6) {
                            return {};
                        }
                        size_t substitute_glyph_ids_offset = subtable_offset + 6;
                        for (size_t coverage_index = 0; coverage_index < glyph_count; coverage_index++) {
                            if (gsub_size < substitute_glyph_ids_offset + coverage_index * 2 + 2) {
                                return {};
                            }
                            uint16_t substitute_glyph_id =
                                GetUint16(gsub, substitute_glyph_ids_offset + coverage_index * 2);
                            if (coverage->size() <= coverage_index) {
                                return {};
                            }
                            subst_map[static_cast<uint32_t>(coverage.value()[coverage_index])] =
                                static_cast<uint32_t>(substitute_glyph_id);
                        }
                    }
                }
            }
        }
        break;
    }
    return subst_map;
}
