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
#include <initguid.h>
#include "base/wchar_helper.hpp"
#include "renderer/font_provider_directwrite.hpp"

namespace aribcaption {

constexpr IID IID_IDWriteFactory = {0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};

FontProviderDirectWrite::FontProviderDirectWrite(Context& context) : log_(GetContextLogger(context)) {}

FontProviderType FontProviderDirectWrite::GetType() {
    return FontProviderType::kDirectWrite;
}

bool FontProviderDirectWrite::Initialize() {
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                                     IID_IDWriteFactory,
                                     static_cast<IUnknown**>(&dwrite_factory_));
    if (FAILED(hr)) {
        log_->e("FontProviderDirectWrite: Failed to create IDWriteFactory");
        return false;
    }

    hr = dwrite_factory_->GetGdiInterop(&dwrite_gdi_interop_);
    if (FAILED(hr)) {
        log_->e("FontProviderDirectWrite: Failed to retrieve IDWriteGdiInterop");
        return false;
    }

    return true;
}

ComPtr<IDWriteFactory> FontProviderDirectWrite::GetDWriteFactory() {
    return dwrite_factory_;
}

void FontProviderDirectWrite::SetLanguage(uint32_t iso6392_language_code) {
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

static uint32_t GetDWriteLocaleIndex(IDWriteLocalizedStrings* strs) {
    uint32_t index = 0;
    BOOL exists = FALSE;
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH] = {0};

    int success = GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH);
    if (success) {
        HRESULT hr = strs->FindLocaleName(locale_name, &index, &exists);
        if (FAILED(hr)) return 0;
    }

    if (!exists) {
        HRESULT hr = strs->FindLocaleName(L"en-us", &index, &exists);
        if (FAILED(hr)) return 0;
    }

    if (!exists) {
        index = 0;
    }

    return index;
}

static std::string DWriteLocalizedStringsToUTF8(IDWriteLocalizedStrings* strs,
                                                std::optional<uint32_t> opt_index = std::nullopt) {
    std::string u8str;

    uint32_t index = 0;
    if (opt_index.has_value()) {
        index = opt_index.value();
    } else {
        index = GetDWriteLocaleIndex(strs);
    }

    uint32_t len = 0;

    HRESULT hr = strs->GetStringLength(index, &len);
    if (FAILED(hr)) return u8str;

    std::wstring wstr;
    wstr.resize(len);

    hr = strs->GetString(index, wstr.data(), len + 1);
    if (FAILED(hr)) return u8str;

    u8str = wchar::WideStringToUTF8(wstr);
    return u8str;
}

