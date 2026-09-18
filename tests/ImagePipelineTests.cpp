#include <photoastra/application/DocumentSession.h>
#include <photoastra/core/RasterImage.h>
#include <photoastra/io/ImageLoader.h>
#include <photoastra/render/SkiaRenderer.h>
#include <QColorSpace>
#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>
#include <array>
#include <cmath>
#include <photoastra/application/ImageExport.h>

class ImagePipelineTests final : public QObject {
    Q_OBJECT
private slots:
    void blendAlphaAndBackdrop()
    {
        using namespace photoastra;
        render::SkiaRenderer renderer;
        for (const auto mode : {core::BlendMode::Normal, core::BlendMode::Multiply, core::BlendMode::Screen}) {
            for (const int sourceAlpha : {0, 128, 255}) for (const int destAlpha : {0, 128, 255}) {
                const auto raster = [](int alpha, bool source) {
                    return std::make_shared<const core::RasterImage>(core::ImageExtent{1, 1},
                        std::vector<std::uint8_t>{static_cast<std::uint8_t>(source ? alpha / 2 : alpha / 4),
                            static_cast<std::uint8_t>(source ? alpha / 4 : alpha / 2), 0, static_cast<std::uint8_t>(alpha)});
                };
                const auto source = raster(sourceAlpha, true), dest = raster(destAlpha, false);
                for (const float opacity : {0.0F, 0.5F, 1.0F}) {
                    core::RasterLayer upper{2, "Source", source, opacity}; upper.blendMode = mode;
                    const auto doc = core::Document({1, 1}, "Blend").withLayers({{1, "Dest", dest}, upper});
                    std::array<std::uint8_t, 4> pixels{};
                    QVERIFY(renderer.composeRaster(doc, {1, 1, 4, pixels, 1}));
                    const double sa = sourceAlpha / 255.0 * opacity, da = destAlpha / 255.0;
                    for (std::size_t channel = 0; channel < 3; ++channel) {
                        const double s = source->pixels()[channel] / 255.0 * opacity;
                        const double d = dest->pixels()[channel] / 255.0;
                        const double expected = mode == core::BlendMode::Normal ? s + d * (1 - sa) :
                            mode == core::BlendMode::Multiply ? s * (1 - da) + d * (1 - sa) + s * d : s + d - s * d;
                        QVERIFY(std::abs(int(pixels[channel]) - std::lround(expected * 255)) <= 2);
                    }
                    QVERIFY(std::abs(int(pixels[3]) - std::lround((sa + da - sa * da) * 255)) <= 1);
                }
            }
            // An isolated source must look identical in every mode, including over the checker.
            const auto source = std::make_shared<const core::RasterImage>(core::ImageExtent{1, 1}, std::vector<std::uint8_t>{64,32,0,128});
            core::RasterLayer layer{1, "Only source", source};
            const auto normal = core::Document({1, 1}, "Backdrop").withLayers({layer});
            layer.blendMode = mode;
            const auto blended = normal.withLayers({layer});
            std::array<std::uint8_t, 4> reference{}, actual{};
            QVERIFY(renderer.renderRaster(normal, core::Viewport{}, {1, 1, 4, reference, 1}));
            QVERIFY(renderer.renderRaster(blended, core::Viewport{}, {1, 1, 4, actual, 1}));
            for (std::size_t channel = 0; channel < 4; ++channel)
                QVERIFY(std::abs(int(reference[channel]) - int(actual[channel])) <= 1);
        }
    }
    void scaledLayersAndExport()
    {
        using namespace photoastra;
        const auto source = std::make_shared<const core::RasterImage>(core::ImageExtent{1, 1}, std::vector<std::uint8_t>{64,32,0,128});
        core::RasterLayer layer{1, "Scaled", source}; layer.position = {1, 1}; layer.scale = {2, 3};
        const auto document = core::Document({5, 5}, "Scale").withLayers({layer, {2, "Unscaled", source}});
        render::SkiaRenderer renderer;
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("scaled.png"));
        std::atomic_bool cancelled{false};
        const auto result = application::exportDocument(document, renderer, path, io::ExportFormat::Png, cancelled);
        QVERIFY2(result.succeeded, qPrintable(result.error));
        const QImage image(path);
        QCOMPARE(image.size(), QSize(5, 5));
        QCOMPARE(image.pixelColor(0, 0).alpha(), 128);
        QCOMPARE(image.pixelColor(2, 3).alpha(), 128);
        QCOMPARE(image.pixelColor(3, 3).alpha(), 0);
        QCOMPARE(image.pixelColor(2, 4).alpha(), 0);
        QCOMPARE(source->extent().width, 1U);
        QCOMPARE(source->pixels()[0], 64);
    }
    void positionedLayersAndClipping()
    {
        using namespace photoastra;
        const auto source = std::make_shared<const core::RasterImage>(core::ImageExtent{1, 1}, std::vector<std::uint8_t>{255,0,0,255});
        const core::Document base({3, 2}, "Position");
        core::History history(std::make_shared<const core::Document>(base.withLayers({{1, "Red", source}})));
        render::SkiaRenderer renderer;
        std::array<std::uint8_t, 24> pixels{};
        QVERIFY(history.execute(core::SetPosition{1, {2, 1}}));
        QVERIFY(renderer.composeRaster(*history.document(), {3, 2, 12, pixels, 1}));
        QCOMPARE(pixels[3], 0); QCOMPARE(pixels[20], 255); QCOMPARE(pixels[23], 255);
        const auto valid = history.document();
        QVERIFY_THROWS_EXCEPTION(std::invalid_argument, history.execute(core::SetPosition{1, {1000001, 0}}));
        QCOMPARE(history.document(), valid);
        QVERIFY(history.undo()); QCOMPARE(history.document()->layers()[0].position.x, 0);
        QVERIFY(history.redo()); QCOMPARE(history.document()->layers()[0].position.x, 2);
        QVERIFY(history.execute(core::SetPosition{1, {-1, 0}}));
        QVERIFY(renderer.composeRaster(*history.document(), {3, 2, 12, pixels, 1}));
        for (auto value : pixels) QCOMPARE(value, 0);
        QCOMPARE(source->pixels()[0], 255);
    }
    void exposureStackAndAlpha()
    {
        using namespace photoastra;
        const auto source = std::make_shared<const core::RasterImage>(core::ImageExtent{3, 1},
            std::vector<std::uint8_t>{128,128,128,255, 64,64,64,128, 0,0,0,0});
        core::Document document({3, 1}, "Exposure", source);
        auto layers = document.layers();
        layers[0].effects = {{1, core::EffectType::Exposure, true, 1}};
        document = document.withLayers(layers);
        render::SkiaRenderer renderer;
        std::array<std::uint8_t, 12> pixels{};
        const auto compose = [&] { return renderer.composeRaster(document, {3, 1, 12, pixels, 1}); };
        QVERIFY2(compose(), std::string(renderer.lastError()).c_str());
        QVERIFY(pixels[0] >= 174 && pixels[0] <= 177); // +1 EV doubles linear light, not encoded sRGB.
        QVERIFY(pixels[4] >= 86 && pixels[4] <= 89);
        QCOMPARE(pixels[7], 128); QCOMPARE(pixels[11], 0); QCOMPARE(pixels[8], 0);
        layers[0].opacity = 0.5F; document = document.withLayers(layers); QVERIFY(compose());
        QVERIFY(pixels[0] >= 86 && pixels[0] <= 89); QCOMPARE(pixels[3], 128);
        layers[0].opacity = 1;
        layers[0].effects[0].enabled = false; document = document.withLayers(layers);
        QVERIFY(compose()); QCOMPARE(pixels[0], 128);
        layers[0].effects = {{1, core::EffectType::Exposure, true, 4}, {2, core::EffectType::Exposure, true, -4}};
        document = document.withLayers(layers); QVERIFY(compose());
        const auto clipped = pixels[0]; QVERIFY(clipped >= 69 && clipped <= 72);
        std::swap(layers[0].effects[0], layers[0].effects[1]);
        document = document.withLayers(layers); QVERIFY(compose());
        QVERIFY(pixels[0] >= 126 && pixels[0] <= 130); QVERIFY(pixels[0] > clipped);
        layers[0].effects.clear(); document = document.withLayers(layers);
        QVERIFY(compose()); QCOMPARE(pixels[0], 128);
        QCOMPARE(source->pixels()[0], 128); QCOMPARE(source->pixels()[4], 64);
    }
    void exportRoundTripAndFailureSafety()
    {
        using namespace photoastra;
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        auto source = std::make_shared<const core::RasterImage>(core::ImageExtent{16, 16},
            std::vector<std::uint8_t>(16 * 16 * 4, 0));
        core::Document document({32, 16}, "Export", {});
        auto red = std::vector<std::uint8_t>(16 * 16 * 4, 0);
        for (std::size_t i = 0; i < red.size(); i += 4) { red[i] = 255; red[i + 3] = 255; }
        source = std::make_shared<const core::RasterImage>(core::ImageExtent{16, 16}, std::move(red));
        document = document.withLayers({{1, "Red", source, 0.5F}});
        render::SkiaRenderer renderer;
        std::atomic_bool cancelled{false};
        const auto png = directory.filePath(QStringLiteral("composición.png"));
        QVERIFY(application::exportDocument(document, renderer, png, io::ExportFormat::Png, cancelled).succeeded);
        QImage decoded(png);
        QCOMPARE(decoded.size(), QSize(32, 16));
        QCOMPARE(decoded.colorSpace(), QColorSpace(QColorSpace::SRgb));
        const auto pixel = decoded.pixelColor(8, 8);
        QCOMPARE(pixel.red(), 255);
        QVERIFY(pixel.alpha() >= 127 && pixel.alpha() <= 128);
        QCOMPARE(decoded.pixelColor(24, 8).alpha(), 0);
        const auto jpg = directory.filePath(QStringLiteral("composición.jpg"));
        QVERIFY(application::exportDocument(document, renderer, jpg, io::ExportFormat::Jpeg, cancelled).succeeded);
        QImage jpeg(jpg);
        const auto pink = jpeg.pixelColor(8, 8);
        QVERIFY(pink.red() > 245 && pink.green() > 120 && pink.green() < 135);
        QVERIFY(jpeg.pixelColor(24, 8).red() > 245 && jpeg.pixelColor(24, 8).green() > 245);
        QFile original(png);
        QVERIFY(original.open(QIODevice::ReadOnly));
        const auto bytes = original.readAll(); original.close();
        cancelled.store(true);
        QVERIFY(application::exportDocument(document, renderer, png, io::ExportFormat::Png, cancelled).cancelled);
        cancelled.store(false);
        render::UnavailableRenderer unavailable;
        QVERIFY(!application::exportDocument(document, unavailable, png, io::ExportFormat::Png, cancelled).succeeded);
        QVERIFY(!application::exportDocument(document, renderer, directory.path(), io::ExportFormat::Png, cancelled).succeeded);
        QVERIFY(original.open(QIODevice::ReadOnly)); QCOMPARE(original.readAll(), bytes);
        QCOMPARE(source->pixels()[0], 255); QCOMPARE(source->pixels()[3], 255);
        const core::Document oversized({33554433, 1}, "Too large");
        QVERIFY(!application::exportDocument(oversized, renderer, png, io::ExportFormat::Png, cancelled).succeeded);
    }
    void newDocumentAndAsyncExport()
    {
        using namespace photoastra;
        QTemporaryDir directory;
        application::DocumentSession session;
        session.addTransparentLayer();
        const auto previous = session.document();
        QVERIFY(!session.createDocument({0, 4}, "Invalid"));
        QVERIFY(!session.createDocument({100000, 100000}, "Invalid"));
        QVERIFY(!session.createDocument({4, 4}, "  "));
        QCOMPARE(session.document(), previous);
        QVERIFY(session.createDocument({64, 32}, QStringLiteral("Nuevo ñ")));
        QVERIFY(session.document()->layers().empty());
        QVERIFY(!session.history().canUndo());
        session.addTransparentLayer();
        const auto snapshot = session.document();
        QSignalSpy finished(&session, &application::DocumentSession::exportFinished);
        QSignalSpy cancelled(&session, &application::DocumentSession::exportCancelled);
        const auto path = directory.filePath(QStringLiteral("result.png"));
        QVERIFY(session.exportImage(path, io::ExportFormat::Png));
        QVERIFY(!session.createDocument({1, 1}, "Busy"));
        QVERIFY(!session.openImage(path));
        QVERIFY(!session.exportImage(path, io::ExportFormat::Png));
        QTRY_COMPARE(finished.count(), 1);
        QCOMPARE(session.document(), snapshot);
        QVERIFY(session.history().dirty());
        QCOMPARE(QImage(path).pixelColor(0, 0).alpha(), 0);
        const auto cancelledPath = directory.filePath(QStringLiteral("cancelled.png"));
        QVERIFY(session.exportImage(cancelledPath, io::ExportFormat::Png));
        session.cancelImport();
        QTRY_VERIFY(!session.busy());
        // Cancellation cannot undo an atomic commit that already completed on the worker.
        if (cancelled.count() == 1) QVERIFY(!QFile::exists(cancelledPath));
        else { QCOMPARE(finished.count(), 2); QCOMPARE(QImage(cancelledPath).size(), QSize(64, 32)); }
    }
    void decodeFormatsAndErrors()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage image(3, 2, QImage::Format_RGBA8888);
        image.fill(QColor(255, 0, 0, 128));
        const auto png = directory.filePath(QStringLiteral("á-transparent.png"));
        const auto jpg = directory.filePath(QStringLiteral("opaque.jpg"));
        QVERIFY(image.save(png));
        image.fill(Qt::green);
        QVERIFY(image.save(jpg));
        std::atomic_bool cancelled = false;
        const auto pngResult = photoastra::io::loadImage(png, cancelled);
        QVERIFY2(pngResult.document, qPrintable(pngResult.error));
        QCOMPARE(pngResult.document->extent().width, 3U);
        QCOMPARE(pngResult.document->layers().front().image->pixels()[0], 128);
        QCOMPARE(pngResult.document->layers().front().image->pixels()[3], 128);
        const auto jpgResult = photoastra::io::loadImage(jpg, cancelled);
        QVERIFY2(jpgResult.document, qPrintable(jpgResult.error));
        QCOMPARE(jpgResult.document->layers().front().image->pixels()[3], 255);
        QVERIFY(jpgResult.document->layers().front().image->pixels()[1] > 240);
        QVERIFY(!photoastra::io::loadImage(directory.filePath(QStringLiteral("missing.png")), cancelled).document);
        QFile broken(directory.filePath(QStringLiteral("broken.png")));
        QVERIFY(broken.open(QIODevice::WriteOnly));
        broken.write("not an image");
        broken.close();
        QVERIFY(!photoastra::io::loadImage(broken.fileName(), cancelled).document);
        cancelled = true;
        QVERIFY(photoastra::io::loadImage(png, cancelled).cancelled);

        cancelled = false;
        QFile jpegFile(jpg);
        QVERIFY(jpegFile.open(QIODevice::ReadOnly));
        auto jpegData = jpegFile.readAll();
        jpegFile.close();
        // EXIF little endian orientation=6 (90 degrees clockwise).
        const QByteArray exif = QByteArray::fromHex("ffe1002245786966000049492a0008000000010012010300010000000600000000000000");
        jpegData.insert(2, exif);
        QVERIFY(jpegFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(jpegFile.write(jpegData), jpegData.size());
        jpegFile.close();
        const auto rotated = photoastra::io::loadImage(jpg, cancelled);
        QVERIFY2(rotated.document, qPrintable(rotated.error));
        QCOMPARE(rotated.document->extent().width, 2U);
        QCOMPARE(rotated.document->extent().height, 3U);
    }
    void colorConversionAndDecodeLimits()
    {
        QTemporaryDir directory;
        QImage linear(2, 2, QImage::Format_RGB32);
        linear.fill(QColor(128, 128, 128));
        linear.setColorSpace(QColorSpace::SRgbLinear);
        const auto path = directory.filePath(QStringLiteral("linear.png"));
        QVERIFY(linear.save(path));
        std::atomic_bool cancelled = false;
        const auto converted = photoastra::io::loadImage(path, cancelled);
        QVERIFY2(converted.document, qPrintable(converted.error));
        const auto red = converted.document->layers().front().image->pixels()[0];
        QVERIFY(red >= 186 && red <= 190); // Linear 0.5 becomes about 0.735 in sRGB.
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        auto png = file.readAll();
        file.close();
        QVERIFY(png.size() > 33);
        // Change IHDR to 8193 x 4096 (> maxPixels) and recompute its PNG CRC.
        png.replace(16, 8, QByteArray::fromHex("0000200100001000"));
        std::uint32_t crc = 0xFFFFFFFFU;
        for (qsizetype i = 12; i < 29; ++i) {
            crc ^= static_cast<std::uint8_t>(png[i]);
            for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ ((crc & 1U) ? 0xEDB88320U : 0U);
        }
        crc ^= 0xFFFFFFFFU;
        for (int i = 0; i < 4; ++i) png[29 + i] = static_cast<char>((crc >> (24 - 8 * i)) & 0xFFU);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(png), png.size());
        file.close();
        const auto oversized = photoastra::io::loadImage(path, cancelled);
        QVERIFY(!oversized.document);
        QVERIFY2(oversized.error.contains(QStringLiteral("33.554.432")), qPrintable(oversized.error));
        const auto bmp = directory.filePath(QStringLiteral("disguised.png"));
        QVERIFY(linear.save(bmp, "BMP"));
        const auto unsupported = photoastra::io::loadImage(bmp, cancelled);
        QVERIFY(!unsupported.document);
        QVERIFY(unsupported.error.contains(QStringLiteral("Formato")));
    }
    void sessionKeepsDocumentOnFailureAndCancel()
    {
        QTemporaryDir directory;
        QImage image(10, 10, QImage::Format_RGB32);
        image.fill(Qt::red);
        const auto path = directory.filePath(QStringLiteral("valid.png"));
        QVERIFY(image.save(path));
        photoastra::application::DocumentSession session;
        QSignalSpy loaded(&session, &photoastra::application::DocumentSession::documentChanged);
        QSignalSpy failed(&session, &photoastra::application::DocumentSession::importFailed);
        QSignalSpy cancelled(&session, &photoastra::application::DocumentSession::importCancelled);
        QVERIFY(session.openImage(path));
        QVERIFY(!session.openImage(path));
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        const auto original = session.document();
        QVERIFY(session.openImage(directory.filePath(QStringLiteral("missing.png"))));
        QTRY_COMPARE_WITH_TIMEOUT(failed.count(), 1, 10000);
        QCOMPARE(session.document(), original);
        QVERIFY(session.openImage(path));
        session.cancelImport();
        QTRY_COMPARE_WITH_TIMEOUT(cancelled.count(), 1, 10000);
        QCOMPARE(session.document(), original);
        QCOMPARE(loaded.count(), 1);
    }
    void layersComposeWithoutChangingSources()
    {
        using namespace photoastra::core;
        const auto red = std::make_shared<const RasterImage>(ImageExtent{1, 1}, std::vector<std::uint8_t>{255,0,0,255});
        const auto blue = std::make_shared<const RasterImage>(ImageExtent{1, 1}, std::vector<std::uint8_t>{0,0,255,255});
        History history(std::make_shared<const Document>(ImageExtent{1, 1}, "Layers", red));
        QVERIFY(history.execute(AddLayer{{2, "Blue", blue, 0.5F}}));
        photoastra::render::SkiaRenderer renderer;
        Viewport view;
        std::array<std::uint8_t, 8> frame{};
        const auto render = [&] { return renderer.renderRaster(*history.document(), view, {2, 1, 8, frame, 1}); };
        QVERIFY(render());
        QVERIFY(frame[0] >= 126 && frame[0] <= 129);
        QVERIFY(frame[2] >= 126 && frame[2] <= 129);
        QCOMPARE(frame[4], 48); // Document boundary leaves the surrounding canvas intact.
        QVERIFY(history.execute(SetVisibility{2, false}));
        QVERIFY(render()); QCOMPARE(frame[0], 255); QCOMPARE(frame[2], 0);
        QVERIFY(history.undo());
        QVERIFY(history.execute(MoveLayer{2, 0}));
        QVERIFY(render()); QCOMPARE(frame[0], 255); QCOMPARE(frame[2], 0);
        QVERIFY(history.execute(RemoveLayer{1}));
        QVERIFY(render()); QVERIFY(frame[0] > 0); QVERIFY(frame[2] > frame[0]);
        QCOMPARE(red->pixels()[0], 255); QCOMPARE(blue->pixels()[3], 255);
        QVERIFY(history.execute(SetOpacity{2, 0}));
        QVERIFY(render()); QCOMPARE(frame[0], frame[2]); // Checkerboard for an invisible stack.
    }
    void importLayerIsUndoableAndBusyEditsAreIgnored()
    {
        QTemporaryDir directory;
        QImage image(2, 3, QImage::Format_RGB32);
        image.fill(Qt::blue);
        const auto path = directory.filePath(QStringLiteral("layer.png"));
        QVERIFY(image.save(path));
        photoastra::application::DocumentSession session;
        const auto initial = session.document();
        QVERIFY(session.importLayer(path));
        session.addTransparentLayer();
        session.undo();
        QCOMPARE(session.document(), initial);
        QTRY_VERIFY(!session.busy());
        QCOMPARE(session.document()->extent(), initial->extent());
        QCOMPARE(session.document()->layers().size(), std::size_t{1});
        QCOMPARE(session.document()->layers()[0].image->extent(), (photoastra::core::ImageExtent{2, 3}));
        session.undo(); QCOMPARE(session.document(), initial);
        session.redo(); QCOMPARE(session.document()->layers().size(), std::size_t{1});
        session.undo();
        session.addTransparentLayer();
        QVERIFY(!session.history().canRedo());
        QVERIFY(!session.document()->layers()[0].image);
    }
    void rasterCompositionAndLifetime()
    {
        auto source = std::make_shared<const photoastra::core::RasterImage>(photoastra::core::ImageExtent{2, 2},
            std::vector<std::uint8_t>{255,0,0,255, 0,255,0,255, 0,0,255,255, 0,0,0,0});
        photoastra::core::Document document({2, 2}, "Pixels", source);
        photoastra::render::SkiaRenderer renderer;
        photoastra::core::Viewport view;
        std::array<std::uint8_t, 16> frame{};
        QVERIFY(renderer.renderRaster(document, view, {2, 2, 8, frame, 1}));
        QCOMPARE(frame[0], 255);
        QCOMPARE(frame[1], 0);
        QCOMPARE(frame[5], 255);
        QCOMPARE(frame[10], 255);
        QCOMPARE(frame[15], 255); // Transparency reveals the opaque checkerboard.
        QVERIFY(frame[12] > 150 && frame[12] == frame[13]);
        QCOMPARE(source->pixels()[15], 0); // Source alpha remains untouched.
        QVERIFY(!renderer.renderRaster(document, view, {2, 2, 1, frame, 1}));
        QVERIFY(!renderer.renderRaster(document, view, {2, 3, 8, frame, 1}));
        view.pan({100, 100});
        QVERIFY(renderer.renderRaster(document, view, {2, 2, 8, frame, 1}));
        QCOMPARE(frame[0], 48); // No stale image left behind after panning away.
        const std::weak_ptr<const photoastra::core::RasterImage> weak = source;
        source.reset();
        document = photoastra::core::Document({2, 2}, "Empty");
        QVERIFY(!weak.expired()); // Skia must retain the externally owned pixel buffer.
        QVERIFY(renderer.renderRaster(document, view, {2, 2, 8, frame, 1}));
        QVERIFY(weak.expired()); // Replacing content releases the cached source.
        auto replacement = std::make_shared<const photoastra::core::RasterImage>(photoastra::core::ImageExtent{1, 1},
            std::vector<std::uint8_t>{255,255,255,255});
        const photoastra::core::Document next({1, 1}, "Replacement", replacement);
        photoastra::core::Viewport fresh;
        QVERIFY(renderer.renderRaster(next, fresh, {2, 2, 8, frame, 2}));
        QCOMPARE(frame[0], 255);
        QCOMPARE(frame[10], 255); // Logical image at DPR 2 covers four physical pixels.
    }
};
QTEST_GUILESS_MAIN(ImagePipelineTests)
#include "ImagePipelineTests.moc"
