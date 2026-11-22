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
#include <cstring>
#include <vector>
#include "base/floating_helper.hpp"
#include "base/unicode_helper.hpp"
#include "base/utf_helper.hpp"
#include "base/wchar_helper.hpp"
#include "renderer/alphablend.hpp"
#include "renderer/canvas.hpp"
#include "renderer/font_provider_directwrite.hpp"
#include "renderer/text_renderer_directwrite.hpp"

namespace aribcaption {

static bool GetFontFaceFiles(IDWriteFontFace* face, std::vector<ComPtr<IDWriteFontFile>>& out_files) {
    UINT32 count = 0;
    if (FAILED(face->GetFiles(&count, nullptr)) || count == 0) {
        return false;
    }
    std::vector<IDWriteFontFile*> raw_files(count, nullptr);
    if (FAILED(face->GetFiles(&count, raw_files.data()))) {
        return false;
    }
    out_files.resize(count);
    for (UINT32 i = 0; i < count; i++) {
        out_files[i].Attach(raw_files[i]);
    }
    return true;
}

// IDWriteFontFace3::Equals() requires Windows 10, compare face index, simulations and file identities instead
static bool IsSameFontFace(IDWriteFontFace* a, IDWriteFontFace* b) {
    if (a == b) {
        return true;
    }
    if (!a || !b || a->GetIndex() != b->GetIndex() || a->GetSimulations() != b->GetSimulations()) {
        return false;
    }

    std::vector<ComPtr<IDWriteFontFile>> files_a;
    std::vector<ComPtr<IDWriteFontFile>> files_b;
    if (!GetFontFaceFiles(a, files_a) || !GetFontFaceFiles(b, files_b) || files_a.size() != files_b.size()) {
        return false;
    }

    for (size_t i = 0; i < files_a.size(); i++) {
        ComPtr<IDWriteFontFileLoader> loader_a;
        ComPtr<IDWriteFontFileLoader> loader_b;
        const void* key_a = nullptr;
        const void* key_b = nullptr;
        UINT32 key_size_a = 0;
        UINT32 key_size_b = 0;
        if (FAILED(files_a[i]->GetLoader(&loader_a)) || FAILED(files_b[i]->GetLoader(&loader_b)) ||
            FAILED(files_a[i]->GetReferenceKey(&key_a, &key_size_a)) ||
            FAILED(files_b[i]->GetReferenceKey(&key_b, &key_size_b))) {
            return false;
        }
        if (loader_a.Get() != loader_b.Get() || key_size_a != key_size_b || std::memcmp(key_a, key_b, key_size_a) != 0) {
            return false;
        }
    }
    return true;
}

// Collects glyph run outlines instead of drawing them, so the caller can verify the layout
// used the selected font face before anything reaches the render target
class OutlineTextRenderer : public IDWriteTextRenderer {
public:
    OutlineTextRenderer(float horizontal_scale,
                        float vertical_scale,
                        ComPtr<IDWriteFontFace> expected_fontface,
                        ComPtr<ID2D1Factory> d2d_factory,
                        ComPtr<ID2D1RenderTarget> render_target)
        : horizontal_scale_(horizontal_scale),
          vertical_scale_(vertical_scale),
          expected_fontface_(std::move(expected_fontface)),
          d2d_factory_(std::move(d2d_factory)),
          render_target_(std::move(render_target)) {}

    virtual ~OutlineTextRenderer() = default;

    [[nodiscard]]
    bool HasRejectedGlyphRun() const {
        return rejected_;
    }

    [[nodiscard]]
    const std::vector<ComPtr<ID2D1TransformedGeometry>>& GetGeometries() const {
        return geometries_;
    }

    [[nodiscard]]
    const std::vector<DWRITE_UNDERLINE>& GetUnderlines() const {
        return underlines_;
    }

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
        if (!glyph_run || glyph_run->glyphCount == 0 || !glyph_run->glyphIndices) {
            return S_OK;
        }
        if (!IsSameFontFace(glyph_run->fontFace, expected_fontface_.Get())) {
            rejected_ = true;
            return S_OK;
        }
        for (UINT32 i = 0; i < glyph_run->glyphCount; i++) {
            if (glyph_run->glyphIndices[i] == 0) {
                rejected_ = true;
                return S_OK;
            }
        }

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

        geometries_.push_back(std::move(transformed_geometry));
        return S_OK;
    }

