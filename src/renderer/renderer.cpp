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

#include "aribcaption/renderer.hpp"
#include "renderer/renderer_impl.hpp"

namespace aribcaption {

Renderer::Renderer(Context& context) : pimpl_(std::make_unique<internal::RendererImpl>(context)) {}

Renderer::~Renderer() = default;

Renderer::Renderer(Renderer&&) noexcept = default;

Renderer& Renderer::operator=(Renderer&&) noexcept = default;

bool Renderer::Initialize(CaptionType caption_type,
                          FontProviderType font_provider_type,
                          TextRendererType text_renderer_type) {
    return pimpl_->Initialize(caption_type, font_provider_type, text_renderer_type);
}

void Renderer::SetStrokeWidth(float dots) {
    pimpl_->SetStrokeWidth(dots);
}

void Renderer::SetReplaceDRCS(bool replace) {
    pimpl_->SetReplaceDRCS(replace);
}

void Renderer::SetForceStrokeText(bool force_stroke) {
    pimpl_->SetForceStrokeText(force_stroke);
}

void Renderer::SetForceNoRuby(bool force_no_ruby) {
    pimpl_->SetForceNoRuby(force_no_ruby);
}

void Renderer::SetForceNoBackground(bool force_no_background) {
    pimpl_->SetForceNoBackground(force_no_background);
}

void Renderer::SetMergeRegionImages(bool merge) {
    pimpl_->SetMergeRegionImages(merge);
}

bool Renderer::SetDefaultFontFamily(const std::vector<std::string>& font_family, bool force_default) {
    return pimpl_->SetDefaultFontFamily(font_family, force_default);
}

bool Renderer::SetLanguageSpecificFontFamily(uint32_t language_code, const std::vector<std::string>& font_family) {
    return pimpl_->SetLanguageSpecificFontFamily(language_code, font_family);
}

bool Renderer::SetFrameSize(int frame_width, int frame_height) {
    return pimpl_->SetFrameSize(frame_width, frame_height);
}

bool Renderer::SetMargins(int top, int bottom, int left, int right) {
    return pimpl_->SetMargins(top, bottom, left, right);
}

void Renderer::SetStoragePolicy(CaptionStoragePolicy policy, std::optional<size_t> upper_limit) {
    pimpl_->SetStoragePolicy(policy, upper_limit);
}

bool Renderer::AppendCaption(const Caption& caption) {
    return pimpl_->AppendCaption(caption);
}

bool Renderer::AppendCaption(Caption&& caption) {
    return pimpl_->AppendCaption(std::move(caption));
}

RenderStatus Renderer::TryRender(int64_t pts) {
    return pimpl_->TryRender(pts);
}

RenderStatus Renderer::Render(int64_t pts, RenderResult& out_result) {
    return pimpl_->Render(pts, out_result);
}

void Renderer::Flush() {
    pimpl_->Flush();
}

}  // namespace aribcaption
