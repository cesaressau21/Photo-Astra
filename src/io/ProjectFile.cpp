#include <photoastra/io/ProjectFile.h>
#include <photoastra/core/RasterImage.h>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QtEndian>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace photoastra::io {
namespace {
constexpr qint64 maxMetadata = 1024 * 1024;
constexpr qint64 chunkSize = 1024 * 1024;
constexpr auto magic = "PASTRA04";
struct Cancelled {};
QString blendName(core::BlendMode mode)
{
    switch (mode) {
    case core::BlendMode::Normal: return QStringLiteral("normal");
    case core::BlendMode::Multiply: return QStringLiteral("multiply");
    case core::BlendMode::Screen: return QStringLiteral("screen");
    }
    throw std::runtime_error("Modo de fusión no compatible.");
}
core::BlendMode blendMode(const QJsonValue& value)
{
    if (value == QJsonValue(QStringLiteral("normal"))) return core::BlendMode::Normal;
    if (value == QJsonValue(QStringLiteral("multiply"))) return core::BlendMode::Multiply;
    if (value == QJsonValue(QStringLiteral("screen"))) return core::BlendMode::Screen;
    throw std::runtime_error("Modo de fusión no compatible.");
}
void check(bool valid, const char* message)
{
    if (!valid) throw std::runtime_error(message);
}
void checkpoint(const std::atomic_bool& cancelled)
{
    if (cancelled.load(std::memory_order_relaxed)) throw Cancelled{};
}
QJsonObject object(const QJsonValue& value, std::initializer_list<const char*> keys)
{
    check(value.isObject(), "Objeto de proyecto inválido.");
    const auto result = value.toObject();
    check(result.size() == static_cast<qsizetype>(keys.size()), "Campos desconocidos o ausentes en el proyecto.");
    for (const auto* key : keys) check(result.contains(QLatin1String(key)), "Campo requerido ausente.");
    return result;
}
double number(const QJsonValue& value)
{
    check(value.isDouble() && std::isfinite(value.toDouble()), "Número de proyecto inválido.");
    return value.toDouble();
}
qint64 integer(const QJsonValue& value, qint64 minimum, qint64 maximum)
{
    const auto n = number(value);
    check(n >= static_cast<double>(minimum) && n <= static_cast<double>(maximum) && std::floor(n) == n,
        "Entero de proyecto fuera de rango.");
    return static_cast<qint64>(n);
}
QString name(const QJsonValue& value)
{
    check(value.isString(), "Nombre de proyecto inválido.");
    const auto text = value.toString();
    check(!text.isEmpty() && text.toUtf8().size() <= 4096 && !text.contains(QChar(0)), "Nombre vacío o demasiado largo.");
    return text;
}
std::uint64_t id(const QJsonValue& value)
{
    check(value.isString(), "Identificador inválido.");
    const auto text = value.toString();
    bool ok = false;
    const auto result = text.toULongLong(&ok);
    check(ok && result > 0 && result <= 0x7FFFFFFFFFFFFFFFULL && QString::number(result) == text,
        "Identificador fuera de rango.");
    return result;
}
bool boolean(const QJsonValue& value)
{
    check(value.isBool(), "Booleano de proyecto inválido."); return value.toBool();
}
core::ImageExtent extent(const QJsonObject& value)
{
    const auto width = integer(value["width"], 1, core::RasterImage::maxPixels);
    const auto height = integer(value["height"], 1, core::RasterImage::maxPixels);
    check(static_cast<std::uint64_t>(width * height) <= core::RasterImage::maxPixels, "Dimensiones excesivas.");
    return {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
}
QByteArray digest(std::span<const std::uint8_t> pixels, const std::atomic_bool& cancelled)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    for (std::size_t offset = 0; offset < pixels.size();) {
        checkpoint(cancelled);
        const auto count = std::min<std::size_t>(chunkSize, pixels.size() - offset);
        hash.addData(QByteArrayView(reinterpret_cast<const char*>(pixels.data() + offset), static_cast<qsizetype>(count)));
        offset += count;
    }
    return hash.result();
}
void write(QIODevice& file, QByteArrayView bytes)
{
    check(file.write(bytes.data(), bytes.size()) == bytes.size(), "No se pudo escribir el proyecto.");
}
}

