#pragma once
#include <photoastra/io/ImageLoader.h>
#include <photoastra/io/ImageWriter.h>

namespace photoastra::io {
inline constexpr qint64 maxProjectPixelBytes = 512LL * 1024 * 1024;
inline constexpr int maxProjectLayers = 256;
[[nodiscard]] LoadResult loadProject(const QString& path, const std::atomic_bool& cancelled);
[[nodiscard]] ExportResult saveProject(const core::Document& document, const QString& path,
                                       const std::atomic_bool& cancelled);
}
