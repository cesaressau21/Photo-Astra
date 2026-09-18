#include <photoastra/application/ImageExport.h>
#include <photoastra/core/RasterImage.h>
#include <photoastra/render/Renderer.h>
#include <QColorSpace>
#include <exception>

namespace photoastra::application {
io::ExportResult exportDocument(const core::Document& document, render::Renderer& renderer,
    const QString& path, io::ExportFormat format, const std::atomic_bool& cancelled)
{
    try {
        if (cancelled.load(std::memory_order_relaxed)) return {{}, true, false};
        const auto extent = document.extent();
        if (static_cast<std::uint64_t>(extent.width) * extent.height > core::RasterImage::maxPixels)
            return {QStringLiteral("El documento supera el límite de 33.554.432 píxeles de esta versión.")};
        QImage image(static_cast<int>(extent.width), static_cast<int>(extent.height), QImage::Format_RGBA8888_Premultiplied);
        if (image.isNull()) return {QStringLiteral("Memoria insuficiente para exportar.")};
        image.setColorSpace(QColorSpace::SRgb);
        if (!renderer.composeRaster(document, {image.width(), image.height(),
            static_cast<std::size_t>(image.bytesPerLine()), {image.bits(), static_cast<std::size_t>(image.sizeInBytes())}, 1}))
            return {QStringLiteral("No se pudo componer el documento para exportar. ") +
                QString::fromUtf8(renderer.lastError().data(), static_cast<qsizetype>(renderer.lastError().size()))};
        return io::writeImage(image, path, format, cancelled);
    } catch (const std::exception& error) {
        return {QString::fromUtf8(error.what())};
    }
}
}
