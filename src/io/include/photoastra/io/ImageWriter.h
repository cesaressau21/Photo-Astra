#pragma once
#include <QImage>
#include <QString>
#include <atomic>

namespace photoastra::io {
enum class ExportFormat { Png, Jpeg };
struct ExportResult {
    QString error;
    bool cancelled = false;
    bool succeeded = false;
};
// Writes a tagged sRGB premultiplied image atomically; JPEG flattens onto white.
[[nodiscard]] ExportResult writeImage(const QImage& image, const QString& path,
    ExportFormat format, const std::atomic_bool& cancelled);
}
