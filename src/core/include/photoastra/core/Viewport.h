#pragma once
#include <photoastra/core/Document.h>

namespace photoastra::core {
struct Point { double x = 0; double y = 0; };
class Viewport final {
public:
    static constexpr double minZoom = 1.0 / 64.0;
    static constexpr double maxZoom = 64.0;
    [[nodiscard]] double zoom() const noexcept { return zoom_; }
    [[nodiscard]] Point offset() const noexcept { return offset_; }
    [[nodiscard]] Point imageToView(Point point) const noexcept;
    [[nodiscard]] Point viewToImage(Point point) const noexcept;
    void zoomAt(double factor, Point anchor) noexcept;
    void pan(Point delta) noexcept;
    void fit(ImageExtent image, double width, double height) noexcept;
private:
    double zoom_ = 1;
    Point offset_;
};
}
