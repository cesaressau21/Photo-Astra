#include <photoastra/core/Document.h>
#include <photoastra/core/RasterImage.h>

#include <stdexcept>
#include <utility>
#include <cmath>
#include <unordered_set>

namespace photoastra::core {

Document::Document(ImageExtent extent, std::string title, std::shared_ptr<const RasterImage> image)
    : extent_(extent), title_(std::move(title))
{
    if (extent.width == 0 || extent.height == 0) {
        throw std::invalid_argument("Document dimensions must be positive");
    }
    if (title_.empty()) {
        throw std::invalid_argument("Document title must not be empty");
    }
    if (image && image->extent() != extent_) {
        throw std::invalid_argument("Document and raster dimensions must match");
    }
    if (image) layers_.push_back({1, title_, std::move(image)});
}

Document Document::withLayers(std::vector<RasterLayer> layers) const
{
    std::unordered_set<LayerId> ids;
    for (const auto& layer : layers) {
        validateEffects(layer.effects);
        switch (layer.blendMode) {
        case BlendMode::Normal: case BlendMode::Multiply: case BlendMode::Screen: break;
        default: throw std::invalid_argument("Unsupported blend mode");
        }
        for (const double scale : {layer.scale.x, layer.scale.y})
            if (!std::isfinite(scale) || scale < minLayerScale || scale > maxLayerScale)
                throw std::invalid_argument("Layer scale out of range");
        if (layer.position.x < -maxLayerOffset || layer.position.x > maxLayerOffset ||
            layer.position.y < -maxLayerOffset || layer.position.y > maxLayerOffset)
            throw std::invalid_argument("Layer position out of range");
        if (layer.id == 0 || !ids.insert(layer.id).second || layer.name.empty() ||
            !std::isfinite(layer.opacity) || layer.opacity < 0 || layer.opacity > 1)
            throw std::invalid_argument("Invalid raster layer metadata");
    }
    auto result = *this;
    result.layers_ = std::move(layers);
    return result;
}

ImageExtent Document::extent() const noexcept { return extent_; }
const std::string& Document::title() const noexcept { return title_; }

} // namespace photoastra::core
