#pragma once
#include <photoastra/core/Document.h>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace photoastra::core {
// Immutable sRGB RGBA8 pixels with premultiplied alpha and tightly packed rows.
class RasterImage final {
public:
    static constexpr std::uint64_t maxPixels = 32ULL * 1024 * 1024;
    RasterImage(ImageExtent extent, std::vector<std::uint8_t> pixels);
    [[nodiscard]] ImageExtent extent() const noexcept { return extent_; }
    [[nodiscard]] std::size_t rowBytes() const noexcept { return static_cast<std::size_t>(extent_.width) * 4; }
    [[nodiscard]] std::span<const std::uint8_t> pixels() const noexcept { return pixels_; }
private:
    ImageExtent extent_;
    std::vector<std::uint8_t> pixels_;
};
}
