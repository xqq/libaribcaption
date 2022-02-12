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
#include <cmath>
#include <functional>
#include "base/utf_helper.hpp"
#include "base/wchar_helper.hpp"
#include "renderer/alphablend.hpp"
#include "renderer/canvas.hpp"
#include "renderer/font_provider_directwrite.hpp"
#include "renderer/text_renderer_directwrite.hpp"

namespace aribcaption {

class OutlineTextRenderer : public IDWriteTextRenderer {
public:
    using UnderlineDrawCB = std::function<void(const DWRITE_UNDERLINE* underline)>;
public:
    OutlineTextRenderer(float horizontal_scale,
                        float vertical_scale,
                        bool draw_outline_stroke,
                        float stroke_width,
                        ComPtr<ID2D1StrokeStyle> stroke_style,
                        ComPtr<ID2D1Factory> d2d_factory,
                        ComPtr<ID2D1RenderTarget> render_target,
                        ComPtr<ID2D1SolidColorBrush> fill_brush,
                        ComPtr<ID2D1SolidColorBrush> outline_brush,
                        UnderlineDrawCB underline_draw_cb)
        : horizontal_scale_(horizontal_scale),
          vertical_scale_(vertical_scale),
          draw_outline_stroke_(draw_outline_stroke),
          stroke_width_(stroke_width),
          stroke_style_(std::move(stroke_style)),
          d2d_factory_(std::move(d2d_factory)),
          render_target_(std::move(render_target)),
          fill_brush_(std::move(fill_brush)),
          outline_brush_(std::move(outline_brush)),
          underline_draw_cb_(std::move(underline_draw_cb)) {}

    virtual ~OutlineTextRenderer() = default;

    IFACEMETHOD(IsPixelSnappingDisabled)(void* client_drawing_context, BOOL* is_disabled) {
        *is_disabled = FALSE;
        return S_OK;
    }

    IFACEMETHOD(GetCurrentTransform)(void* client_drawing_context, DWRITE_MATRIX* transform) {
        render_target_->GetTransform(reinterpret_cast<D2D1_MATRIX_3X2_F*>(transform));
        return S_OK;
    }

    IFACEMETHOD(GetPixelsPerDip)(void* client_drawing_context, FLOAT* pixels_pre_dip) {
        float dpi_x = 0.0f;
        float dpi_y = 0.0f;
        render_target_->GetDpi(&dpi_x, &dpi_y);
        *pixels_pre_dip = dpi_x / 96;
        return S_OK;
    }

    IFACEMETHOD(DrawGlyphRun)(void* client_drawing_context,
                              float baseline_origin_x,
                              float baseline_origin_y,
                              DWRITE_MEASURING_MODE measuring_mode,
                              DWRITE_GLYPH_RUN const* glyph_run,
                              DWRITE_GLYPH_RUN_DESCRIPTION const* glyph_run_description,
                              IUnknown* client_drawing_effect) {
        ComPtr<ID2D1PathGeometry> path_geometry;
        HRESULT hr = d2d_factory_->CreatePathGeometry(&path_geometry);
        if (FAILED(hr))
            return hr;

        ComPtr<ID2D1GeometrySink> sink;
        hr = path_geometry->Open(&sink);
        if (FAILED(hr))
            return hr;

        hr = glyph_run->fontFace->GetGlyphRunOutline(
            glyph_run->fontEmSize,
            glyph_run->glyphIndices,
            glyph_run->glyphAdvances,
            glyph_run->glyphOffsets,
            glyph_run->glyphCount,
            glyph_run->isSideways,
            glyph_run->bidiLevel % 2,
            sink.Get()
        );
        if (FAILED(hr))
            return hr;

        hr = sink->Close();
        if (FAILED(hr))
            return hr;

        const D2D1::Matrix3x2F matrix = D2D1::Matrix3x2F(
            horizontal_scale_, 0.0f,
            0.0f, vertical_scale_,
            baseline_origin_x, baseline_origin_y
        );

        ComPtr<ID2D1TransformedGeometry> transformed_geometry;
        hr = d2d_factory_->CreateTransformedGeometry(path_geometry.Get(), matrix, &transformed_geometry);
        if (FAILED(hr))
            return hr;

        if (draw_outline_stroke_) {
            render_target_->DrawGeometry(transformed_geometry.Get(),
                                         outline_brush_.Get(),
                                         stroke_width_,
                                         stroke_style_.Get());
        }
        render_target_->FillGeometry(transformed_geometry.Get(), fill_brush_.Get());

        return S_OK;
    }

