#pragma once

#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <photoastra/core/Effect.h>

namespace photoastra::core {
class RasterImage;
enum class BlendMode { Normal, Multiply, Screen };
using LayerId = std::uint64_t;
inline constexpr std::int32_t maxLayerOffset = 1000000;
struct LayerPosition {
    std::int32_t x = 0;
    std::int32_t y = 0;
    bool operator==(const LayerPosition&) const = default;
};
inline constexpr double minLayerScale = 1.0 / 64.0;
inline constexpr double maxLayerScale = 64.0;
struct LayerScale {
    double x = 1;
    double y = 1;
    bool operator==(const LayerScale&) const = default;
};

// Bottom-to-top ordering. A null image is a transparent raster without allocation.
struct RasterLayer {
    LayerId id;
    std::string name;
    std::shared_ptr<const RasterImage> image;
    float opacity = 1.0F;
    bool visible = true;
    EffectStack effects{};
    LayerPosition position{};
    LayerScale scale{};
    BlendMode blendMode = BlendMode::Normal;
};

struct ImageExtent {
    std::uint32_t width;
    std::uint32_t height;
    bool operator==(const ImageExtent&) const = default;
};

// Document snapshots share immutable source pixels; view state belongs to Canvas.
class Document final {
public:
    explicit Document(ImageExtent extent, std::string title,
                      std::shared_ptr<const RasterImage> image = {});

    [[nodiscard]] ImageExtent extent() const noexcept;
    [[nodiscard]] const std::string& title() const noexcept;
    [[nodiscard]] const std::vector<RasterLayer>& layers() const noexcept { return layers_; }
    [[nodiscard]] Document withLayers(std::vector<RasterLayer> layers) const;

private:
    ImageExtent extent_;
    std::string title_;
    std::vector<RasterLayer> layers_;
};

} // namespace photoastra::core
