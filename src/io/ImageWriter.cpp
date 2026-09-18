#include <photoastra/io/ImageWriter.h>
#include <QColorSpace>
#include <QImageWriter>
#include <QPainter>
#include <QSaveFile>

namespace photoastra::io {
ExportResult writeImage(const QImage& image, const QString& path, ExportFormat format,
                        const std::atomic_bool& cancelled)
{
    if (cancelled.load(std::memory_order_relaxed)) return {{}, true, false};
    if (image.isNull() || path.isEmpty() ||
        (format != ExportFormat::Png && format != ExportFormat::Jpeg))
        return {QStringLiteral("Imagen, ruta o formato de exportación no válido.")};
    QImage output = image;
    if (format == ExportFormat::Jpeg) {
        output = QImage(image.size(), QImage::Format_RGB32);
        if (output.isNull()) return {QStringLiteral("Memoria insuficiente para exportar JPEG.")};
        output.setColorSpace(QColorSpace::SRgb);
        output.fill(Qt::white);
        QPainter painter(&output);
        painter.drawImage(0, 0, image);
    }
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return {file.errorString()};
    QImageWriter writer(&file, format == ExportFormat::Png ? "png" : "jpeg");
    if (format == ExportFormat::Jpeg) writer.setQuality(95);
    if (!writer.write(output)) return {writer.errorString()};
    if (cancelled.load(std::memory_order_relaxed)) {
        file.cancelWriting();
        return {{}, true, false};
    }
    if (!file.commit()) return {file.errorString()};
    return {{}, false, true};
}
}