    IFACEMETHOD(DrawUnderline)(void* client_drawing_context,
                               float baseline_origin_x,
                               float baseline_origin_y,
                               DWRITE_UNDERLINE const* underline,
                               IUnknown* client_drawing_effect) {
        underline_draw_cb_(underline);
        return S_OK;
    }

    IFACEMETHOD(DrawStrikethrough)(void* client_drawing_context,
                                   float baseline_origin_x,
                                   float baseline_origin_y,
                                   DWRITE_STRIKETHROUGH const* strike_through,
                                   IUnknown* client_drawing_effect) {
        return E_NOTIMPL;
    }

    IFACEMETHOD(DrawInlineObject)(void* client_drawing_context,
                                  float origin_x,
                                  float origin_y,
                                  IDWriteInlineObject* inline_object,
                                  BOOL is_sideways,
                                  BOOL is_right_to_left,
                                  IUnknown* client_drawing_effect) {
        return E_NOTIMPL;
    }
public:
    IFACEMETHOD_(unsigned long, AddRef)() {
        return InterlockedIncrement(&ref_count_);
    }

    IFACEMETHOD_(unsigned long, Release)() {
        unsigned long result = InterlockedDecrement(&ref_count_);
        if (result == 0) {
            delete this;
            return 0;
        }
        return result;
    }

    IFACEMETHOD(QueryInterface)(IID const& riid, void** ppv_object) {
        if (riid == __uuidof(IDWriteTextRenderer) ||
            riid == __uuidof(IDWritePixelSnapping) ||
            riid == __uuidof(IUnknown)) {
            *ppv_object = this;
        } else {
            *ppv_object = nullptr;
            return E_FAIL;
        }

        this->AddRef();
        return S_OK;
    }
private:
    unsigned long ref_count_ = 0;
    float horizontal_scale_ = 1.0f;
    float vertical_scale_ = 1.0f;
    bool draw_outline_stroke_ = false;
    float stroke_width_ = 0.0f;
    ComPtr<ID2D1StrokeStyle> stroke_style_;
    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<ID2D1RenderTarget> render_target_;
    ComPtr<ID2D1SolidColorBrush> fill_brush_;
    ComPtr<ID2D1SolidColorBrush> outline_brush_;
    UnderlineDrawCB underline_draw_cb_;
};


TextRendererDirectWrite::TextRendererDirectWrite(Context& context, FontProvider& font_provider)
    : log_(GetContextLogger(context)), font_provider_(font_provider) {
    assert(font_provider.GetType() == FontProviderType::kDirectWrite);
}

TextRendererDirectWrite::~TextRendererDirectWrite() = default;

bool TextRendererDirectWrite::Initialize() {
    auto& provider = static_cast<FontProviderDirectWrite&>(font_provider_);
    if (provider.GetType() != FontProviderType::kDirectWrite) {
        log_->e("TextRendererDirectWrite: Font provider must be FontProviderDirectWrite");
        return false;
    }

    // Retrieve IDWriteFactory from FontProviderDirectWrite
    dwrite_factory_ = provider.GetDWriteFactory();
    if (!dwrite_factory_) {
        log_->e("TextRendererDirectWrite: FontProviderDirectWrite::GetDWriteFactory() returns nullptr");
        return false;
    }

    // Create IWICImagingFactory
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&wic_factory_));
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: CoCreateInstance for CLSID_WICImagingFactory failed");
        return false;
    }

    // Create ID2D1Factory
    D2D1_FACTORY_OPTIONS d2d1_options = {D2D1_DEBUG_LEVEL_ERROR};
#ifndef NDEBUG
    d2d1_options.debugLevel = D2D1_DEBUG_LEVEL_INFORMATION;
#endif
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, d2d1_options, d2d_factory_.GetAddressOf());
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: D2D1CreateFactory() failed");
        return false;
    }

    // Create d2d stroke style for outline stroke
    hr = d2d_factory_->CreateStrokeStyle(
        D2D1::StrokeStyleProperties(
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_ROUND,
            D2D1_CAP_STYLE_ROUND,
            D2D1_LINE_JOIN_ROUND,
            10.0f,
            D2D1_DASH_STYLE_SOLID,
            0.0f),
        nullptr,
        0,
        &stroke_style_);
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: ID2D1Factory::CreateStrokeStyle() failed");
        return false;
    }

    return true;
}