auto FontProviderDirectWrite::GetFontFace(const std::string& font_name,
                                          std::optional<uint32_t> ucs4) -> Result<FontfaceInfo, FontProviderError> {
    std::string converted_family_name = ConvertFamilyName(font_name, iso6392_language_code_);
    std::wstring wide_font_name = wchar::UTF8ToWideString(converted_family_name);

    LOGFONTW lf = {0};
    wcscpy_s(lf.lfFaceName, wide_font_name.c_str());
    lf.lfWeight = FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_TT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = ANTIALIASED_QUALITY;
    lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;

    ComPtr<IDWriteFont> dwrite_font;
    HRESULT hr = dwrite_gdi_interop_->CreateFontFromLOGFONT(&lf, &dwrite_font);
    if (FAILED(hr)) {
        log_->e("FontProviderDirectWrite: IDWriteGdiInterop::CreateFontFromLOGFONT() failed");
        return Err(FontProviderError::kFontNotFound);
    }

    ComPtr<IDWriteFontFamily> dwrite_font_family;
    hr = dwrite_font->GetFontFamily(&dwrite_font_family);
    if (FAILED(hr)) {
        log_->e("FontProviderDirectWrite: IDWriteFont::GetFontFamily() failed");
        return Err(FontProviderError::kOtherError);
    }

    ComPtr<IDWriteFontFace> dwrite_fontface;
    hr = dwrite_font->CreateFontFace(&dwrite_fontface);
    if (FAILED(hr)) {
        log_->e("FontProviderDirectWrite: IDWriteFont::CreateFontFace() failed");
        return Err(FontProviderError::kOtherError);
    }

    // Check whether the font contains the requested Unicode codepoint
    if (ucs4.has_value()) {
        BOOL ucs4_exists = FALSE;
        hr = dwrite_font->HasCharacter(ucs4.value(), &ucs4_exists);
        if (FAILED(hr) || !ucs4_exists) {
            log_->w("FontProviderDirectWrite: Font %s doesn't contain U+%04X", font_name.c_str(), ucs4.value());
            return Err(FontProviderError::kCodePointNotFound);
        }
    }

    // Retrieve Font family name
    BOOL exists = FALSE;
    ComPtr<IDWriteLocalizedStrings> localized_family_names;
    hr = dwrite_font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_WIN32_FAMILY_NAMES,
                                              &localized_family_names,
                                              &exists);
    if (FAILED(hr) || !exists) {
        hr = dwrite_font_family->GetFamilyNames(&localized_family_names);
        if (FAILED(hr)) {
            log_->w("FontProviderDirectWrite: Retrieve font family name failed");
            return Err(FontProviderError::kOtherError);
        }
    }

    // Retrieve Font PostScript name
    ComPtr<IDWriteLocalizedStrings> localized_postscript_names;
    hr = dwrite_font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME,
                                              &localized_postscript_names,
                                              &exists);
    if (FAILED(hr) || !exists) {
        log_->w("FontProviderDirectWrite: Retrieve font PostScript name failed");
        return Err(FontProviderError::kOtherError);
    }

    // Retrieve Font filename
    uint32_t file_count = 1;
    ComPtr<IDWriteFontFile> dwrite_font_file;
    hr = dwrite_fontface->GetFiles(&file_count, &dwrite_font_file);
    if (FAILED(hr) || !dwrite_font_file) {
        log_->w("FontProviderDirectWrite: Retrieve font file name failed");
        return Err(FontProviderError::kOtherError);
    }

    const void* reference_key = nullptr;
    uint32_t key_size = 0;

    hr = dwrite_font_file->GetReferenceKey(&reference_key, &key_size);
    if (FAILED(hr)) {
        log_->w("FontProviderDirectWrite: IDWriteFontFile::GetReferenceKey() failed");
        return Err(FontProviderError::kOtherError);
    }

    ComPtr<IDWriteFontFileLoader> dwrite_font_fileloader;
    hr = dwrite_font_file->GetLoader(&dwrite_font_fileloader);
    if (FAILED(hr)) {
        log_->w("FontProviderDirectWrite: IDWriteFontFileLoader::GetLoader() failed");
        return Err(FontProviderError::kOtherError);
    }

    ComPtr<IDWriteLocalFontFileLoader> dwrite_local_font_fileloader;
    hr = dwrite_font_fileloader.As(&dwrite_local_font_fileloader);
    if (FAILED(hr)) {
        log_->w("FontProviderDirectWrite: QueryInterface to IDWriteLocalFontFileLoader failed");
        return Err(FontProviderError::kOtherError);
    }

    std::wstring file_path;
    uint32_t file_path_len = 0;
    hr = dwrite_local_font_fileloader->GetFilePathLengthFromKey(reference_key, key_size, &file_path_len);
    if (FAILED(hr)) {
        log_->w("FontProviderDirectWrite: IDWriteLocalFontFileLoader GetFilePathLengthFromKey() failed");
        return Err(FontProviderError::kOtherError);
    }

    file_path.resize(file_path_len);
    hr = dwrite_local_font_fileloader->GetFilePathFromKey(reference_key, key_size, file_path.data(), file_path_len + 1);
    if (FAILED(hr)) {
        log_->w("FontProviderDirectWrite: IDWriteLocalFontFileLoader GetFilePathFromKey() failed");
        return Err(FontProviderError::kOtherError);
    }


    FontfaceInfo fontface_info;
    fontface_info.filename = wchar::WideStringToUTF8(file_path);
    fontface_info.family_name = DWriteLocalizedStringsToUTF8(localized_family_names.Get());
    fontface_info.postscript_name = DWriteLocalizedStringsToUTF8(localized_postscript_names.Get(), 0);
    fontface_info.face_index = static_cast<int>(dwrite_fontface->GetIndex());
    fontface_info.provider_type = FontProviderType::kDirectWrite;

    auto fontface_info_private = std::make_unique<FontfaceInfoPrivateDirectWrite>();
    fontface_info_private->font = std::move(dwrite_font);
    fontface_info_private->fontface = std::move(dwrite_fontface);

    fontface_info.provider_priv = std::move(fontface_info_private);

    return Ok(std::move(fontface_info));
}

}  // namespace aribcaption