    IFACEMETHOD(DrawUnderline)(void* client_drawing_context,
                               float baseline_origin_x,
                               float baseline_origin_y,
                               DWRITE_UNDERLINE const* underline,
                               IUnknown* client_drawing_effect) {
        underlines_.push_back(*underline);
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
    ComPtr<IDWriteFontFace> expected_fontface_;
    ComPtr<ID2D1Factory> d2d_factory_;
    ComPtr<ID2D1RenderTarget> render_target_;
    bool rejected_ = false;
    std::vector<ComPtr<ID2D1TransformedGeometry>> geometries_;
    std::vector<DWRITE_UNDERLINE> underlines_;
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

    // An empty font fallback stops IDWriteTextLayout from substituting another font (Windows 8.1+)
    ComPtr<IDWriteFactory2> dwrite_factory2;
    if (SUCCEEDED(dwrite_factory_.As(&dwrite_factory2)) && dwrite_factory2) {
        ComPtr<IDWriteFontFallbackBuilder> fallback_builder;
        if (SUCCEEDED(dwrite_factory2->CreateFontFallbackBuilder(&fallback_builder))) {
            fallback_builder->CreateFontFallback(&no_fallback_);
        }
    }

    return true;
}

void TextRendererDirectWrite::SetLanguage(uint32_t iso6392_language_code) {
    if (iso6392_language_code_ != iso6392_language_code) {
        // Font provider resolves font families (e.g. sans-serif) per language,
        // and IDWriteTextFormat carries the locale name. Reset loaded fonts & text formats.
        ResetFontFaces();
    }
    iso6392_language_code_ = iso6392_language_code;
}

bool TextRendererDirectWrite::SetFontFamily(const std::vector<std::string>& font_family) {
    if (font_family.empty()) {
        return false;
    }

    if (!font_family_.empty() && font_family_ != font_family) {
        ResetFontFaces();
    }

    font_family_ = font_family;
    return true;
}

void TextRendererDirectWrite::ResetFontFaces() {
    font_slots_.clear();
    system_fallback_slots_.clear();
    main_slot_index_.reset();
    main_load_error_ = FontProviderError::kFontNotFound;
    missing_codepoints_.clear();
}

void TextRendererDirectWrite::SetReplaceMSZHalfWidthGlyph(bool replace) {
    replace_msz_halfwidth_glyph_ = replace;
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
                                       float stroke_width, int char_width, int char_height, float aspect_ratio,
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

    // Skip space characters
    if (unicode::IsSpaceCharacter(ucs4)) {
        return TextRenderStatus::kOK;
    }

    if (auto err = EnsureMainSlot()) {
        log_->e("TextRendererDirectWrite: Cannot find valid font");
        return FontProviderErrorToStatus(*err);
    }

    std::u16string wide_char;
    utf::UTF16AppendCodePoint(wide_char, ucs4);

    auto try_slot = [&](FontSlot& slot) -> TextRenderStatus {
        return TryDrawCharWithSlot(render_ctx, slot, wide_char, target_x, target_y, ucs4, style, color, stroke_color,
                                   stroke_width, char_width, char_height, aspect_ratio, underline_info);
    };

    FontSlot& main_slot = font_slots_[*main_slot_index_];
    if (FontfaceHasCharacter(main_slot.faceinfo, ucs4)) {
        TextRenderStatus status = try_slot(main_slot);
        if (status != TextRenderStatus::kCodePointNotFound) {
            return status;
        }
    }

    if (fallback_policy == TextRenderFallbackPolicy::kFailOnCodePointNotFound) {
        return TextRenderStatus::kCodePointNotFound;
    }

    if (missing_codepoints_.find(ucs4) != missing_codepoints_.end()) {
        return TextRenderStatus::kCodePointNotFound;
    }

    // Try fallback fonts in configured font-family list
    for (size_t i = *main_slot_index_ + 1; i < font_slots_.size(); i++) {
        FontSlot& slot = font_slots_[i];
        if (slot.state == SlotState::kUnavailable) {
            continue;
        }
        if (slot.state == SlotState::kUnloaded) {
            LoadSlot(i);
            if (slot.state != SlotState::kLoaded) {
                continue;
            }
        }
        if (!FontfaceHasCharacter(slot.faceinfo, ucs4)) {
            continue;
        }

        TextRenderStatus status = try_slot(slot);
        if (status != TextRenderStatus::kCodePointNotFound) {
            return status;
        }
    }

    // Try system fallback, if allowed by fallback policy
    if (fallback_policy == TextRenderFallbackPolicy::kAutoFallback && !unicode::IsPrivateUseArea(ucs4)) {
        for (auto& slot : system_fallback_slots_) {
            if (slot.state != SlotState::kLoaded || !FontfaceHasCharacter(slot.faceinfo, ucs4)) {
                continue;
            }

            TextRenderStatus status = try_slot(slot);
            if (status != TextRenderStatus::kCodePointNotFound) {
                return status;
            }
        }

        auto new_slot = LoadSystemFallbackSlot(ucs4, font_family_[*main_slot_index_]);
        if (new_slot.is_ok()) {
            TextRenderStatus status = try_slot(new_slot.value());
            if (status == TextRenderStatus::kOK) {
                CacheSystemFallbackSlot(std::move(new_slot.value()));
                return TextRenderStatus::kOK;
            }
            if (status != TextRenderStatus::kCodePointNotFound) {
                return status;
            }
        } else if (new_slot.error() != TextRenderStatus::kCodePointNotFound) {
            log_->e("TextRendererDirectWrite: System fallback lookup for U+%04X failed", ucs4);
            return new_slot.error();
        }
    }

    // A font-family-only miss may still be served by system fallback later, so don't cache it,
    // unless it is a PUA codepoint which system fallback never handles
    if (fallback_policy == TextRenderFallbackPolicy::kAutoFallback || unicode::IsPrivateUseArea(ucs4)) {
        missing_codepoints_.insert(ucs4);
        log_->e("TextRendererDirectWrite: Cannot find available fallback font for U+%04X", ucs4);
    }
    return TextRenderStatus::kCodePointNotFound;
}

auto TextRendererDirectWrite::TryDrawCharWithSlot(TextRenderContext& render_ctx, FontSlot& slot,
                                                  std::u16string_view wide_char, int target_x, int target_y,
                                                  uint32_t ucs4, CharStyle style, ColorRGBA color,
                                                  ColorRGBA stroke_color, float stroke_width, int char_width,
                                                  int char_height, float aspect_ratio,
                                                  std::optional<UnderlineInfo> underline_info) -> TextRenderStatus {
    IDWriteTextFormat* text_format = EnsureTextFormat(slot, char_height);
    if (!text_format) {
        log_->e("TextRendererDirectWrite: Create IDWriteTextFormat failed");
        return TextRenderStatus::kOtherError;
    }

    FontfaceInfo* face_info = &slot.faceinfo;
    UINT32 text_length = static_cast<UINT32>(wide_char.length());

    // Create DirectWrite TextLayout
    ComPtr<IDWriteTextLayout> text_layout;
    HRESULT hr = dwrite_factory_->CreateTextLayout(reinterpret_cast<const WCHAR*>(wide_char.data()),
                                                   text_length,
                                                   text_format, 16384.0f, 16384.0f, &text_layout);
    if (FAILED(hr) || !text_layout) {
        log_->e("TextRendererDirectWrite: Create IDWriteTextLayout failed");
        return TextRenderStatus::kOtherError;
    }

    // Set underline property if required
    if (style & CharStyle::kCharStyleUnderline) {
        text_layout->SetUnderline(TRUE, DWRITE_TEXT_RANGE{0, text_length});
    }

    // Get font's metrics
    auto face_info_priv = static_cast<FontfaceInfoPrivateDirectWrite*>(face_info->provider_priv.get());
    DWRITE_FONT_METRICS font_metrics = {0};
    face_info_priv->font->GetMetrics(&font_metrics);
    double em_size = static_cast<double>(char_height) * face_info_priv->scale;
    int ascent = static_cast<int>(std::lround(font_metrics.ascent * em_size / font_metrics.designUnitsPerEm));

    bool is_requesting_halfwidth = floating::AlmostEquals(aspect_ratio, 0.5f, 0.05f);
    bool needless_horizontal_scaling = is_requesting_halfwidth && unicode::IsHalfwidthCharacter(ucs4);

    // Calculate horizontal scale factor
    float horizontal_scale = 1.0f;
    if (char_width != char_height && !needless_horizontal_scaling) {
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

    // Check if the font has a half width glyph for this character.
    if (replace_msz_halfwidth_glyph_ && is_requesting_halfwidth) {
        ComPtr<IDWriteTypography> typography;
        DWRITE_TEXT_METRICS full_width_text_metrics = metrics;
        DWRITE_FONT_FEATURE feature = {DWRITE_FONT_FEATURE_TAG_HALF_WIDTH, 1};
        if (SUCCEEDED(dwrite_factory_->CreateTypography(&typography)) &&
            SUCCEEDED(typography->AddFontFeature(feature)) &&
            SUCCEEDED(text_layout->SetTypography(typography.Get(), DWRITE_TEXT_RANGE{0, text_length}))) {
            hr = text_layout->GetMetrics(&metrics);
            if (FAILED(hr)) {
                log_->e("TextRendererDirectWrite: GetMetrics() failed");
                return TextRenderStatus::kOtherError;
            }
            // Avoid scaling because it has a half width glyph.
            if (full_width_text_metrics.width / 2 == metrics.width) {
                char_width = char_height;
                horizontal_scale = 1.0f;
            } else {
                if (SUCCEEDED(dwrite_factory_->CreateTypography(&typography))) {
                    text_layout->SetTypography(typography.Get(), DWRITE_TEXT_RANGE{0, text_length});
                }
            }
        }
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

    ComPtr<OutlineTextRenderer> outline_text_renderer(new OutlineTextRenderer(
        horizontal_scale, 1.0f, face_info_priv->fontface, d2d_factory_, render_target));

    hr = text_layout->Draw(nullptr,
                           outline_text_renderer.Get(),
                           static_cast<float>(target_x + margin_x),
                           static_cast<float>(target_y + margin_y));
    if (FAILED(hr)) {
        log_->e("TextRendererDirectWrite: IDWriteTextLayout::Draw() failed");
        return TextRenderStatus::kOtherError;
    }

    if (outline_text_renderer->HasRejectedGlyphRun() || outline_text_renderer->GetGeometries().empty()) {
        log_->w("TextRendererDirectWrite: Rejected glyph run for U+%04X from %s",
                ucs4, face_info->family_name.c_str());
        return TextRenderStatus::kCodePointNotFound;
    }

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

    bool draw_outline_stroke = style & CharStyle::kCharStyleStroke;
    for (const auto& geometry : outline_text_renderer->GetGeometries()) {
        if (draw_outline_stroke) {
            render_target->DrawGeometry(geometry.Get(), outline_brush.Get(), stroke_width * 2, stroke_style_.Get());
        }
        render_target->FillGeometry(geometry.Get(), fill_brush.Get());
    }

    for (const DWRITE_UNDERLINE& underline : outline_text_renderer->GetUnderlines()) {
        underline_callback(&underline);
    }

    return TextRenderStatus::kOK;
}

bool TextRendererDirectWrite::IsDuplicateFont(const FontfaceInfo& info) const {
    auto matches_list = [&](const std::vector<FontSlot>& slots) -> bool {
        for (const auto& existing : slots) {
            if (existing.state != SlotState::kLoaded) {
                continue;
            }
            if (!info.filename.empty() && !existing.faceinfo.filename.empty()) {
                if (info.filename == existing.faceinfo.filename && info.face_index == existing.faceinfo.face_index) {
                    return true;
                }
            } else if (!info.postscript_name.empty() && !existing.faceinfo.postscript_name.empty()) {
                if (info.postscript_name == existing.faceinfo.postscript_name) {
                    return true;
                }
            } else if (!info.family_name.empty() &&
                       info.family_name == existing.faceinfo.family_name &&
                       info.face_index == existing.faceinfo.face_index) {
                return true;
            }
        }
        return false;
    };

    return matches_list(font_slots_) || matches_list(system_fallback_slots_);
}

auto TextRendererDirectWrite::LoadSlot(size_t slot_index) -> std::optional<FontProviderError> {
    FontSlot& slot = font_slots_[slot_index];
    if (slot.state != SlotState::kUnloaded) {
        return std::nullopt;
    }

    auto result = font_provider_.GetFontFace(font_family_[slot_index]);
    if (result.is_err()) {
        slot.state = SlotState::kUnavailable;
        return result.error();
    }

    FontfaceInfo& info = result.value();
    if (info.provider_type != FontProviderType::kDirectWrite) {
        log_->e("TextRendererDirectWrite: Font provider must be FontProviderDirectWrite");
        slot.state = SlotState::kUnavailable;
        return FontProviderError::kOtherError;
    }

    auto priv = static_cast<FontfaceInfoPrivateDirectWrite*>(info.provider_priv.get());
    if (!priv || !priv->font || !priv->fontface) {
        slot.state = SlotState::kUnavailable;
        return FontProviderError::kFontNotFound;
    }

    if (IsDuplicateFont(info)) {
        slot.state = SlotState::kUnavailable;
        return std::nullopt;
    }

    slot.faceinfo = std::move(info);
    slot.state = SlotState::kLoaded;
    return std::nullopt;
}

auto TextRendererDirectWrite::EnsureMainSlot() -> std::optional<FontProviderError> {
    if (font_family_.empty()) {
        return FontProviderError::kFontNotFound;
    }

    if (main_slot_index_) {
        return std::nullopt;
    }

    if (font_slots_.empty()) {
        font_slots_.resize(font_family_.size());
    }

    for (size_t i = 0; i < font_slots_.size(); i++) {
        if (font_slots_[i].state == SlotState::kUnavailable) {
            continue;
        }
        if (auto err = LoadSlot(i)) {
            main_load_error_ = *err;
        }
        if (font_slots_[i].state == SlotState::kLoaded) {
            main_slot_index_ = i;
            return std::nullopt;
        }
    }

    return main_load_error_;
}

auto TextRendererDirectWrite::LoadSystemFallbackSlot(uint32_t ucs4, std::string_view base_family_name)
    -> Result<FontSlot, TextRenderStatus> {
    if (unicode::IsPrivateUseArea(ucs4)) {
        return Err(TextRenderStatus::kCodePointNotFound);
    }

    auto result = font_provider_.GetFallbackFontFace(ucs4, base_family_name);
    if (result.is_err()) {
        return Err(result.error() == FontProviderError::kOtherError ? TextRenderStatus::kOtherError
                                                                     : TextRenderStatus::kCodePointNotFound);
    }

    FontfaceInfo& info = result.value();
    if (info.provider_type != FontProviderType::kDirectWrite) {
        return Err(TextRenderStatus::kOtherError);
    }

    auto priv = static_cast<FontfaceInfoPrivateDirectWrite*>(info.provider_priv.get());
    if (!priv || !priv->font || !priv->fontface || !FontfaceHasCharacter(info, ucs4) || IsDuplicateFont(info)) {
        return Err(TextRenderStatus::kCodePointNotFound);
    }

    FontSlot slot;
    slot.faceinfo = std::move(info);
    slot.state = SlotState::kLoaded;
    return Ok(std::move(slot));
}

void TextRendererDirectWrite::CacheSystemFallbackSlot(FontSlot&& slot) {
    static constexpr size_t kMaxSystemFallbackSlots = 16;
    if (system_fallback_slots_.size() >= kMaxSystemFallbackSlots) {
        system_fallback_slots_.erase(system_fallback_slots_.begin());
    }
    system_fallback_slots_.push_back(std::move(slot));
}

auto TextRendererDirectWrite::EnsureTextFormat(FontSlot& slot, int char_height) -> IDWriteTextFormat* {
    if (!slot.text_format || slot.pixel_height != char_height) {
        auto priv = static_cast<FontfaceInfoPrivateDirectWrite*>(slot.faceinfo.provider_priv.get());
        float scale = priv ? priv->scale : 1.0f;
        slot.text_format = CreateDWriteTextFormat(slot.faceinfo, static_cast<float>(char_height) * scale);
        if (!slot.text_format) {
            slot.pixel_height = 0;
            return nullptr;
        }
        slot.pixel_height = char_height;
    }
    return slot.text_format.Get();
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

auto TextRendererDirectWrite::CreateDWriteTextFormat(FontfaceInfo& face_info, float font_size)
    -> ComPtr<IDWriteTextFormat> {
    ComPtr<IDWriteTextFormat> text_format;
    std::wstring font_family_name = wchar::UTF8ToWideString(face_info.family_name);

    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
    // Resolve the family name in the collection the selected font belongs to
    ComPtr<IDWriteFontCollection> font_collection;
    if (auto priv = static_cast<FontfaceInfoPrivateDirectWrite*>(face_info.provider_priv.get());
        priv && priv->font) {
        weight = priv->font->GetWeight();
        style = priv->font->GetStyle();
        stretch = priv->font->GetStretch();

        ComPtr<IDWriteFontFamily> font_family;
        if (SUCCEEDED(priv->font->GetFontFamily(&font_family)) && font_family) {
            font_family->GetFontCollection(&font_collection);
        }
    }

    HRESULT hr = dwrite_factory_->CreateTextFormat(font_family_name.c_str(),
                                                   font_collection.Get(),
                                                   weight,
                                                   style,
                                                   stretch,
                                                   font_size,
                                                   ISO6392ToWindowsLocaleName(iso6392_language_code_),
                                                   &text_format);
    if (FAILED(hr) || !text_format) {
        log_->e("TextRendererDirectWrite: IDWriteFactory::CreateTextFormat() failed");
        text_format.Reset();
        return text_format;
    }

    if (no_fallback_) {
        ComPtr<IDWriteTextFormat1> text_format1;
        if (SUCCEEDED(text_format.As(&text_format1)) && text_format1) {
            text_format1->SetFontFallback(no_fallback_.Get());
        }
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
