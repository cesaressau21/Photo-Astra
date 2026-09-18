#pragma once

#include <string_view>
#include <photoastra/core/Document.h>
#include <photoastra/core/Viewport.h>
#include <span>
#include <cstdint>

namespace photoastra::render {

struct RendererInfo {
    std::string_view name;
    bool available;
    bool gpuAccelerated;
};

struct RasterTarget {
    int width;
    int height;
    std::size_t rowBytes;
    std::span<std::uint8_t> pixels;
    double devicePixelRatio = 1;
};
struct OpenGlTarget {
    int width;
    int height;
    unsigned int framebuffer;
    int samples;
    int stencilBits;
    double devicePixelRatio = 1;
};
using GlProc = void (*)();
using GlResolver = GlProc (*)(void*, const char*);

// Targets are borrowed for the duration of a call. GL calls require the owning context current.
class Renderer {
public:
    virtual ~Renderer() = default;
    virtual std::string_view lastError() const noexcept { return {}; }
    [[nodiscard]] virtual RendererInfo info() const noexcept = 0;
    virtual bool renderRaster(const core::Document&, const core::Viewport&, RasterTarget) = 0;
    // Full document, native resolution, transparent background; no viewport decoration.
    virtual bool composeRaster(const core::Document&, RasterTarget) { return false; }
    virtual bool initializeOpenGl(void*, GlResolver) { return false; }
    virtual bool renderOpenGl(const core::Document&, const core::Viewport&, OpenGlTarget) { return false; }
    virtual void releaseOpenGl(bool /*contextCurrent*/) {}
};

class UnavailableRenderer final : public Renderer {
public:
    [[nodiscard]] RendererInfo info() const noexcept override;
    bool renderRaster(const core::Document&, const core::Viewport&, RasterTarget) override { return false; }
};

} // namespace photoastra::render
