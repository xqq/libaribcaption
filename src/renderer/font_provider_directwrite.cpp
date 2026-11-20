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
#include <optional>
#include <string_view>
#include "base/unicode_helper.hpp"
#include "base/utf_helper.hpp"
#include "base/wchar_helper.hpp"
#include "renderer/font_provider_directwrite.hpp"

namespace aribcaption {

constexpr IID IID_IDWriteFactory = {0xb859ee5a, 0xd838, 0x4b5b, {0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48}};

namespace {

class SingleCharTextAnalysisSource final : public IDWriteTextAnalysisSource {
public:
    SingleCharTextAnalysisSource(const WCHAR* text, UINT32 text_length, const WCHAR* locale_name)
        : text_(text), text_length_(text_length), locale_name_(locale_name) {}

    IFACEMETHOD(GetTextAtPosition)(UINT32 text_position, const WCHAR** text_string, UINT32* text_length) override {
        if (text_position >= text_length_) {
            *text_string = nullptr;
            *text_length = 0;
        } else {
            *text_string = text_ + text_position;
            *text_length = text_length_ - text_position;
        }
        return S_OK;
    }

    IFACEMETHOD(GetTextBeforePosition)(UINT32 text_position, const WCHAR** text_string, UINT32* text_length) override {
        if (text_position == 0 || text_position > text_length_) {
            *text_string = nullptr;
            *text_length = 0;
        } else {
            *text_string = text_;
            *text_length = text_position;
        }
        return S_OK;
    }

    IFACEMETHOD_(DWRITE_READING_DIRECTION, GetParagraphReadingDirection)() override {
        return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
    }

    IFACEMETHOD(GetLocaleName)(UINT32 text_position, UINT32* text_length, const WCHAR** locale_name) override {
        *text_length = text_position < text_length_ ? (text_length_ - text_position) : 0;
        *locale_name = locale_name_;
        return S_OK;
    }

    IFACEMETHOD(GetNumberSubstitution)(UINT32 text_position,
                                       UINT32* text_length,
                                       IDWriteNumberSubstitution** number_substitution) override {
        *text_length = text_position < text_length_ ? (text_length_ - text_position) : 0;
        *number_substitution = nullptr;
        return S_OK;
    }

    IFACEMETHOD_(ULONG, AddRef)() override {
        return InterlockedIncrement(&ref_count_);
    }

    IFACEMETHOD_(ULONG, Release)() override {
        ULONG result = InterlockedDecrement(&ref_count_);
        if (result == 0) {
            delete this;
            return 0;
        }
        return result;
    }

    IFACEMETHOD(QueryInterface)(IID const& riid, void** ppv_object) override {
        if (riid == __uuidof(IDWriteTextAnalysisSource) || riid == __uuidof(IUnknown)) {
            *ppv_object = this;
            AddRef();
            return S_OK;
        }
        *ppv_object = nullptr;
        return E_NOINTERFACE;
    }
private:
    ULONG ref_count_ = 0;
    const WCHAR* text_ = nullptr;
    UINT32 text_length_ = 0;
    const WCHAR* locale_name_ = L"";
};

const wchar_t* ISO6392ToWindowsLocaleName(uint32_t iso6392_language_code) {
    switch (iso6392_language_code) {
        case ThreeCC("por"):
            return L"pt-BR";
        case ThreeCC("spa"):
            return L"es-CL";
        case ThreeCC("eng"):
            return L"en";
        case ThreeCC("jpn"):
        default:
            return L"ja-JP";
    }
}

}  // namespace

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

    // IDWriteFactory2 / IDWriteFontFallback requires Windows 8.1+; degrade gracefully on Windows 7
    ComPtr<IDWriteFactory2> dwrite_factory2;
    if (SUCCEEDED(dwrite_factory_.As(&dwrite_factory2)) && dwrite_factory2) {
        dwrite_factory2->GetSystemFontFallback(&dwrite_font_fallback_);
    }

    return true;
}

ComPtr<IDWriteFactory> FontProviderDirectWrite::GetDWriteFactory() {
    return dwrite_factory_;
}

void FontProviderDirectWrite::SetLanguage(uint32_t iso6392_language_code) {
    iso6392_language_code_ = iso6392_language_code;
}

