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

#include <unistd.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include "base/language_code.hpp"
#include "base/scoped_holder.hpp"
#include "base/unicode_helper.hpp"
#include "renderer/font_provider_android.hpp"

using namespace tinyxml2;
using namespace aribcaption::internal;

inline constexpr const char* kAndroidFontsXML_LMP = "/system/etc/fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_System = "/system/etc/system_fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Fallback = "/system/etc/fallback_fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Fallback_JA = "/system/etc/fallback_fonts-ja.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Vendor = "/vendor/etc/fallback_fonts.xml";
inline constexpr const char* kAndroidFontsXML_OLD_Vendor_JA = "/vendor/etc/fallback_fonts-ja.xml";

namespace aribcaption {

FontProviderAndroid::FontProviderAndroid(Context& context) : log_(GetContextLogger(context)) {}

FontProviderType FontProviderAndroid::GetType() {
    return FontProviderType::kAndroid;
}

bool FontProviderAndroid::Initialize() {
    const char* android_root = getenv("ANDROID_ROOT");
    base_font_path_ = android_root ? android_root : "/system";
    base_font_path_.append("/fonts/");

    bool ret = ParseAndroidSystemFonts();
    return ret;
}

void FontProviderAndroid::SetLanguage(uint32_t iso6392_language_code) {
    iso6392_language_code_ = iso6392_language_code;
}

// Pick the font closest to Regular: prefer upright fonts, then the weight nearest to 400
// Families like "sans-serif-medium" (alias with weight="500") contain no weight 400 font
static FontFile* FindRegularFont(FontFamily& family) {
    FontFile* best = nullptr;
    int best_score = 0;

    for (auto& font : family.fonts) {
        int score = std::abs(font.weight - 400) + (font.is_italic ? 1000 : 0);
        if (!best || score < best_score) {
            best = &font;
            best_score = score;
        }
    }

    return best;
}

namespace {

using CodePointRange = std::pair<uint32_t, uint32_t>;

inline uint16_t ReadBE16(const uint8_t* p) {
    return static_cast<uint16_t>((static_cast<uint16_t>(p[0]) << 8) | p[1]);
}

inline uint32_t ReadBE32(const uint8_t* p) {
    return (static_cast<uint32_t>(p[0]) << 24) |
           (static_cast<uint32_t>(p[1]) << 16) |
           (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

bool ReadFileAt(FILE* fp, uint64_t offset, void* dst, size_t len) {
    if (fseek(fp, static_cast<long>(offset), SEEK_SET) != 0) {
        return false;
    }
    return fread(dst, 1, len, fp) == len;
}

void AppendRange(std::vector<CodePointRange>& ranges, uint32_t start, uint32_t end) {
    if (start <= end) {
        ranges.emplace_back(start, end);
    }
}

void ParseCmapFormat4(const uint8_t* table, size_t table_size, std::vector<CodePointRange>& ranges) {
    if (table_size < 14) {
        return;
    }
    uint16_t seg_count_x2 = ReadBE16(table + 6);
    if (seg_count_x2 < 2 || (seg_count_x2 & 1) != 0) {
        return;
    }
    size_t seg_count = seg_count_x2 / 2;
    size_t end_code_off = 14;
    size_t start_code_off = end_code_off + seg_count_x2 + 2;
    size_t id_delta_off = start_code_off + seg_count_x2;
    size_t id_range_off = id_delta_off + seg_count_x2;
    if (id_range_off + seg_count_x2 > table_size) {
        return;
    }

    for (size_t i = 0; i < seg_count; i++) {
        uint16_t end_code = ReadBE16(table + end_code_off + i * 2);
        uint16_t start_code = ReadBE16(table + start_code_off + i * 2);
        if (start_code == 0xFFFF && end_code == 0xFFFF) {
            break;
        }
        if (start_code > end_code) {
            continue;
        }
        uint16_t id_delta = ReadBE16(table + id_delta_off + i * 2);
        size_t range_entry_off = id_range_off + i * 2;
        uint16_t range_offset = ReadBE16(table + range_entry_off);

        if (range_offset == 0) {
            uint32_t zero_cp = static_cast<uint16_t>(0u - id_delta);
            if (zero_cp >= start_code && zero_cp <= end_code) {
                if (zero_cp > start_code) {
                    AppendRange(ranges, start_code, zero_cp - 1);
                }
                if (zero_cp < end_code) {
                    AppendRange(ranges, zero_cp + 1, end_code);
                }
            } else {
                AppendRange(ranges, start_code, end_code);
            }
        } else {
            bool in_run = false;
            uint32_t run_start = 0;
            for (uint32_t cp = start_code; cp <= end_code; cp++) {
                size_t glyph_off = range_entry_off + range_offset + static_cast<size_t>(cp - start_code) * 2;
                if (glyph_off + 2 > table_size) {
                    break;
                }
                uint16_t glyph_id = ReadBE16(table + glyph_off);
                bool mapped = glyph_id != 0 && static_cast<uint16_t>(glyph_id + id_delta) != 0;
                if (mapped && !in_run) {
                    run_start = cp;
                    in_run = true;
                } else if (!mapped && in_run) {
                    AppendRange(ranges, run_start, cp - 1);
                    in_run = false;
                }
            }
            if (in_run) {
                AppendRange(ranges, run_start, end_code);
            }
        }
    }
}

void ParseCmapFormat12(const uint8_t* table, size_t table_size, std::vector<CodePointRange>& ranges) {
    if (table_size < 16) {
        return;
    }
    uint32_t num_groups = ReadBE32(table + 12);
    size_t max_groups = (table_size - 16) / 12;
    if (num_groups > max_groups) {
        num_groups = static_cast<uint32_t>(max_groups);
    }

    for (uint32_t i = 0; i < num_groups; i++) {
        const uint8_t* g = table + 16 + static_cast<size_t>(i) * 12;
        uint32_t start_cp = ReadBE32(g);
        uint32_t end_cp = ReadBE32(g + 4);
        uint32_t start_glyph = ReadBE32(g + 8);
        if (start_glyph == 0 && start_cp < end_cp) {
            start_cp++;
        } else if (start_glyph == 0) {
            continue;
        }
        AppendRange(ranges, start_cp, end_cp);
    }
}

std::vector<CodePointRange> LoadSFNTUnicodeRanges(const std::string& full_path, int collection_index) {
    std::vector<CodePointRange> ranges;
    if (collection_index < 0) {
        return ranges;
    }

    ScopedHolder<FILE*> fp(std::fopen(full_path.c_str(), "rb"), [](FILE* f) { std::fclose(f); });
    if (!fp) {
        return ranges;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        return ranges;
    }
    long file_size_long = ftell(fp);
    if (file_size_long < 12) {
        return ranges;
    }
    uint64_t file_size = static_cast<uint64_t>(file_size_long);

    uint8_t header[12];
    if (!ReadFileAt(fp, 0, header, sizeof(header))) {
        return ranges;
    }

    uint32_t sfnt_tag = ReadBE32(header);
    uint64_t sfnt_offset = 0;
    if (sfnt_tag == 0x74746366u) {  // 'ttcf'
        uint32_t num_fonts = ReadBE32(header + 8);
        if (static_cast<uint32_t>(collection_index) >= num_fonts) {
            return ranges;
        }
        uint64_t entry_off = 12 + static_cast<uint64_t>(collection_index) * 4;
        if (entry_off + 4 > file_size) {
            return ranges;
        }
        uint8_t off_buf[4];
        if (!ReadFileAt(fp, entry_off, off_buf, sizeof(off_buf))) {
            return ranges;
        }
        sfnt_offset = ReadBE32(off_buf);
        if (sfnt_offset + 12 > file_size || !ReadFileAt(fp, sfnt_offset, header, sizeof(header))) {
            return ranges;
        }
        sfnt_tag = ReadBE32(header);
    }

    if (sfnt_tag != 0x00010000u && sfnt_tag != 0x4F54544Fu && sfnt_tag != 0x74727565u) {
        return ranges;
    }

    uint16_t num_tables = ReadBE16(header + 4);
    uint64_t dir_size = static_cast<uint64_t>(num_tables) * 16;
    if (sfnt_offset + 12 + dir_size > file_size) {
        return ranges;
    }

    std::vector<uint8_t> table_dir(static_cast<size_t>(dir_size));
    if (!ReadFileAt(fp, sfnt_offset + 12, table_dir.data(), table_dir.size())) {
        return ranges;
    }

    bool has_outline = false;
    bool has_color_bitmap = false;
    uint32_t cmap_offset = 0;
    uint32_t cmap_length = 0;
    for (uint16_t i = 0; i < num_tables; i++) {
        const uint8_t* rec = table_dir.data() + static_cast<size_t>(i) * 16;
        uint32_t tag = ReadBE32(rec);
        if (tag == 0x676C7966u || tag == 0x43464620u || tag == 0x43464632u) {  // 'glyf', 'CFF ', 'CFF2'
            has_outline = true;
        } else if (tag == 0x43424454u || tag == 0x73626978u) {  // 'CBDT', 'sbix'
            has_color_bitmap = true;
        } else if (tag == 0x636D6170u) {  // 'cmap'
            cmap_offset = ReadBE32(rec + 8);
            cmap_length = ReadBE32(rec + 12);
        }
    }

    // Android renders via FreeType's outline path, so skip bitmap-only/color-bitmap fonts
    constexpr uint32_t kMaxCmapSize = 16u * 1024u * 1024u;
    if (!has_outline || has_color_bitmap || cmap_length < 4 || cmap_length > kMaxCmapSize ||
        static_cast<uint64_t>(cmap_offset) + cmap_length > file_size) {
        return ranges;
    }

    std::vector<uint8_t> cmap(cmap_length);
    if (!ReadFileAt(fp, cmap_offset, cmap.data(), cmap.size())) {
        return ranges;
    }

    uint16_t num_subtables = ReadBE16(cmap.data() + 2);
    if (4 + static_cast<size_t>(num_subtables) * 8 > cmap.size()) {
        return ranges;
    }

    uint32_t format12_off = 0;
    uint32_t format4_off = 0;
    for (uint16_t i = 0; i < num_subtables; i++) {
        const uint8_t* rec = cmap.data() + 4 + static_cast<size_t>(i) * 8;
        uint16_t platform_id = ReadBE16(rec);
        uint16_t encoding_id = ReadBE16(rec + 2);
        uint32_t sub_off = ReadBE32(rec + 4);
        if (sub_off + 2 > cmap.size()) {
            continue;
        }
        uint16_t format = ReadBE16(cmap.data() + sub_off);
        bool is_unicode_full = (platform_id == 3 && encoding_id == 10) || (platform_id == 0 && encoding_id == 4);
        bool is_unicode_bmp = (platform_id == 3 && encoding_id == 1) || (platform_id == 0 && encoding_id == 3);
        if (format == 12 && is_unicode_full && format12_off == 0) {
            format12_off = sub_off;
        } else if (format == 4 && is_unicode_bmp && format4_off == 0) {
            format4_off = sub_off;
        }
    }

    if (format12_off != 0) {
        ParseCmapFormat12(cmap.data() + format12_off, cmap.size() - format12_off, ranges);
    } else if (format4_off != 0) {
        ParseCmapFormat4(cmap.data() + format4_off, cmap.size() - format4_off, ranges);
    }

    if (ranges.empty()) {
        return ranges;
    }

    std::sort(ranges.begin(), ranges.end());
    std::vector<CodePointRange> merged;
    merged.reserve(ranges.size());
    for (const auto& r : ranges) {
        if (!merged.empty() && r.first <= merged.back().second + 1) {
            if (r.second > merged.back().second) {
                merged.back().second = r.second;
            }
        } else {
            merged.push_back(r);
        }
    }
    return merged;
}

bool RangesContainCodePoint(const std::vector<CodePointRange>& ranges, uint32_t ucs4) {
    auto it = std::upper_bound(ranges.begin(), ranges.end(), ucs4,
                               [](uint32_t cp, const CodePointRange& r) { return cp < r.first; });
    if (it == ranges.begin()) {
        return false;
    }
    --it;
    return ucs4 >= it->first && ucs4 <= it->second;
}

bool FamilyMatchesLanguage(const FontFamily& family, std::string_view lang) {
    for (const auto& l : family.languages) {
        if (l == lang) {
            return true;
        }
    }
    return false;
}

}  // namespace

auto FontProviderAndroid::GetFontFace(const std::string& font_name) -> Result<FontfaceInfo, FontProviderError> {
    FontFamily* family = nullptr;

    if (iso6392_language_code_ == ThreeCC("jpn") || iso6392_language_code_ == 0) {
        family = FindFallbackFamilyByLanguageAndFallbackFor("ja", font_name);
        if (!family) {
            family = FindFallbackFamilyByLanguageAndFallbackFor("ja", "sans-serif");
        }
    } else {
        family = FindFamilyByName(font_name);
    }

    if (!family) {
        return Err(FontProviderError::kFontNotFound);
    }

    FontFile* font_file = FindRegularFont(*family);
    if (!font_file) {
        log_->w("FontProviderAndroid: Font family %s contains no font file", font_name.c_str());
        return Err(FontProviderError::kFontNotFound);
    }

    FontfaceInfo info;
    info.family_name = font_name;
    info.postscript_name = font_file->postscript_name;
    info.filename = base_font_path_ + font_file->filename;
    info.face_index = font_file->collection_index;
    info.provider_type = FontProviderType::kAndroid;

    return Ok(std::move(info));
}

bool FontProviderAndroid::FontHasCodePoint(const std::string& full_path, int collection_index, uint32_t ucs4) {
    std::string cache_key = full_path + "#" + std::to_string(collection_index);
    auto iter = cmap_cache_.find(cache_key);
    if (iter == cmap_cache_.end()) {
        iter = cmap_cache_.emplace(std::move(cache_key), LoadSFNTUnicodeRanges(full_path, collection_index)).first;
    }
    return RangesContainCodePoint(iter->second, ucs4);
}

auto FontProviderAndroid::GetFallbackFontFace(uint32_t ucs4, std::string_view base_family_name)
    -> Result<FontfaceInfo, FontProviderError> {
    if (ucs4 == 0 || unicode::IsPrivateUseArea(ucs4)) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    auto try_family = [&](FontFamily& family) -> std::optional<FontfaceInfo> {
        FontFile* font_file = FindRegularFont(family);
        if (!font_file) {
            return std::nullopt;
        }
        std::string full_path = base_font_path_ + font_file->filename;
        if (!FontHasCodePoint(full_path, font_file->collection_index, ucs4)) {
            return std::nullopt;
        }
        FontfaceInfo info;
        info.family_name = !family.names.empty() ? family.names[0] : family.fallback_for;
        info.postscript_name = font_file->postscript_name;
        info.filename = std::move(full_path);
        info.face_index = font_file->collection_index;
        info.provider_type = FontProviderType::kAndroid;
        return info;
    };

    std::string_view lang = ISO6392ToISO6391LanguageString(iso6392_language_code_);
    std::string_view target_fallback_for = base_family_name.empty() ? std::string_view("sans-serif") : base_family_name;

    auto scan = [&](bool require_language, std::string_view fallback_for) -> std::optional<FontfaceInfo> {
        for (auto& family : font_families_) {
            if (!family.is_fallback || family.fallback_for != fallback_for) {
                continue;
            }
            if (require_language && !FamilyMatchesLanguage(family, lang)) {
                continue;
            }
            if (auto info = try_family(family)) {
                return info;
            }
        }
        return std::nullopt;
    };

    // 1. Language-matching fallback families: the exact style of base_family_name first, then sans-serif.
    // 2. Same for fallback families of any language.
    // Language outranks style, e.g. a Japanese sans-serif fallback beats a serif fallback for another language.
    for (bool require_language : {true, false}) {
        if (auto info = scan(require_language, target_fallback_for)) {
            return Ok(std::move(*info));
        }
        if (target_fallback_for != "sans-serif") {
            if (auto info = scan(require_language, "sans-serif")) {
                return Ok(std::move(*info));
            }
        }
    }

    // 3. Remaining fallback families of any style
    for (auto& family : font_families_) {
        if (!family.is_fallback) {
            continue;
        }
        if (auto info = try_family(family)) {
            return Ok(std::move(*info));
        }
    }

    // 4. Named families (e.g. sans-serif / Roboto when main font was a CJK fallback)
    for (auto& family : font_families_) {
        if (family.is_fallback) {
            continue;
        }
        if (auto info = try_family(family)) {
            return Ok(std::move(*info));
        }
    }

    return Err(FontProviderError::kCodePointNotFound);
}

FontFamily* FontProviderAndroid::FindFamilyByName(std::string_view search_name) {
    for (auto& family : font_families_) {
        for (auto& name : family.names) {
            if (name == search_name) {
                return &family;
            }
        }
    }

    return nullptr;
}

FontFamily* FontProviderAndroid::FindFallbackFamilyByLanguageAndFallbackFor(std::string_view lang,
                                                                            std::string_view fallback_for) {
    for (auto& family : font_families_) {
        if (!family.is_fallback) {
            continue;
        }
        if (family.fallback_for != fallback_for) {
            continue;
        }
        for (auto& language : family.languages) {
            if (language == lang) {
                return &family;
            }
        }
    }

    return nullptr;
}

bool FontProviderAndroid::ParseAndroidSystemFonts() {
    // First, try parse the fonts.xml with new format (Lollipop+)
    // /system/etc/fonts.xml
    if (!ParseFontsXML(kAndroidFontsXML_LMP)) {
        log_->w("FontProviderAndroid: Load Lollipop+ config %s failed", kAndroidFontsXML_LMP);

        bool legacy_config_load_succeed = false;

        // If failed, fallback to parse the old system_fonts.xml & fallback_fonts.xml
        // /system/etc/system_fonts.xml
        if (!ParseFontsXML(kAndroidFontsXML_OLD_System)) {
            log_->w("FontProviderAndroid: Load legacy config %s failed", kAndroidFontsXML_OLD_System);
        } else {
            legacy_config_load_succeed = true;
        }

        // /system/etc/fallback_fonts-ja.xml
        if (!ParseFontsXML(kAndroidFontsXML_OLD_Fallback_JA)) {
            // /system/etc/fallback_fonts.xml
            if (!ParseFontsXML(kAndroidFontsXML_OLD_Fallback)) {
                log_->w("FontProviderAndroid: Load legacy fallback config %s failed", kAndroidFontsXML_OLD_Fallback);
            }
        }

        // /vendor/etc/fallback_fonts-ja.xml
        if (!ParseFontsXML(kAndroidFontsXML_OLD_Vendor_JA)) {
            // /vendor/etc/fallback_fonts.xml
            if (!ParseFontsXML(kAndroidFontsXML_OLD_Vendor)) {
                log_->v("FontProviderAndroid: Cannot load legacy vendor config %s", kAndroidFontsXML_OLD_Vendor);
            }
        }

        // If failed, consider Android version is below 4.x (e.g. 2.x, 3.x)
        if (!legacy_config_load_succeed) {
            if (!PrepareFontsForGingerbread()) {
                log_->w("FontProviderAndroid: Search fonts for Android 2.x (Gingerbread) failed");
                return false;
            }
        }

        AnnotateLanguageForOldFamilySets();
    }

    return true;
}

static bool CheckFileExists(const char* filename) {
    return access(filename, R_OK) == 0;
}

bool FontProviderAndroid::CheckFileAndAppendFontFamily(const char* family_name, const char* filename, bool is_fallback) {
    std::string full_filename = base_font_path_ + filename;
    if (!CheckFileExists(full_filename.c_str())) {
        return false;
    }

    FontFamily& family = font_families_.emplace_back();
    if (family_name) {
        family.names.emplace_back(family_name);
    }
    if (is_fallback) {
        family.is_fallback = true;
        family.fallback_for = "sans-serif";
    }

    FontFile& font = family.fonts.emplace_back();
    font.filename = filename;
    font.weight = 400;
    return true;
}

bool FontProviderAndroid::PrepareFontsForGingerbread() {
    bool ret = CheckFileAndAppendFontFamily("sans-serif", "DroidSans.ttf", false);
    if (!ret) {
        return false;
    }

    CheckFileAndAppendFontFamily("serif", "DroidSerif-Regular.ttf", false);
    CheckFileAndAppendFontFamily("monospace", "DroidSansMono.ttf", false);

    CheckFileAndAppendFontFamily(nullptr, "MTLmr3m.ttf", true);
    CheckFileAndAppendFontFamily(nullptr, "DroidSansJapanese.ttf", true);
    CheckFileAndAppendFontFamily(nullptr, "DroidSansFallback.ttf", true);
    return true;
}

static bool AnnotateJAForSpecificFontFile(std::vector<FontFamily>& font_families, std::string_view font_filename) {
    bool detected_filename = false;

    for (auto& family : font_families) {
        for (auto& font : family.fonts) {
            if (font.filename == font_filename) {
                detected_filename = true;
                family.languages.emplace_back("ja");
            }
        }
    }

    return detected_filename;
}

bool FontProviderAndroid::AnnotateLanguageForOldFamilySets() {
    bool detected_ja_font = false;

    for (auto& family : font_families_) {
        for (auto& lang : family.languages) {
            if (lang == "ja") detected_ja_font = true;
        }
    }

    if (detected_ja_font) {
        return true;
    }
    // else: annotate lang="ja" for MTLmr3m.ttf, DroidSansJapanese.ttf or DroidSansFallback.ttf

    bool ret = AnnotateJAForSpecificFontFile(font_families_, "MTLmr3m.ttf");
    if (ret) {
        return true;
    }

    ret = AnnotateJAForSpecificFontFile(font_families_, "DroidSansJapanese.ttf");
    if (ret) {
        return true;
    }

    ret = AnnotateJAForSpecificFontFile(font_families_, "DroidSansFallback.ttf");
    if (ret) {
        return true;
    }

    return false;
}

bool FontProviderAndroid::ParseFontsXML(const char *xml_path) {
    XMLDocument doc(true, Whitespace::COLLAPSE_WHITESPACE);
    XMLError err = doc.LoadFile(xml_path);
    if (err != XMLError::XML_SUCCESS) {
        log_->v("FontProviderAndroid: Open %s failed", xml_path);
        return false;
    }

    XMLElement* root = doc.RootElement();
    if (!root) {
        log_->e("FontProviderAndroid: Root element not found in %s", xml_path);
        return false;
    }
    if (strcmp(root->Name(), "familyset") != 0) {
        log_->e("FontProviderAndroid: Root element must be familyset, found %s", root->Name());
        return false;
    }

    const XMLAttribute* version_attr = root->FindAttribute("version");
    if (version_attr && version_attr->UnsignedValue() >= 21) {
        return HandleFamilySetLMP(root);
    } else {
        return HandleFamilySetOLD(root);
    }
}

static void SplitByComma(const char* input, std::vector<std::string>& out) {
    std::string str;

    for (const char* s = input; *s != 0; s++) {
        if (*s == ',') {
            out.push_back(std::move(str));
            str = std::string();
        } else if (*s == ' ' || *s == '\t') {
            continue;
        } else {
            str.push_back(*s);
        }
    }

    if (!str.empty()) {
        out.push_back(std::move(str));
    }
}

// Android 5.0+ fonts.xml
bool FontProviderAndroid::HandleFamilySetLMP(XMLElement* root) {
    XMLElement* element = root->FirstChildElement();

    FontFamily current_family;

    while (element) {
        if (strcmp(element->Name(), "family") == 0) {
            LMPHandleFamily(element);
        } else if (strcmp(element->Name(), "alias") == 0) {
            LMPHandleAlias(element);
        }
        element = element->NextSiblingElement();
    }

    return true;
}

bool FontProviderAndroid::LMPHandleFamily(XMLElement* element) {
    FontFamily current_family;

    if (const char* name = element->Attribute("name")) {
        current_family.names.emplace_back(name);
    }

    if (const char* lang = element->Attribute("lang")) {
        SplitByComma(lang, current_family.languages);
    }

    // Unnamed <family> blocks (with or without lang) are fallback families in Android fonts.xml
    if (current_family.names.empty()) {
        current_family.is_fallback = true;
        current_family.fallback_for = "sans-serif";
    }

    if (const char* variant = element->Attribute("variant")) {
        if (strcmp(variant, "compact") == 0) {
            current_family.variant = FontVariant::kCompact;
        } else if (strcmp(variant, "elegant") == 0) {
            current_family.variant = FontVariant::kElegant;
        }
    }

    element = element->FirstChildElement();

    while (element) {
        if (strcmp(element->Name(), "font") == 0) {
            LMPHandleFont(element, current_family);
        }
        element = element->NextSiblingElement();
    }

    font_families_.push_back(std::move(current_family));

    return true;
}

bool FontProviderAndroid::LMPHandleFont(XMLElement* element, internal::FontFamily& family) {
    const char* filename = element->GetText();
    if (!filename) {
        return false;
    }

    FontFile font;
    font.filename = filename;
    font.weight = element->IntAttribute("weight", 400);
    font.collection_index = element->IntAttribute("index", 0);

    if (const char* style = element->Attribute("style")) {
        if (strcmp(style, "italic") == 0) {
            font.is_italic = true;
        }
    }

    if (const char* postscript_name = element->Attribute("postScriptName")) {
        font.postscript_name = postscript_name;
    }

    if (const char* fallback_for = element->Attribute("fallbackFor")) {
        if (family.languages.empty()) {
            return false;
        }

        FontFamily* fallback_family =
                FindFallbackFamilyByLanguageAndFallbackFor(family.languages[0], fallback_for);
        if (!fallback_family) {
            FontFamily& new_fallback_family = font_families_.emplace_back();
            new_fallback_family.languages = family.languages;
            new_fallback_family.variant = family.variant;
            new_fallback_family.is_fallback = true;
            new_fallback_family.fallback_for = fallback_for;
            new_fallback_family.fonts.push_back(std::move(font));
        } else {
            fallback_family->fonts.push_back(std::move(font));
        }
    } else {
        family.fonts.push_back(std::move(font));
    }

    return true;
}

bool FontProviderAndroid::LMPHandleAlias(tinyxml2::XMLElement* element) {
    if (!element->FindAttribute("name") || !element->FindAttribute("to")) {
        log_->e("FontProviderAndroid: Missing name/to for alias in fonts.xml");
        return false;
    }

    if (const XMLAttribute* weight_attr = element->FindAttribute("weight")) {
        FontFamily* target_family = FindFamilyByName(element->Attribute("to"));
        if (!target_family) {
            log_->e("FontProviderAndroid: Alias target not found: %s", element->Attribute("to"));
            return false;
        }

        FontFamily new_family;
        new_family.names = {element->Attribute("name")};
        new_family.languages = target_family->languages;
        new_family.variant = target_family->variant;
        new_family.is_fallback = target_family->is_fallback;
        new_family.fallback_for = target_family->fallback_for;

        int request_weight = weight_attr->IntValue();

        for (auto& font : target_family->fonts) {
            if (font.weight == request_weight) {
                new_family.fonts.push_back(font);
            }
        }

        font_families_.push_back(std::move(new_family));
    } else {
        FontFamily* target_family = FindFamilyByName(element->Attribute("to"));
        if (!target_family) {
            log_->e("FontProviderAndroid: Alias target not found: %s", element->Attribute("to"));
            return false;
        }
        target_family->names.emplace_back(element->Attribute("name"));
    }

    return true;
}

// Before Android 5.0, system_fonts.xml / fallback_fonts.xml
bool FontProviderAndroid::HandleFamilySetOLD(XMLElement* root) {
    XMLElement* element = root->FirstChildElement();
    while (element) {
        if (strcmp(element->Name(), "family") == 0) {
            FontFamily family;
            if (JBHandleFamily(element, family)) {
                // Parse success
                font_families_.push_back(std::move(family));
            }
        }
        element = element->NextSiblingElement();
    }
    return true;
}

bool FontProviderAndroid::JBHandleFamily(XMLElement* element, FontFamily& family) {
    bool has_nameset = false;
    element = element->FirstChildElement();

    while (element) {
        if (strcmp(element->Name(), "nameset") == 0) {
            has_nameset = true;
            if (!JBHandleNameset(element, family))
                return false;
        } else if (strcmp(element->Name(), "fileset") == 0) {
            if (!JBHandleFileset(element, family))
                return false;
        }
        element = element->NextSiblingElement();
    }

    if (!has_nameset) {
        family.is_fallback = true;
        family.fallback_for = "sans-serif";
    }

    return true;
}

bool FontProviderAndroid::JBHandleNameset(XMLElement* element, FontFamily& family) {
    element = element->FirstChildElement();
    while (element) {
        if (strcmp(element->Name(), "name") == 0) {
            if (const char* name = element->GetText()) {
                family.names.emplace_back(name);
            }
        }
        element = element->NextSiblingElement();
    }
    return true;
}

bool FontProviderAndroid::JBHandleFileset(XMLElement* element, FontFamily& family) {
    element = element->FirstChildElement();
    while (element) {
        if (strcmp(element->Name(), "file") == 0) {
            if (!JBHandleFile(element, family))
                return false;
        }
        element = element->NextSiblingElement();
    }
    return true;
}

bool FontProviderAndroid::JBHandleFile(XMLElement* element, FontFamily& family) {
    const char* filename = element->GetText();
    if (!filename) {
        return true;  // Skip <file> without filename
    }

    FontFile& font = family.fonts.emplace_back();
    font.filename = filename;

    std::string filename_lcase(font.filename.length(), 0);
    std::transform(font.filename.begin(),
                   font.filename.end(),
                   filename_lcase.begin(),
                   [](char c) { return std::tolower(c); });

    if (filename_lcase.find("thin") != std::string::npos) {
        font.weight = 100;
    } else if (filename_lcase.find("light") != std::string::npos) {
        font.weight = 300;
    } else if (filename_lcase.find("regular") != std::string::npos) {
        font.weight = 400;
    } else if (filename_lcase.find("medium") != std::string::npos) {
        font.weight = 500;
    } else if (filename_lcase.find("black") != std::string::npos) {
        font.weight = 900;
    } else if (filename_lcase.find("bold") != std::string::npos) {
        font.weight = 700;
    } else {
        // default as 400 (Regular)
        font.weight = 400;
    }

    if (filename_lcase.find("italic") != std::string::npos) {
        font.is_italic = true;
    }

    if (const char* lang = element->Attribute("lang")) {
        auto& languages = family.languages;
        if (languages.empty() || std::find(languages.begin(), languages.end(), lang) == languages.end()) {
            languages.emplace_back(lang);
        }
    }

    if (const char* variant = element->Attribute("variant")) {
        if (strcmp(variant, "elegant") == 0) {
            family.variant = FontVariant::kElegant;
        } else if (strcmp(variant, "compact") == 0) {
            family.variant = FontVariant::kCompact;
        }
    }

    return true;
}

}  // namespace aribcaption
