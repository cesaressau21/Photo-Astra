#include <photoastra/core/LayerMoveGesture.h>
#include <photoastra/core/RasterImage.h>
#include <algorithm>
#include <cmath>

namespace photoastra::core {
bool LayerMoveGesture::begin(std::shared_ptr<const Document> document, LayerId id, Point anchor)
{
    original_.reset(); preview_.reset();
    if (!document || !std::isfinite(anchor.x) || !std::isfinite(anchor.y)) return false;
    for (const auto& layer : document->layers()) {
        if (layer.id != id || !layer.image || !layer.visible || layer.opacity == 0) continue;
        const auto size = layer.image->extent();
        const double x = anchor.x - layer.position.x, y = anchor.y - layer.position.y;
        if (x < 0 || y < 0 || x >= size.width * layer.scale.x || y >= size.height * layer.scale.y || anchor.x < 0 || anchor.y < 0 ||
            anchor.x >= document->extent().width || anchor.y >= document->extent().height) return false;
        original_ = std::move(document); preview_ = original_;
        id_ = id; anchor_ = anchor; initial_ = position_ = layer.position;
        return true;
    }
    return false;
}
std::shared_ptr<const Document> LayerMoveGesture::update(Point point)
{
    if (!original_ || !std::isfinite(point.x) || !std::isfinite(point.y)) return preview_;
    const auto coordinate = [](double value) {
        return static_cast<std::int32_t>(std::round(std::clamp(value, -double(maxLayerOffset), double(maxLayerOffset))));
    };
    const LayerPosition next{coordinate(initial_.x + point.x - anchor_.x), coordinate(initial_.y + point.y - anchor_.y)};
    if (next == position_) return preview_;
    auto layers = original_->layers();
    for (auto& layer : layers) if (layer.id == id_) layer.position = next;
    preview_ = std::make_shared<const Document>(original_->withLayers(std::move(layers)));
    position_ = next;
    return preview_;
}
std::optional<SetPosition> LayerMoveGesture::command() const
{
    if (!original_ || initial_ == position_) return {};
    return SetPosition{id_, position_};
}
}