// The returned view refers to either family_name or a string literal
static std::string_view ConvertFamilyName(std::string_view family_name, uint32_t iso6392_language_code) {
    std::string_view font_name = family_name;

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

auto FontProviderDirectWrite::GetFontFace(const std::string& font_name) -> Result<FontfaceInfo, FontProviderError> {
    std::string_view converted_family_name = ConvertFamilyName(font_name, iso6392_language_code_);
    std::wstring wide_font_name = wchar::UTF8ToWideString(converted_family_name);

    ComPtr<IDWriteFont> dwrite_font;
    ComPtr<IDWriteFontCollection> system_collection;
    HRESULT hr = dwrite_factory_->GetSystemFontCollection(&system_collection, FALSE);
    if (SUCCEEDED(hr) && system_collection) {
        UINT32 family_index = 0;
        BOOL family_exists = FALSE;
        hr = system_collection->FindFamilyName(wide_font_name.c_str(), &family_index, &family_exists);
        if (SUCCEEDED(hr) && family_exists) {
            ComPtr<IDWriteFontFamily> family;
            hr = system_collection->GetFontFamily(family_index, &family);
            if (SUCCEEDED(hr) && family) {
                family->GetFirstMatchingFont(DWRITE_FONT_WEIGHT_NORMAL,
                                             DWRITE_FONT_STRETCH_NORMAL,
                                             DWRITE_FONT_STYLE_NORMAL,
                                             &dwrite_font);
            }
        }
    }

    // Fall back to GDI LOGFONT lookup for GDI-only or subfamily names
    if (!dwrite_font && wide_font_name.length() < LF_FACESIZE) {
        LOGFONTW lf = {0};
        wcscpy_s(lf.lfFaceName, wide_font_name.c_str());
        lf.lfWeight = FW_NORMAL;
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfOutPrecision = OUT_TT_PRECIS;
        lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
        lf.lfQuality = ANTIALIASED_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        dwrite_gdi_interop_->CreateFontFromLOGFONT(&lf, &dwrite_font);
    }

    if (!dwrite_font) {
        log_->w("FontProviderDirectWrite: Cannot find font %s", font_name.c_str());
        return Err(FontProviderError::kFontNotFound);
    }

    return BuildFontfaceInfo(std::move(dwrite_font));
}

auto FontProviderDirectWrite::GetFallbackFontFace(uint32_t ucs4, std::string_view base_family_name)
    -> Result<FontfaceInfo, FontProviderError> {
    if (ucs4 == 0 || unicode::IsPrivateUseArea(ucs4) || !dwrite_font_fallback_) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    std::u16string utf16;
    size_t codeunit_count = utf::UTF16AppendCodePoint(utf16, ucs4);
    const wchar_t* locale_name = ISO6392ToWindowsLocaleName(iso6392_language_code_);

    std::wstring wide_base_family;
    if (!base_family_name.empty()) {
        std::string_view converted = ConvertFamilyName(base_family_name, iso6392_language_code_);
        wide_base_family = wchar::UTF8ToWideString(converted);
    }

    ComPtr<SingleCharTextAnalysisSource> source(new SingleCharTextAnalysisSource(
        reinterpret_cast<const WCHAR*>(utf16.data()),
        static_cast<UINT32>(codeunit_count),
        locale_name));

    UINT32 mapped_length = 0;
    ComPtr<IDWriteFont> mapped_font;
    FLOAT scale = 1.0f;
    HRESULT hr = dwrite_font_fallback_->MapCharacters(
        source.Get(),
        0,
        static_cast<UINT32>(codeunit_count),
        nullptr,
        wide_base_family.empty() ? nullptr : wide_base_family.c_str(),
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        &mapped_length,
        &mapped_font,
        &scale);
    if (FAILED(hr) || !mapped_font || mapped_length == 0) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    BOOL has_char = FALSE;
    hr = mapped_font->HasCharacter(ucs4, &has_char);
    if (FAILED(hr) || !has_char) {
        return Err(FontProviderError::kCodePointNotFound);
    }

    return BuildFontfaceInfo(std::move(mapped_font));
}

auto FontProviderDirectWrite::BuildFontfaceInfo(ComPtr<IDWriteFont> dwrite_font)
    -> Result<FontfaceInfo, FontProviderError> {
    ComPtr<IDWriteFontFamily> dwrite_font_family;
    HRESULT hr = dwrite_font->GetFontFamily(&dwrite_font_family);
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

    // Retrieve Font family name
    BOOL exists = FALSE;
    ComPtr<IDWriteLocalizedStrings> localized_family_names;
    hr = dwrite_font_family->GetFamilyNames(&localized_family_names);
    if (FAILED(hr)) {
        log_->w("FontProviderDirectWrite: Retrieve font family name failed");
        return Err(FontProviderError::kOtherError);
    }

    std::string postscript_name;
    ComPtr<IDWriteLocalizedStrings> localized_postscript_names;
    hr = dwrite_font->GetInformationalStrings(DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME,
                                              &localized_postscript_names,
                                              &exists);
    if (SUCCEEDED(hr) && exists && localized_postscript_names) {
        postscript_name = DWriteLocalizedStringsToUTF8(localized_postscript_names.Get(), 0);
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
    fontface_info.postscript_name = std::move(postscript_name);
    fontface_info.face_index = static_cast<int>(dwrite_fontface->GetIndex());
    fontface_info.provider_type = FontProviderType::kDirectWrite;

    auto fontface_info_private = std::make_unique<FontfaceInfoPrivateDirectWrite>();
    fontface_info_private->font = std::move(dwrite_font);
    fontface_info_private->fontface = std::move(dwrite_fontface);

    fontface_info.provider_priv = std::move(fontface_info_private);

    return Ok(std::move(fontface_info));
}

}  // namespace aribcaption
