#include <photoastra/render/SkiaRenderer.h>
#include "SkiaCompositor.h"
#include <photoastra/core/RasterImage.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkImage.h>
#include <include/core/SkPaint.h>
#include <include/core/SkShader.h>
#include <include/core/SkSurface.h>
#include <include/gpu/ganesh/GrBackendSurface.h>
#include <include/gpu/ganesh/GrDirectContext.h>
#include <include/gpu/ganesh/SkSurfaceGanesh.h>
#include <include/gpu/ganesh/gl/GrGLAssembleInterface.h>
#include <include/gpu/ganesh/gl/GrGLBackendSurface.h>
#include <include/gpu/ganesh/gl/GrGLDirectContext.h>
#include <array>
#include <algorithm>
#include <cmath>

namespace photoastra::render {
struct SkiaRenderer::Impl {
    sk_sp<GrDirectContext> gpu;
    SkiaCompositor compositor;
    sk_sp<SkShader> checker;

    Impl()
    {
        std::array<std::uint32_t, 16 * 16> pixels{};
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) pixels[static_cast<std::size_t>(y * 16 + x)] =
                ((x / 8 + y / 8) % 2 == 0) ? 0xFFE0E0E0 : 0xFFBDBDBD;
        }
        const auto tile = SkImages::RasterFromPixmapCopy(SkPixmap(
            SkImageInfo::Make(16, 16, kRGBA_8888_SkColorType, kPremul_SkAlphaType), pixels.data(), 16 * 4));
        if (tile) checker = tile->makeShader(SkTileMode::kRepeat, SkTileMode::kRepeat, SkSamplingOptions{});
    }

    bool draw(SkCanvas* canvas, const core::Document& document, const core::Viewport& view, double dpr)
    {
        canvas->clear(SkColorSetRGB(48, 51, 56));
        const auto origin = view.offset();
        const auto extent = document.extent();
        canvas->save();
        canvas->scale(static_cast<float>(dpr), static_cast<float>(dpr));
        const auto bounds = SkRect::MakeXYWH(static_cast<float>(origin.x), static_cast<float>(origin.y),
            static_cast<float>(extent.width * view.zoom()), static_cast<float>(extent.height * view.zoom()));
        SkPaint background;
        background.setColor(SK_ColorWHITE);
        background.setShader(checker);
        canvas->drawRect(bounds, background);
        canvas->translate(static_cast<float>(origin.x), static_cast<float>(origin.y));
        canvas->scale(static_cast<float>(view.zoom()), static_cast<float>(view.zoom()));
        const bool isolate = std::any_of(document.layers().begin(), document.layers().end(), [](const auto& layer) {
            return layer.visible && layer.image && layer.opacity > 0 && layer.blendMode != core::BlendMode::Normal;
        });
        if (isolate) {
            // Blend against document pixels, never against the transparency checkerboard.
            canvas->clipRect(SkRect::MakeWH(static_cast<float>(extent.width), static_cast<float>(extent.height)));
            canvas->saveLayer(nullptr, nullptr);
        }
        const bool success = compositor.draw(*canvas, document);
        if (isolate) canvas->restore();
        canvas->restore();
        return success;
    }
};

SkiaRenderer::SkiaRenderer() : impl_(std::make_unique<Impl>()) {}
SkiaRenderer::~SkiaRenderer() { releaseOpenGl(false); }
std::string_view SkiaRenderer::lastError() const noexcept { return impl_->compositor.error(); }
RendererInfo SkiaRenderer::info() const noexcept
{
    return {impl_->gpu ? "Skia / OpenGL" : "Skia / CPU", true, static_cast<bool>(impl_->gpu)};
}
bool SkiaRenderer::renderRaster(const core::Document& document, const core::Viewport& view, RasterTarget target)
{
    if (target.width <= 0 || target.height <= 0 || !std::isfinite(target.devicePixelRatio) || target.devicePixelRatio <= 0 ||
        target.rowBytes < static_cast<std::size_t>(target.width) * 4 ||
        target.rowBytes > target.pixels.size() / static_cast<std::size_t>(target.height)) return false;
    auto surface = SkSurfaces::WrapPixels(SkImageInfo::Make(target.width, target.height,
        kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB()), target.pixels.data(), target.rowBytes);
    return surface && impl_->draw(surface->getCanvas(), document, view, target.devicePixelRatio);
}
bool SkiaRenderer::composeRaster(const core::Document& document, RasterTarget target)
{
    if (target.width <= 0 || target.height <= 0 || target.devicePixelRatio != 1 ||
        static_cast<std::uint32_t>(target.width) != document.extent().width ||
        static_cast<std::uint32_t>(target.height) != document.extent().height ||
        target.rowBytes < static_cast<std::size_t>(target.width) * 4 ||
        target.rowBytes > target.pixels.size() / static_cast<std::size_t>(target.height)) return false;
    auto surface = SkSurfaces::WrapPixels(SkImageInfo::Make(target.width, target.height,
        kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB()), target.pixels.data(), target.rowBytes);
    if (!surface) return false;
    surface->getCanvas()->clear(SK_ColorTRANSPARENT);
    return impl_->compositor.draw(*surface->getCanvas(), document);
}
bool SkiaRenderer::initializeOpenGl(void* context, GlResolver resolver)
{
    releaseOpenGl(true);
    if (!resolver) return false;
    auto interface = GrGLMakeAssembledInterface(context, resolver);
    if (!interface) return false;
    impl_->gpu = GrDirectContexts::MakeGL(std::move(interface));
    if (impl_->gpu) impl_->gpu->setResourceCacheLimit(128ULL * 1024 * 1024);
    return static_cast<bool>(impl_->gpu);
}
bool SkiaRenderer::renderOpenGl(const core::Document& document, const core::Viewport& view, OpenGlTarget target)
{
    if (!impl_->gpu || target.width <= 0 || target.height <= 0 ||
        !std::isfinite(target.devicePixelRatio) || target.devicePixelRatio <= 0) return false;
    impl_->gpu->resetContext(); // Qt also uses this context when composing its widgets.
    GrGLFramebufferInfo framebuffer{target.framebuffer, 0x8058}; // GL_RGBA8
    auto backend = GrBackendRenderTargets::MakeGL(target.width, target.height, target.samples, target.stencilBits, framebuffer);
    auto surface = SkSurfaces::WrapBackendRenderTarget(impl_->gpu.get(), backend, kBottomLeft_GrSurfaceOrigin,
        kRGBA_8888_SkColorType, SkColorSpace::MakeSRGB(), nullptr);
    if (!surface) return false;
    const bool success = impl_->draw(surface->getCanvas(), document, view, target.devicePixelRatio);
    impl_->gpu->flushAndSubmit(surface.get());
    return success;
}
void SkiaRenderer::releaseOpenGl(bool contextCurrent)
{
    if (impl_->gpu) {
        if (!contextCurrent) impl_->gpu->abandonContext();
        impl_->gpu.reset();
    }
}
}