ExportResult saveProject(const core::Document& document, const QString& path, const std::atomic_bool& cancelled)
{
    try {
        checkpoint(cancelled);
        check(!path.isEmpty(), "Ruta vacía.");
        check(document.layers().size() <= maxProjectLayers, "El formato inicial admite hasta 256 capas.");
        QJsonArray layers, rasters;
        std::vector<std::shared_ptr<const core::RasterImage>> images;
        std::unordered_map<const core::RasterImage*, int> indices;
        qint64 total = 0;
        for (const auto& layer : document.layers()) {
            checkpoint(cancelled);
            int raster = -1;
            if (layer.image) {
                const auto [entry, inserted] = indices.emplace(layer.image.get(), static_cast<int>(images.size()));
                raster = entry->second;
                if (inserted) {
                    const auto bytes = static_cast<qint64>(layer.image->pixels().size());
                    total += bytes;
                    check(total <= maxProjectPixelBytes, "El proyecto supera 512 MiB de píxeles.");
                    const auto size = layer.image->extent();
                    rasters.append(QJsonObject{{"width", static_cast<int>(size.width)}, {"height", static_cast<int>(size.height)},
                        {"sha256", QString::fromLatin1(digest(layer.image->pixels(), cancelled).toHex())}});
                    images.push_back(layer.image);
                }
            }
            QJsonArray effects;
            for (const auto& effect : layer.effects) {
                check(effect.id <= 0x7FFFFFFFFFFFFFFFULL, "Identificador de efecto fuera de rango.");
                effects.append(QJsonObject{{"id", QString::number(effect.id)}, {"kind", "exposure"},
                    {"enabled", effect.enabled}, {"stops", effect.exposureStops}});
            }
            check(layer.id <= 0x7FFFFFFFFFFFFFFFULL, "Identificador de capa fuera de rango.");
            layers.append(QJsonObject{{"id", QString::number(layer.id)}, {"name", name(QString::fromStdString(layer.name))},
                {"kind", "raster"}, {"raster", raster}, {"opacity", layer.opacity}, {"visible", layer.visible}, {"effects", effects},
                {"x", layer.position.x}, {"y", layer.position.y}, {"scaleX", layer.scale.x}, {"scaleY", layer.scale.y},
                {"blendMode", blendName(layer.blendMode)}});
        }
        QJsonObject root{{"version", 4}, {"title", name(QString::fromStdString(document.title()))},
            {"width", static_cast<qint64>(document.extent().width)}, {"height", static_cast<qint64>(document.extent().height)},
            {"encoding", "srgb-rgba8-premul"}, {"rasters", rasters}, {"layers", layers}};
        (void)extent(root);
        const auto metadata = QJsonDocument(root).toJson(QJsonDocument::Compact);
        check(metadata.size() <= maxMetadata, "Metadatos demasiado grandes.");
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        check(file.open(QIODevice::WriteOnly), "No se pudo crear el archivo temporal del proyecto.");
        write(file, QByteArrayView(magic, 8));
        const quint32 length = qToBigEndian(static_cast<quint32>(metadata.size()));
        write(file, QByteArrayView(reinterpret_cast<const char*>(&length), 4));
        write(file, QCryptographicHash::hash(metadata, QCryptographicHash::Sha256));
        write(file, metadata);
        for (const auto& image : images) {
            const auto pixels = image->pixels();
            for (std::size_t offset = 0; offset < pixels.size();) {
                checkpoint(cancelled);
                const auto count = std::min<std::size_t>(chunkSize, pixels.size() - offset);
                write(file, QByteArrayView(reinterpret_cast<const char*>(pixels.data() + offset), static_cast<qsizetype>(count)));
                offset += count;
            }
        }
        checkpoint(cancelled);
        check(file.commit(), "No se pudo confirmar el proyecto guardado.");
        return {{}, false, true};
    } catch (const Cancelled&) { return {{}, true, false}; }
    catch (const std::exception& error) { return {QString::fromUtf8(error.what())}; }
}