void TextRendererDirectWrite::SetLanguage(uint32_t iso6392_language_code) {
    iso6392_language_code_ = iso6392_language_code;
}

bool TextRendererDirectWrite::SetFontFamily(const std::vector<std::string>& font_family) {
    if (font_family.empty()) {
        return false;
    }

    font_family_ = font_family;
    return false;
}

struct TextRenderContextPrivateDirectWrite : public TextRenderContext::ContextPrivate {
public:
    TextRenderContextPrivateDirectWrite() = default;
    ~TextRenderContextPrivateDirectWrite() override = default;
public:
    ComPtr<IWICBitmap> wic_bitmap;
    ComPtr<ID2D1RenderTarget> d2d_render_target;
};

auto TextRendererDirectWrite::BeginDraw(Bitmap& target_bmp) -> TextRenderContext {
    auto priv = std::make_unique<TextRenderContextPrivateDirectWrite>();
    // Create WIC bitmap
    HRESULT hr = wic_factory_->CreateBitmap(static_cast<UINT>(target_bmp.width()),
                                            static_cast<UINT>(target_bmp.height()),
                                            GUID_WICPixelFormat32bppPRGBA,
                                            WICBitmapCreateCacheOption::WICBitmapCacheOnLoad,
                                            &priv->wic_bitmap);
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: Allocate IWICBitmap failed");
        return TextRenderContext(target_bmp);
    }

    // Create WIC-target Direct2D render target
    priv->d2d_render_target = CreateWICRenderTarget(priv->wic_bitmap.Get());
    if (!priv->d2d_render_target) {
        log_->e("TextRendererDirectWrite: Create WIC ID2D1RenderTarget failed");
        return TextRenderContext(target_bmp);
    }

    priv->d2d_render_target->BeginDraw();
    priv->d2d_render_target->Clear();
    priv->d2d_render_target->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    priv->d2d_render_target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);

    return TextRenderContext{target_bmp, std::move(priv)};
}

void TextRendererDirectWrite::EndDraw(TextRenderContext& context) {
    auto priv = static_cast<TextRenderContextPrivateDirectWrite*>(context.GetPrivate());

    HRESULT hr = priv->d2d_render_target->EndDraw();
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: ID2D1RenderTarget::EndDraw() returned error");
    }
    priv->d2d_render_target.Reset();

    bool result = BlendWICBitmapToBitmap(priv->wic_bitmap.Get(), context.GetBitmap(), 0, 0);
    if (!result) {
        log_->e("TextRendererDirectWrite: BlendWICBitmapToBitmap() failed");
    }
    priv->wic_bitmap.Reset();
}

