#include <photoastra/core/RasterImage.h>
#include <stdexcept>
#include <utility>

namespace photoastra::core {
RasterImage::RasterImage(ImageExtent extent, std::vector<std::uint8_t> pixels)
    : extent_(extent), pixels_(std::move(pixels))
{
    const auto count = static_cast<std::uint64_t>(extent.width) * extent.height;
    if (count == 0 || count > maxPixels || pixels_.size() != count * 4) {
        throw std::invalid_argument("Invalid raster extent or pixel buffer size");
    }
}
}
