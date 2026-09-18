#include <photoastra/render/Renderer.h>

namespace photoastra::render {

RendererInfo UnavailableRenderer::info() const noexcept
{
    return {"Sin backend", false, false};
}

} // namespace photoastra::render