auto TextRendererDirectWrite::DrawChar(TextRenderContext& render_ctx, int target_x, int target_y,
                                       uint32_t ucs4, CharStyle style, ColorRGBA color, ColorRGBA stroke_color,
                                       float stroke_width, int char_width, int char_height,
                                       std::optional<UnderlineInfo> underline_info,
                                       TextRenderFallbackPolicy fallback_policy) -> TextRenderStatus {
    if (!render_ctx.GetPrivate()) {
        log_->e("TextRendererDirectWrite: Invalid TextRenderContext, BeginDraw() failed or not called");
        return TextRenderStatus::kOtherError;
    }

    assert(char_height > 0);
    if (stroke_width < 0.0f) {
        stroke_width = 0.0f;
    }

    // Handle space characters
    if (ucs4 == 0x0009 || ucs4 == 0x0020 || ucs4 == 0x00A0 || ucs4 == 0x1680 ||
        ucs4 == 0x3000 || ucs4 == 0x202F || ucs4 == 0x205F || (ucs4 >= 0x2000 && ucs4 <= 0x200A)) {
        return TextRenderStatus::kOK;
    }

    // Load main font if not loaded
    if (!main_faceinfo_) {
        auto result = LoadDWriteFont();
        if (result.is_err()) {
            log_->e("TextRendererDirectWrite: Cannot find valid font");
            return FontProviderErrorToStatus(result.error());
        }
        auto& pair = result.value();
        main_faceinfo_ = std::move(pair.first);
        main_face_index_ = pair.second;
    }

    // Setup IDWriteTextFormat for main font
    if (!main_text_format_ || char_height != main_text_format_pixel_height_) {
        main_text_format_pixel_height_ = char_height;
        main_text_format_ = CreateDWriteTextFormat(main_faceinfo_.value(), char_height);
        if (!main_text_format_) {
            log_->e("TextRendererDirectWrite: Create IDWriteTextFormat failed");
            return TextRenderStatus::kOtherError;
        }
    }

    FontfaceInfo* face_info = &main_faceinfo_.value();
    IDWriteTextFormat* text_format = main_text_format_.Get();

    // If codepoint was not found in main font, load fallback font
    if (!FontfaceHasCharacter(main_faceinfo_.value(), ucs4)) {
        log_->w("TextRendererDirectWrite: Main font %s doesn't contain U+%04X",
                main_faceinfo_.value().family_name.c_str(), ucs4);

        if (fallback_policy == TextRenderFallbackPolicy::kFailOnCodePointNotFound) {
            return TextRenderStatus::kCodePointNotFound;
        }

        if (main_face_index_ + 1 >= font_family_.size()) {
            // Fallback fonts not available
            return TextRenderStatus::kCodePointNotFound;
        }

        bool reset_fallback_text_format = false;
        // Check fallback faceinfo
        if (!fallback_faceinfo_ || !FontfaceHasCharacter(fallback_faceinfo_.value(), ucs4)) {
            // Fallback font not loaded, or doesn't contain required codepoint
            auto result = LoadDWriteFont(ucs4, main_face_index_ + 1);
            if (result.is_err()) {
                log_->e("TextRendererDirectWrite: Cannot find available fallback font for U+%04X", ucs4);
                return FontProviderErrorToStatus(result.error());
            }
            auto& pair = result.value();
            fallback_faceinfo_ = std::move(pair.first);
            reset_fallback_text_format = true;
        }

        // Setup IDWriteTextFormat for fallback font
        if (!fallback_text_format_ ||
            char_height != fallback_text_format_pixel_height_ ||
            reset_fallback_text_format) {
            fallback_text_format_pixel_height_ = char_height;
            fallback_text_format_ = CreateDWriteTextFormat(fallback_faceinfo_.value(), char_height);
            if (!fallback_text_format_) {
                log_->e("TextRendererDirectWrite: Create fallback IDWriteTextFormat failed");
                return TextRenderStatus::kOtherError;
            }
        }

        face_info = &fallback_faceinfo_.value();
        text_format = fallback_text_format_.Get();
    }

    std::u16string wide_char;
    utf::UTF16AppendCodePoint(wide_char, ucs4);

    // Create DirectWrite TextLayout
    ComPtr<IDWriteTextLayout> text_layout;
    HRESULT hr = dwrite_factory_->CreateTextLayout(reinterpret_cast<WCHAR*>(wide_char.data()),
                                                   static_cast<UINT32>(wide_char.length()),
                                                   text_format, 16384.0f, 16384.0f, &text_layout);
    if (FAILED(hr) || !text_layout) {
        log_->e("TextRendererDirectWrite: Create IDWriteTextLayout failed");
        return TextRenderStatus::kOtherError;
    }

    // Set underline property if required
    if (style & CharStyle::kCharStyleUnderline) {
        text_layout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{0, 1});
    }

    // Get font's metrics
    auto face_info_priv = static_cast<FontfaceInfoPrivateDirectWrite*>(face_info->provider_priv.get());
    DWRITE_FONT_METRICS font_metrics = {0};
    face_info_priv->font->GetMetrics(&font_metrics);
    int ascent = MulDiv(font_metrics.ascent, char_height, font_metrics.designUnitsPerEm);
    [[maybe_unused]]
    int descent = MulDiv(font_metrics.descent, char_height, font_metrics.designUnitsPerEm);

    // Calculate horizontal scale factor
    float horizontal_scale = 1.0f;
    if (char_width != char_height) {
        horizontal_scale = static_cast<float>(char_width) / static_cast<float>(char_height);
    }

    // Calculate reserve spaces for outline stroke
    int margin_x = 0;
    int margin_y = 0;
    if (style & CharStyle::kCharStyleStroke && stroke_width > 0.0f) {
        margin_x = margin_y = static_cast<int>(ceilf(stroke_width));
    }

    // Get rendered text's width & height
    DWRITE_TEXT_METRICS metrics = {0};
    hr = text_layout->GetMetrics(&metrics);
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: GetMetrics() failed");
        return TextRenderStatus::kOtherError;
    }

    // Calculate charbox size
    int charbox_width = static_cast<int>(ceilf((metrics.width + (float)margin_x * 2) * horizontal_scale));
    int charbox_height = static_cast<int>(ceilf(metrics.height)) + margin_y * 2;
    if (charbox_width == 0) {
        charbox_width = static_cast<int>(static_cast<float>(charbox_height) * horizontal_scale);
    }

    // Adjust x coordinate for reserve spaces
    target_x -= margin_x;

    // Adjust target_y based on actual text bitmap height
    int y_adjust = (char_height - charbox_height) / 2;
    target_y += y_adjust;

    auto render_ctx_priv = static_cast<TextRenderContextPrivateDirectWrite*>(render_ctx.GetPrivate());
    Bitmap& target_bmp = render_ctx.GetBitmap();
    ID2D1RenderTarget* render_target = render_ctx_priv->d2d_render_target.Get();

    auto underline_callback = [&](const DWRITE_UNDERLINE* underline) -> void {
        if (!underline_info)
            return;

        int underline_y = target_y + ascent + static_cast<int>(underline->offset);
        Rect underline_rect(underline_info->start_x,
                            underline_y,
                            underline_info->start_x + underline_info->width,
                            underline_y + 1);

        int half_thickness = static_cast<int>(ceilf(underline->thickness / 2));

        underline_rect.top -= half_thickness - 1;
        underline_rect.bottom += half_thickness;

        Canvas canvas(target_bmp);
        canvas.DrawRect(color, underline_rect);
    };

    ComPtr<ID2D1SolidColorBrush> fill_brush;
    render_target->CreateSolidColorBrush(RGBAToD2DColor(color), &fill_brush);

    ComPtr<ID2D1SolidColorBrush> outline_brush;
    render_target->CreateSolidColorBrush(RGBAToD2DColor(stroke_color), &outline_brush);

    ComPtr<OutlineTextRenderer> outline_text_renderer(new OutlineTextRenderer(
        horizontal_scale, 1.0f, style & CharStyle::kCharStyleStroke, stroke_width * 2,
        stroke_style_, d2d_factory_, render_target, fill_brush, outline_brush, underline_callback));

    text_layout->Draw(nullptr,
                      outline_text_renderer.Get(),
                      static_cast<float>(target_x + margin_x),
                      static_cast<float>(target_y + margin_y));

    return TextRenderStatus::kOK;
}

