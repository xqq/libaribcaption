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

#include <windows.h>
#include <vector>
#include "base/utf_helper.hpp"
#include "base/wchar_helper.hpp"
#include "renderer/font_provider_gdi.hpp"

namespace aribcaption {

FontProviderGDI::FontProviderGDI(Context& context) : log_(GetContextLogger(context)) {}

FontProviderType FontProviderGDI::GetType() {
    return FontProviderType::kGDI;
}

bool FontProviderGDI::Initialize() {
    HDC dc = CreateCompatibleDC(nullptr);
    if (!dc) {
        return false;
    }

    hdc_ = ScopedHolder<HDC>(dc, [](HDC h) { DeleteDC(h); });
    return true;
}

void FontProviderGDI::SetLanguage(uint32_t iso6392_language_code) {
    iso6392_language_code_ = iso6392_language_code;
}

static std::string ConvertFamilyName(const std::string& family_name, uint32_t iso6392_language_code) {
    std::string font_name = family_name;

    if (family_name == "sans-serif") {
        if (iso6392_language_code == ThreeCC("jpn")) {
            font_name = "MS Gothic";
        } else {
            font_name = "Verdana";
        }
    } else if (family_name == "serif") {
        if (iso6392_language_code == ThreeCC("jpn")) {
            font_name = "MS Mincho";
        } else {
            font_name = "Times New Roman";
        }
    } else if (family_name == "monospace") {
        if (iso6392_language_code == ThreeCC("jpn")) {
            font_name = "MS Gothic";
        } else {
            font_name = "Courier New";
        }
    }

    return font_name;
}

static bool RetrieveFontData(HDC hdc, std::vector<uint8_t>& buffer, bool& is_ttc) {
    constexpr DWORD ttcf_table = 0x66637474;
    DWORD table = ttcf_table;

    uint32_t size = GetFontData(hdc, ttcf_table, 0, nullptr, 0);
    if (size == GDI_ERROR) {
        is_ttc = false;
        table = 0;
        size = GetFontData(hdc, table, 0, nullptr, 0);
    } else {
        is_ttc = true;
        table = ttcf_table;
    }

    if (size == GDI_ERROR || !size) {
        return false;
    }

    buffer.resize(size);
    DWORD ret = GetFontData(hdc, table, 0, buffer.data(), size);
    if (ret == GDI_ERROR) {
        return false;
    }

    return true;
}

static bool CheckCodePointExists(HDC hdc, uint32_t ucs4) {
    std::u16string u16str;
    utf::UTF16AppendCodePoint(u16str, ucs4);

    std::u16string indices(u16str.length(), 0);

    DWORD ret = GetGlyphIndicesW(hdc,
                                 reinterpret_cast<LPCWSTR>(u16str.c_str()),
                                 static_cast<int>(u16str.length()),
                                 reinterpret_cast<LPWORD>(indices.data()),
                                 GGI_MARK_NONEXISTING_GLYPHS);
    if (ret == GDI_ERROR) {
        return false;
    }

    for (size_t i = 0; i < u16str.length(); i++) {
        if (utf::IsUTF16Surrogate(u16str[i])) {
            continue;
        }
        if (indices[i] == 0xffff) {
            return false;
        }
    }

    return true;
}

#if defined(_MSC_VER) && !defined(__clang__)
    // Specifying calling convention for lambdas is unsupported in MSVC (except clang-cl)
    #define LAMBDA_CALL_CONV(a)
#else
    // gcc, clang, etc.
    #define LAMBDA_CALL_CONV(a) a
#endif

auto FontProviderGDI::GetFontFace(const std::string& font_name, std::optional<uint32_t> ucs4)
        -> Result<FontfaceInfo, FontProviderError> {
    std::string converted_family_name = ConvertFamilyName(font_name, iso6392_language_code_);
    std::wstring wide_font_name = wchar::UTF8ToWideString(converted_family_name);

    LOGFONTW lf{};
    wcscpy_s(lf.lfFaceName, wide_font_name.c_str());
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;

    std::vector<LOGFONTW> matches;

    EnumFontFamiliesExW(
        hdc_,
        &lf,
        [](const LOGFONTW* lf, const TEXTMETRICW*, DWORD, LPARAM lparam) LAMBDA_CALL_CONV(CALLBACK) -> int {
            auto vec = reinterpret_cast<std::vector<LOGFONTW>*>(lparam);
            vec->push_back(*lf);
            return 1;
        },
        reinterpret_cast<LPARAM>(&matches),
        0
    );
    if (matches.empty()) {
        return Err(FontProviderError::kFontNotFound);
    }
    wcscpy_s(lf.lfFaceName, matches[0].lfFaceName);

    FontfaceInfo info;
    info.family_name = wchar::WideStringToUTF8(lf.lfFaceName);
    info.provider_type = FontProviderType::kGDI;

    ScopedHolder<HFONT> hfont(CreateFontIndirectW(&lf), [](HFONT obj) { DeleteObject(obj); });
    SelectObject(hdc_, hfont);

    if (ucs4.has_value()) {
        if (!CheckCodePointExists(hdc_, ucs4.value())) {
            SelectObject(hdc_, nullptr);
            return Err(FontProviderError::kCodePointNotFound);
        }
    }

    bool is_ttc = false;
    bool succ = RetrieveFontData(hdc_, info.font_data, is_ttc);
    if (!succ || info.font_data.empty()) {
        SelectObject(hdc_, nullptr);
        return Err(FontProviderError::kOtherError);
    }

    if (is_ttc) {
        info.face_index = -1;
    }  // else: face_index = 0

    SelectObject(hdc_, nullptr);
    return Ok(std::move(info));
}

}  // namespace aribcaption
