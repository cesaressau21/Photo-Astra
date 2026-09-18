#include <photoastra/io/ProjectFile.h>
#include <photoastra/application/DocumentSession.h>
#include <photoastra/core/RasterImage.h>
#include <photoastra/render/SkiaRenderer.h>
#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <QtEndian>
#include <array>

using namespace photoastra;
namespace {
core::Document sample()
{
    const auto image = std::make_shared<const core::RasterImage>(core::ImageExtent{1, 1},
        std::vector<std::uint8_t>{64,32,0,128});
    core::RasterLayer bottom{77, "Capa ñ", image, 0.75F};
    bottom.position = {1, 2};
    bottom.scale = {1.5, 0.75};
    bottom.blendMode = core::BlendMode::Multiply;
    bottom.effects = {{9, core::EffectType::Exposure, true, 0.5F}, {10, core::EffectType::Exposure, false, -2}};
    core::RasterLayer top{92, "Hidden shared", image, 0.5F, false};
    top.blendMode = core::BlendMode::Screen;
    const core::Document document({2, 3}, "Proyecto ñ");
    return document.withLayers({bottom, top, {113, "Vacía", {}}});
}
QByteArray readAll(const QString& path)
{
    QFile file(path); if (!file.open(QIODevice::ReadOnly)) return {}; return file.readAll();
}
bool writeAll(const QString& path, const QByteArray& bytes)
{
    QFile file(path); return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QByteArray pack(const QJsonObject& root, const QByteArray& pixels)
{
    const auto json = QJsonDocument(root).toJson(QJsonDocument::Compact);
    const auto length = qToBigEndian(static_cast<quint32>(json.size()));
    QByteArray data(root["version"].toInt() == 1 ? "PASTRA01" : root["version"].toInt() == 2 ? "PASTRA02" :
        root["version"].toInt() == 3 ? "PASTRA03" : "PASTRA04", 8);
    data.append(reinterpret_cast<const char*>(&length), 4);
    data.append(QCryptographicHash::hash(json, QCryptographicHash::Sha256));
    data.append(json); data.append(pixels); return data;
}
}
class ProjectFileTests final : public QObject {
    Q_OBJECT
private slots:
    void legacyTransformsAndInvalidV3()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("legacy.pastra"));
        std::atomic_bool cancelled{false};
        QVERIFY(io::saveProject(sample(), path, cancelled).succeeded);
        const auto bytes = readAll(path);
        const auto length = qFromBigEndian<quint32>(bytes.constData() + 8);
        auto root = QJsonDocument::fromJson(bytes.mid(44, length)).object();
        const auto pixels = bytes.mid(44 + length);
        auto layers = root["layers"].toArray();
        auto layer = layers[0].toObject(); layer["x"] = 0.5; layers[0] = layer; root["layers"] = layers;
        QVERIFY(writeAll(path, pack(root, pixels))); QVERIFY(!io::loadProject(path, cancelled).document);
        layer["x"] = 1000001; layers[0] = layer; root["layers"] = layers;
        QVERIFY(writeAll(path, pack(root, pixels))); QVERIFY(!io::loadProject(path, cancelled).document);
        layer["x"] = 1;
        for (const QJsonValue& invalid : {QJsonValue(0), QJsonValue(-2), QJsonValue(65), QJsonValue(0.001), QJsonValue("large"), QJsonValue()}) {
            layer["scaleX"] = invalid; layers[0] = layer; root["layers"] = layers;
            QVERIFY(writeAll(path, pack(root, pixels))); QVERIFY(!io::loadProject(path, cancelled).document);
        }
        layer["scaleX"] = 1.5;
        for (const QJsonValue& invalid : {QJsonValue("overlay"), QJsonValue(1), QJsonValue()}) {
            layer["blendMode"] = invalid; layers[0] = layer; root["layers"] = layers;
            QVERIFY(writeAll(path, pack(root, pixels))); QVERIFY(!io::loadProject(path, cancelled).document);
        }
        root["version"] = 3;
        for (qsizetype i = 0; i < layers.size(); ++i) {
            auto legacy = layers[i].toObject(); legacy.remove("blendMode"); layers[i] = legacy;
        }
        root["layers"] = layers;
        QVERIFY(writeAll(path, pack(root, pixels)));
        const auto v3 = io::loadProject(path, cancelled); QVERIFY2(v3.document, qPrintable(v3.error));
        for (const auto& restored : v3.document->layers()) QCOMPARE(restored.blendMode, core::BlendMode::Normal);
        QVERIFY(v3.document->layers()[0].scale == (core::LayerScale{1.5, 0.75}));
        root["version"] = 2;
        for (qsizetype i = 0; i < layers.size(); ++i) {
            auto legacy = layers[i].toObject(); legacy.remove("scaleX"); legacy.remove("scaleY"); layers[i] = legacy;
        }
        root["layers"] = layers;
        QVERIFY(writeAll(path, pack(root, pixels)));
        const auto v2 = io::loadProject(path, cancelled); QVERIFY2(v2.document, qPrintable(v2.error));
        QVERIFY(v2.document->layers()[0].position == (core::LayerPosition{1, 2}));
        for (const auto& restored : v2.document->layers()) QVERIFY(restored.scale == core::LayerScale{});
        root["version"] = 1;
        for (qsizetype i = 0; i < layers.size(); ++i) {
            auto legacy = layers[i].toObject(); legacy.remove("x"); legacy.remove("y"); layers[i] = legacy;
        }
        root["layers"] = layers;
        QVERIFY(writeAll(path, pack(root, pixels)));
        const auto loaded = io::loadProject(path, cancelled); QVERIFY2(loaded.document, qPrintable(loaded.error));
        for (const auto& restored : loaded.document->layers()) QVERIFY(restored.position == core::LayerPosition{});
        QVERIFY(io::saveProject(*loaded.document, path, cancelled).succeeded);
        QVERIFY(readAll(path).startsWith("PASTRA04"));
    }
    void roundTripAndComposite()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("proyecto-ñ.pastra"));
        std::atomic_bool cancelled{false};
        const auto before = sample();
        QVERIFY(io::saveProject(before, path, cancelled).succeeded);
        const auto result = io::loadProject(path, cancelled);
        QVERIFY2(result.document, qPrintable(result.error));
        const auto& after = *result.document;
        QCOMPARE(after.title(), before.title()); QCOMPARE(after.extent(), before.extent());
        QCOMPARE(after.layers().size(), before.layers().size());
        for (std::size_t i = 0; i < before.layers().size(); ++i) {
            const auto& a = after.layers()[i]; const auto& b = before.layers()[i];
            QCOMPARE(a.id, b.id); QCOMPARE(a.name, b.name); QCOMPARE(a.opacity, b.opacity);
            QVERIFY(a.position == b.position);
            QVERIFY(a.scale == b.scale);
            QCOMPARE(a.blendMode, b.blendMode);
            QCOMPARE(a.visible, b.visible); QVERIFY(a.effects == b.effects);
        }
        QCOMPARE(after.layers()[0].image, after.layers()[1].image);
        QVERIFY(!after.layers()[2].image);
        QVERIFY(std::equal(after.layers()[0].image->pixels().begin(), after.layers()[0].image->pixels().end(),
            before.layers()[0].image->pixels().begin()));
        render::SkiaRenderer renderer;
        std::array<std::uint8_t, 24> first{}, second{};
        QVERIFY(renderer.composeRaster(before, {2, 3, 8, first, 1}));
        QVERIFY(renderer.composeRaster(after, {2, 3, 8, second, 1}));
        QVERIFY(first == second);
        QVERIFY(io::saveProject(core::Document({1, 1}, "Empty"), path, cancelled).succeeded);
        const auto empty = io::loadProject(path, cancelled); QVERIFY(empty.document);
        QVERIFY(empty.document->layers().empty());
    }
    void malformedFiles_data()
    {
        QTest::addColumn<int>("scenario");
        const char* names[] = {"magic", "truncated", "trailing", "metadata-checksum", "pixel-checksum",
            "metadata-size", "version", "duplicate-layer", "raster-ref", "dimensions", "unknown-effect",
            "bad-alpha", "opacity", "wrong-type", "unknown-field", "effect-id", "effect-range"};
        for (int i = 0; i < 17; ++i) QTest::newRow(names[i]) << i;
    }
    void malformedFiles()
    {
        QFETCH(int, scenario);
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("bad.pastra"));
        std::atomic_bool cancelled{false};
        QVERIFY(io::saveProject(sample(), path, cancelled).succeeded);
        auto data = readAll(path);
        const auto length = qFromBigEndian<quint32>(data.constData() + 8);
        auto root = QJsonDocument::fromJson(data.mid(44, length)).object();
        auto pixels = data.mid(44 + length);
        auto layers = root["layers"].toArray();
        auto layer = layers[0].toObject();
        auto effects = layer["effects"].toArray();
        auto effect = effects[0].toObject();
        switch (scenario) {
        case 0: data[0] = 'X'; break;
        case 1: data.chop(1); break;
        case 2: data.append('X'); break;
        case 3: data[44] = 'X'; break;
        case 4: data[data.size() - 4] = 60; break;
        case 5: data[8] = char(0x7F); break;
        case 6: root["version"] = 5; break;
        case 7: layer["id"] = "92"; break;
        case 8: layer["raster"] = 5; break;
        case 9: root["width"] = 33554433; break;
        case 10: effect["kind"] = "external-code"; break;
        case 11: {
            pixels[0] = static_cast<char>(-1);
            auto rasters = root["rasters"].toArray(); auto raster = rasters[0].toObject();
            raster["sha256"] = QString::fromLatin1(QCryptographicHash::hash(pixels, QCryptographicHash::Sha256).toHex());
            rasters[0] = raster; root["rasters"] = rasters; break;
        }
        case 12: layer["opacity"] = -1; break;
        case 13: layer["visible"] = "false"; break;
        case 14: root["future-feature"] = true; break;
        case 15: effect["id"] = "18446744073709551615"; break;
        case 16: effect["stops"] = 20; break;
        }
        if (scenario >= 6) {
            effects[0] = effect; layer["effects"] = effects; layers[0] = layer; root["layers"] = layers;
            data = pack(root, pixels);
        }
        QVERIFY(writeAll(path, data));
        const auto result = io::loadProject(path, cancelled);
        QVERIFY(!result.document); QVERIFY(!result.error.isEmpty());
    }
    void atomicFailureAndCancellation()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("preserve.pastra"));
        std::atomic_bool cancelled{false};
        QVERIFY(io::saveProject(sample(), path, cancelled).succeeded);
        const auto original = readAll(path);
        cancelled.store(true);
        QVERIFY(io::saveProject(sample(), path, cancelled).cancelled);
        QVERIFY(io::loadProject(path, cancelled).cancelled);
        QCOMPARE(readAll(path), original);
        cancelled.store(false);
        QVERIFY(!io::saveProject(core::Document({33554433, 1}, "Large"), path, cancelled).succeeded);
        QCOMPARE(readAll(path), original);
        QVERIFY(!io::saveProject(sample(), directory.path(), cancelled).succeeded);
    }
    void sessionSaveOpenAndHistory()
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("session.pastra"));
        application::DocumentSession session;
        session.addTransparentLayer(); session.addExposure(session.document()->layers()[0].id);
        const auto snapshot = session.document();
        QSignalSpy saved(&session, &application::DocumentSession::projectSaved);
        QSignalSpy failed(&session, &application::DocumentSession::projectSaveFailed);
        QVERIFY(session.saveDocument(path));
        QVERIFY(!session.createDocument({1, 1}, "Busy"));
        QTRY_COMPARE(saved.count(), 1);
        QVERIFY(!session.history().dirty()); QCOMPARE(session.document(), snapshot);
        QVERIFY(session.history().canUndo()); session.undo(); QVERIFY(session.history().dirty());
        session.redo(); QVERIFY(!session.history().dirty());
        session.addTransparentLayer(); const auto modified = session.document();
        QVERIFY(session.saveDocument(directory.path())); QTRY_COMPARE(failed.count(), 1);
        QVERIFY(session.history().dirty()); QCOMPARE(session.document(), modified); QCOMPARE(session.projectPath(), path);
        QVERIFY(session.openImage(path)); QTRY_VERIFY(!session.busy());
        QCOMPARE(session.document()->layers().size(), snapshot->layers().size());
        QVERIFY(!session.history().canUndo()); QVERIFY(!session.history().dirty());
        std::atomic_bool cancelled{false}; QVERIFY(io::saveProject(sample(), path, cancelled).succeeded);
        QVERIFY(session.openImage(path)); QTRY_VERIFY(!session.busy());
        session.addTransparentLayer(); QVERIFY(session.document()->layers().back().id > 113);
        const auto beforeFailure = session.document();
        QVERIFY(writeAll(path, "broken"));
        QSignalSpy openFailed(&session, &application::DocumentSession::importFailed);
        QVERIFY(session.openImage(path)); QTRY_COMPARE(openFailed.count(), 1);
        QCOMPARE(session.document(), beforeFailure);
    }
};
QTEST_GUILESS_MAIN(ProjectFileTests)
#include "ProjectFileTests.moc"