auto TextRendererDirectWrite::LoadDWriteFont(std::optional<uint32_t> codepoint,
                                             std::optional<size_t> begin_index)
    -> Result<std::pair<FontfaceInfo, size_t>, FontProviderError> {
    if (begin_index && begin_index.value() >= font_family_.size()) {
        return Err(FontProviderError::kFontNotFound);
    }

    // begin_index is optional
    size_t font_index = begin_index.value_or(0);

    const std::string& font_name = font_family_[font_index];
    auto result = font_provider_.GetFontFace(font_name, codepoint);

    while (result.is_err() && font_index + 1 < font_family_.size()) {
        // Find next suitable font
        font_index++;
        result = font_provider_.GetFontFace(font_family_[font_index], codepoint);
    }
    if (result.is_err()) {
        // Not found, return Err Result
        return Err(result.error());
    }

    FontfaceInfo& info = result.value();
    if (info.provider_type != FontProviderType::kDirectWrite) {
        log_->e("TextRendererDirectWrite: Font provider must be FontProviderDirectWrite");
        return Err(FontProviderError::kOtherError);
    }

    return Ok(std::make_pair(std::move(info), font_index));
}

static const wchar_t* ISO6392ToWindowsLocaleName(uint32_t iso6392_language_code) {
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

auto TextRendererDirectWrite::CreateDWriteTextFormat(FontfaceInfo& face_info, int font_size)
    -> ComPtr<IDWriteTextFormat> {
    ComPtr<IDWriteTextFormat> text_format;
    std::wstring font_family_name = wchar::UTF8ToWideString(face_info.family_name);

    HRESULT hr = dwrite_factory_->CreateTextFormat(font_family_name.c_str(),
                                                   nullptr,
                                                   DWRITE_FONT_WEIGHT_NORMAL,
                                                   DWRITE_FONT_STYLE_NORMAL,
                                                   DWRITE_FONT_STRETCH_NORMAL,
                                                   static_cast<float>(font_size),
                                                   ISO6392ToWindowsLocaleName(iso6392_language_code_),
                                                   &text_format);
    if (FAILED(hr) || !text_format) {
        log_->e("TextRendererDirectWrite: IDWriteFactory::CreateTextFormat() failed");
        return nullptr;
    }

    text_format->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
    text_format->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);

    return text_format;
}

