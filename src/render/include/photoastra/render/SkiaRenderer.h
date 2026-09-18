#pragma once
#include <photoastra/render/Renderer.h>
#include <memory>

namespace photoastra::render {
class SkiaRenderer final : public Renderer {
public:
    SkiaRenderer();
    ~SkiaRenderer() override;
    std::string_view lastError() const noexcept override;
    [[nodiscard]] RendererInfo info() const noexcept override;
    bool renderRaster(const core::Document&, const core::Viewport&, RasterTarget) override;
    bool composeRaster(const core::Document&, RasterTarget) override;
    bool initializeOpenGl(void* context, GlResolver resolver) override;
    bool renderOpenGl(const core::Document&, const core::Viewport&, OpenGlTarget) override;
    void releaseOpenGl(bool contextCurrent) override;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
}
