#include <photoastra/io/ImageLoader.h>
#include <photoastra/core/RasterImage.h>
#include <QColorSpace>
#include <QFile>
#include <QFileInfo>
#include <QImageReader>
#include <cstring>
#include <exception>

namespace photoastra::io {
LoadResult loadImage(const QString& path, const std::atomic_bool& cancelled)
{
    const auto stopped = [&] { return cancelled.load(std::memory_order_relaxed); };
    if (stopped()) return {{}, {}, true};
    try {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return {{}, QStringLiteral("No se pudo abrir: %1").arg(file.errorString())};
        if (file.size() > 128LL * 1024 * 1024) return {{}, QStringLiteral("El archivo supera el límite inicial de 128 MiB.")};
        QImageReader reader(&file);
        reader.setDecideFormatFromContent(true);
        reader.setAutoTransform(true);
        const auto format = reader.format().toLower();
        if (format != "png" && format != "jpeg" && format != "jpg") {
            return {{}, QStringLiteral("Formato no compatible. Selecciona una imagen PNG o JPEG.")};
        }
        const auto size = reader.size();
        const auto allowed = [](QSize value) {
            return value.width() > 0 && value.height() > 0 &&
                static_cast<std::uint64_t>(value.width()) * static_cast<std::uint64_t>(value.height()) <= core::RasterImage::maxPixels;
        };
        if (!allowed(size)) return {{}, QStringLiteral("Dimensiones inválidas o superiores al límite inicial de 33.554.432 píxeles.")};
        auto decoded = reader.read();
        if (stopped()) return {{}, {}, true};
        if (decoded.isNull()) return {{}, QStringLiteral("No se pudo decodificar la imagen: %1").arg(reader.errorString())};
        if (!allowed(decoded.size())) return {{}, QStringLiteral("La imagen decodificada excede los límites permitidos.")};
        if (!decoded.colorSpace().isValid()) decoded.setColorSpace(QColorSpace::SRgb);
        decoded = decoded.convertedToColorSpace(QColorSpace::SRgb);
        if (stopped()) return {{}, {}, true};
        decoded = decoded.convertToFormat(QImage::Format_RGBA8888_Premultiplied);
        if (decoded.isNull()) return {{}, QStringLiteral("No hay memoria suficiente para convertir la imagen.")};
        const core::ImageExtent extent{static_cast<std::uint32_t>(decoded.width()), static_cast<std::uint32_t>(decoded.height())};
        const auto rowBytes = static_cast<std::size_t>(extent.width) * 4;
        std::vector<std::uint8_t> pixels(rowBytes * extent.height);
        for (std::uint32_t y = 0; y < extent.height; ++y) {
            if (stopped()) return {{}, {}, true};
            std::memcpy(pixels.data() + y * rowBytes, decoded.constScanLine(static_cast<int>(y)), rowBytes);
        }
        auto image = std::make_shared<const core::RasterImage>(extent, std::move(pixels));
        auto document = std::make_shared<const core::Document>(extent, QFileInfo(path).fileName().toStdString(), std::move(image));
        return {std::move(document), {}};
    } catch (const std::exception& error) {
        return {{}, QStringLiteral("Error al importar: %1").arg(QString::fromUtf8(error.what()))};
    }
}
}