auto TextRendererDirectWrite::CreateWICRenderTarget(IWICBitmap* target) -> ComPtr<ID2D1RenderTarget> {
    D2D1_RENDER_TARGET_PROPERTIES properties;
    properties.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
    properties.pixelFormat.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_PREMULTIPLIED;
    properties.dpiX = 96.0f;
    properties.dpiY = 96.0f;
    properties.usage = D2D1_RENDER_TARGET_USAGE_FORCE_BITMAP_REMOTING;
    properties.minLevel = D2D1_FEATURE_LEVEL_DEFAULT;

    ComPtr<ID2D1RenderTarget> render_target;
    HRESULT hr = d2d_factory_->CreateWicBitmapRenderTarget(target, properties, &render_target);
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: CreateWicBitmapRenderTarget() failed");
    }

    return render_target;
}

bool TextRendererDirectWrite::BlendWICBitmapToBitmap(IWICBitmap* wic_bitmap,
                                                     Bitmap& target_bmp,
                                                     int target_x, int target_y) {
    uint32_t width = 0;
    uint32_t height = 0;
    wic_bitmap->GetSize(&width, &height);

    WICRect lock_rect = {0, 0, static_cast<int>(width), static_cast<int>(height)};

    ComPtr<IWICBitmapLock> lock;
    HRESULT hr = wic_bitmap->Lock(&lock_rect, WICBitmapLockRead, &lock);
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: IWICBitmap::Lock() failed");
        return false;
    }

    uint32_t stride = 0;
    lock->GetStride(&stride);

    uint32_t buffer_size = 0;
    uint8_t* buffer = nullptr;
    hr = lock->GetDataPointer(&buffer_size, &buffer);
    if (FAILED(hr) || !buffer) {
        log_->e("TextRendererDirectWrite: IWICBitmapLock::GetDataPointer() failed");
        return false;
    }

    Rect rect{target_x, target_y, target_x + (int)width, target_y + (int)height};
    Rect clipped = Rect::ClipRect(target_bmp.GetRect(), rect);
    int clip_x_offset = clipped.left - target_x;
    int clip_y_offset = clipped.top - target_y;
    auto line_width = static_cast<size_t>(clipped.width());

    for (int y = clipped.top; y < clipped.bottom; y++) {
        ColorRGBA* dest_begin = target_bmp.GetPixelAt(clipped.left, y);
        int src_begin_offset = (clip_y_offset + y - clipped.top) * (int)stride + clip_x_offset * 4;
        auto src_begin = reinterpret_cast<const ColorRGBA*>(buffer + src_begin_offset);
        alphablend::BlendLine_PremultipliedSrc(dest_begin, src_begin, line_width);
    }

    return true;
}

D2D1_COLOR_F TextRendererDirectWrite::RGBAToD2DColor(ColorRGBA color) {
    return D2D1::ColorF(static_cast<float>(color.r) / 255.0f,
                        static_cast<float>(color.g) / 255.0f,
                        static_cast<float>(color.b) / 255.0f,
                        static_cast<float>(color.a) / 255.0f);
}

bool TextRendererDirectWrite::FontfaceHasCharacter(FontfaceInfo& fontface, uint32_t ucs4) {
    auto face_info_priv = static_cast<FontfaceInfoPrivateDirectWrite*>(fontface.provider_priv.get());

    BOOL ucs4_exists = FALSE;
    HRESULT hr = face_info_priv->font->HasCharacter(ucs4, &ucs4_exists);

    return SUCCEEDED(hr) && ucs4_exists;
}

}  // namespace aribcaption