LoadResult loadProject(const QString& path, const std::atomic_bool& cancelled)
{
    try {
        checkpoint(cancelled);
        QFile file(path);
        check(file.open(QIODevice::ReadOnly), "No se pudo abrir el proyecto.");
        check(file.size() >= 44 && file.size() <= 44 + maxMetadata + maxProjectPixelBytes, "Tamaño de proyecto inválido.");
        const auto header = file.read(8);
        const int version = header == QByteArray("PASTRA01", 8) ? 1 : header == QByteArray("PASTRA02", 8) ? 2 :
            header == QByteArray("PASTRA03", 8) ? 3 : header == QByteArray(magic, 8) ? 4 : 0;
        check(version != 0, "Cabecera o versión de proyecto no compatible.");
        const auto lengthBytes = file.read(4);
        check(lengthBytes.size() == 4, "Cabecera incompleta.");
        const auto length = qFromBigEndian<quint32>(lengthBytes.constData());
        check(length > 0 && length <= maxMetadata, "Metadatos demasiado grandes.");
        const auto expectedHash = file.read(32);
        const auto metadata = file.read(length);
        check(metadata.size() == length && QCryptographicHash::hash(metadata, QCryptographicHash::Sha256) == expectedHash,
            "Metadatos truncados o dañados.");
        const auto json = QJsonDocument::fromJson(metadata);
        check(json.isObject(), "JSON de proyecto inválido.");
        const auto root = object(json.object(), {"version", "title", "width", "height", "encoding", "rasters", "layers"});
        check(integer(root["version"], 1, 4) == version && root["encoding"] == "srgb-rgba8-premul", "Versión o codificación no compatible.");
        const auto size = extent(root);
        const auto title = name(root["title"]).toStdString();
        check(root["rasters"].isArray() && root["layers"].isArray(), "Listas de proyecto inválidas.");
        const auto rasterList = root["rasters"].toArray();
        const auto layerList = root["layers"].toArray();
        check(rasterList.size() <= maxProjectLayers && layerList.size() <= maxProjectLayers, "Demasiadas capas o rasters.");
        std::vector<core::ImageExtent> sizes;
        std::vector<QByteArray> hashes;
        qint64 total = 0;
        for (const auto value : rasterList) {
            const auto raster = object(value, {"width", "height", "sha256"});
            const auto dimensions = extent(raster);
            sizes.push_back(dimensions);
            total += static_cast<qint64>(dimensions.width) * dimensions.height * 4;
            check(total <= maxProjectPixelBytes, "El proyecto supera 512 MiB de píxeles.");
            const auto hash = raster["sha256"].toString().toLatin1();
            const auto decoded = QByteArray::fromHex(hash);
            check(hash.size() == 64 && decoded.toHex() == hash, "Checksum de raster inválido.");
            hashes.push_back(decoded);
        }
        check(file.size() - file.pos() == total, "Datos de píxeles truncados o sobrantes.");
        std::vector<core::RasterLayer> layers;
        std::vector<int> refs;
        for (const auto value : layerList) {
            const auto layer = version == 1 ? object(value, {"id", "name", "kind", "raster", "opacity", "visible", "effects"}) :
                version == 2 ? object(value, {"id", "name", "kind", "raster", "opacity", "visible", "effects", "x", "y"}) :
                version == 3 ? object(value, {"id", "name", "kind", "raster", "opacity", "visible", "effects", "x", "y", "scaleX", "scaleY"}) :
                object(value, {"id", "name", "kind", "raster", "opacity", "visible", "effects", "x", "y", "scaleX", "scaleY", "blendMode"});
            check(layer["kind"] == "raster" && layer["effects"].isArray(), "Tipo de capa o efectos no compatible.");
            core::RasterLayer result{id(layer["id"]), name(layer["name"]).toStdString(), {}};
            if (version >= 2) result.position = {
                static_cast<std::int32_t>(integer(layer["x"], -core::maxLayerOffset, core::maxLayerOffset)),
                static_cast<std::int32_t>(integer(layer["y"], -core::maxLayerOffset, core::maxLayerOffset))};
            if (version >= 3) result.scale = {number(layer["scaleX"]), number(layer["scaleY"])};
            if (version >= 4) result.blendMode = blendMode(layer["blendMode"]);
            result.opacity = static_cast<float>(number(layer["opacity"]));
            check(number(layer["opacity"]) >= 0 && number(layer["opacity"]) <= 1, "Opacidad fuera de rango.");
            result.visible = boolean(layer["visible"]);
            const auto effectList = layer["effects"].toArray();
            check(effectList.size() <= static_cast<qsizetype>(core::maxEffectsPerLayer), "Demasiados efectos.");
            for (const auto effectValue : effectList) {
                const auto effect = object(effectValue, {"id", "kind", "enabled", "stops"});
                check(effect["kind"] == "exposure", "Efecto no compatible.");
                const auto stops = number(effect["stops"]);
                check(stops >= -8 && stops <= 8, "Exposición fuera de rango.");
                result.effects.push_back({id(effect["id"]), core::EffectType::Exposure, boolean(effect["enabled"]), static_cast<float>(stops)});
            }
            refs.push_back(static_cast<int>(integer(layer["raster"], -1, rasterList.size() - 1)));
            layers.push_back(std::move(result));
        }
        core::Document document(size, title);
        document = document.withLayers(layers); // Validate all metadata before allocating pixel buffers.
        std::vector<std::shared_ptr<const core::RasterImage>> images;
        for (std::size_t index = 0; index < sizes.size(); ++index) {
            checkpoint(cancelled);
            const auto dimensions = sizes[index];
            std::vector<std::uint8_t> pixels(static_cast<std::size_t>(dimensions.width) * dimensions.height * 4);
            for (std::size_t offset = 0; offset < pixels.size();) {
                checkpoint(cancelled);
                const auto count = std::min<std::size_t>(chunkSize, pixels.size() - offset);
                check(file.read(reinterpret_cast<char*>(pixels.data() + offset), static_cast<qint64>(count)) == static_cast<qint64>(count), "Raster incompleto.");
                for (std::size_t p = offset; p < offset + count; p += 4)
                    check(pixels[p] <= pixels[p + 3] && pixels[p + 1] <= pixels[p + 3] && pixels[p + 2] <= pixels[p + 3], "Alpha premultiplicado inválido.");
                offset += count;
            }
            check(digest(pixels, cancelled) == hashes[index], "Píxeles dañados: checksum incorrecto.");
            images.push_back(std::make_shared<const core::RasterImage>(dimensions, std::move(pixels)));
        }
        for (std::size_t index = 0; index < layers.size(); ++index)
            if (refs[index] >= 0) layers[index].image = images[static_cast<std::size_t>(refs[index])];
        checkpoint(cancelled);
        return {std::make_shared<const core::Document>(document.withLayers(std::move(layers))), {}};
    } catch (const Cancelled&) { return {{}, {}, true}; }
    catch (const std::exception& error) { return {{}, QString::fromUtf8(error.what())}; }
}
}
