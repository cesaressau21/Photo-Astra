#include <photoastra/core/Viewport.h>
#include <algorithm>
#include <cmath>

namespace photoastra::core {
Point Viewport::imageToView(Point p) const noexcept { return {p.x * zoom_ + offset_.x, p.y * zoom_ + offset_.y}; }
Point Viewport::viewToImage(Point p) const noexcept { return {(p.x - offset_.x) / zoom_, (p.y - offset_.y) / zoom_}; }
void Viewport::zoomAt(double factor, Point anchor) noexcept
{
    if (!std::isfinite(factor) || factor <= 0 || !std::isfinite(anchor.x) || !std::isfinite(anchor.y)) return;
    const auto imagePoint = viewToImage(anchor);
    zoom_ = std::clamp(zoom_ * factor, minZoom, maxZoom);
    offset_ = {anchor.x - imagePoint.x * zoom_, anchor.y - imagePoint.y * zoom_};
}
void Viewport::pan(Point delta) noexcept
{
    if (std::isfinite(delta.x) && std::isfinite(delta.y)) {
        offset_.x = std::clamp(offset_.x + delta.x, -1e9, 1e9);
        offset_.y = std::clamp(offset_.y + delta.y, -1e9, 1e9);
    }
}
void Viewport::fit(ImageExtent image, double width, double height) noexcept
{
    if (image.width == 0 || image.height == 0 || !std::isfinite(width) || !std::isfinite(height) || width <= 0 || height <= 0) return;
    zoom_ = std::clamp(std::min({1.0, std::max(1.0, width - 48) / image.width,
                               std::max(1.0, height - 48) / image.height}), minZoom, maxZoom);
    offset_ = {(width - image.width * zoom_) / 2, (height - image.height * zoom_) / 2};
}
}
