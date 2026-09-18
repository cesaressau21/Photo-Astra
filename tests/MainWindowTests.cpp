#include <photoastra/core/Document.h>
#include <photoastra/render/Renderer.h>
#include <photoastra/render/SkiaRenderer.h>
#include <photoastra/application/DocumentSession.h>
#include <photoastra/ui/CanvasWidget.h>
#include <photoastra/core/RasterImage.h>
#include <photoastra/ui/MainWindow.h>

#include <QAction>
#include <QDockWidget>
#include <QListWidget>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>
#include <QTest>
#include <QToolBar>
#include <QTemporaryDir>
#include <QSignalSpy>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QSurfaceFormat>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QMessageBox>
#include <QTimer>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QComboBox>
#include <QCheckBox>
#include <cmath>
#include <photoastra/io/ProjectFile.h>

namespace {
bool enterDialogFile(QFileDialog* dialog, const QString& path)
{
    // QFileDialog::selectFile ignores a visible filename field that already has focus.
    auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit"));
    if (!name) return false;
    name->setText(path);
    return true;
}
}

class RefusingGlRenderer final : public photoastra::render::Renderer {
public:
    explicit RefusingGlRenderer(std::shared_ptr<bool> attempted) : attempted_(std::move(attempted)) {}
    photoastra::render::RendererInfo info() const noexcept override { return cpu_.info(); }
    bool initializeOpenGl(void*, photoastra::render::GlResolver) override { *attempted_ = true; return false; }
    bool renderRaster(const photoastra::core::Document& doc, const photoastra::core::Viewport& view,
                      photoastra::render::RasterTarget target) override { return cpu_.renderRaster(doc, view, target); }
private:
    std::shared_ptr<bool> attempted_;
    photoastra::render::SkiaRenderer cpu_;
};

class MainWindowTests final : public QObject {
    Q_OBJECT
private slots:
    void layerBlendControlsAndCanvas()
    {
        using namespace photoastra;
        QTemporaryDir directory;
        const auto bottomPath = directory.filePath(QStringLiteral("bottom.png"));
        const auto topPath = directory.filePath(QStringLiteral("top.png"));
        QImage fixture(64, 64, QImage::Format_RGBA8888);
        fixture.fill(QColor(128, 64, 192)); QVERIFY(fixture.save(bottomPath));
        fixture.fill(QColor(64, 192, 128)); QVERIFY(fixture.save(topPath));
        application::DocumentSession session;
        session.openImage(bottomPath); QTRY_VERIFY(!session.busy());
        session.importLayer(topPath); QTRY_VERIFY(!session.busy());
        QCOMPARE(session.document()->layers().size(), std::size_t(2));
        ui::MainWindow window(session, std::make_unique<render::SkiaRenderer>());
        window.show(); QTRY_VERIFY(window.isVisible());
        auto* canvas = window.findChild<ui::CanvasWidget*>(QStringLiteral("canvas")); QVERIFY(canvas);
        canvas->actualSize();
        auto* combo = window.findChild<QComboBox*>(QStringLiteral("layerBlendMode")); QVERIFY(combo && combo->isEnabled());
        const auto original = session.document();
        const auto pixel = [&] {
            QTest::qWait(50);
            auto* surface = canvas->findChild<QWidget*>(QStringLiteral("canvasSurface"));
            auto* gl = qobject_cast<QOpenGLWidget*>(surface);
            const auto frame = gl ? gl->grabFramebuffer() : surface->grab().toImage();
            const auto point = canvas->viewport().imageToView({32, 32});
            const double dpr = frame.width() / double(surface->width());
            return frame.pixelColor(qRound(point.x * dpr), qRound(point.y * dpr));
        };
        if (qEnvironmentVariableIsSet("PHOTO_ASTRA_REQUIRE_GPU")) QTRY_VERIFY(canvas->rendererInfo().gpuAccelerated);
        for (const auto mode : {core::BlendMode::Multiply, core::BlendMode::Screen}) {
            combo->setCurrentIndex(combo->findData(static_cast<int>(mode)));
            QCOMPARE(session.document()->layers()[1].blendMode, mode);
            const QColor expected = mode == core::BlendMode::Multiply ? QColor(32, 48, 96) : QColor(160, 208, 224);
            const auto actual = pixel();
            QVERIFY(std::abs(actual.red() - expected.red()) <= 2);
            QVERIFY(std::abs(actual.green() - expected.green()) <= 2);
            QVERIFY(std::abs(actual.blue() - expected.blue()) <= 2);
            const auto exportPath = directory.filePath(QStringLiteral("blend.png"));
            session.exportImage(exportPath, io::ExportFormat::Png); QTRY_VERIFY(!session.busy());
            const auto exported = QImage(exportPath).pixelColor(32, 32);
            QVERIFY(std::abs(exported.red() - actual.red()) <= 2);
            QVERIFY(std::abs(exported.green() - actual.green()) <= 2);
            QVERIFY(std::abs(exported.blue() - actual.blue()) <= 2);
            session.undo(); QCOMPARE(session.document(), original);
            QTRY_COMPARE(combo->currentData().toInt(), static_cast<int>(core::BlendMode::Normal));
            session.redo(); QTRY_COMPARE(combo->currentData().toInt(), static_cast<int>(mode));
            session.edit(core::SetVisibility{original->layers()[0].id, false});
            const auto alone = pixel();
            QVERIFY(std::abs(alone.red() - 64) <= 1); QVERIFY(std::abs(alone.green() - 192) <= 1);
            session.undo(); session.undo(); QCOMPARE(session.document(), original);
            QCoreApplication::processEvents();
        }
        session.undo(); // Undo the initial import, returning to the clean opened document.
        QVERIFY(window.close());
    }
    void scalePreviewAcceptCancel()
    {
        using namespace photoastra;
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("scale-source.png"));
        QImage source(64, 64, QImage::Format_RGBA8888); source.fill(Qt::red); QVERIFY(source.save(path));
        application::DocumentSession session;
        session.openImage(path); QTRY_VERIFY(!session.busy());
        const auto original = session.document(); QVERIFY(original->layers().size() == 1);
        ui::MainWindow window(session, std::make_unique<render::SkiaRenderer>());
        window.show(); QTRY_VERIFY(window.isVisible());
        auto* canvas = window.findChild<ui::CanvasWidget*>(QStringLiteral("canvas")); QVERIFY(canvas);
        canvas->actualSize();
        auto* button = window.findChild<QPushButton*>(QStringLiteral("scaleLayerButton")); QVERIFY(button && button->isEnabled());
        for (const bool accept : {false, true}) {
            QTimer::singleShot(0, &window, [&] {
                auto* dialog = window.findChild<QDialog*>(QStringLiteral("layerScaleDialog"));
                if (!dialog) { QFAIL("Scale dialog missing"); }
                // Ensure assertion failures also close the nested event loop.
                QTimer::singleShot(3000, dialog, &QDialog::reject);
                auto* x = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("layerScaleX"));
                auto* y = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("layerScaleY"));
                QVERIFY(x && y);
                x->setValue(75); x->setValue(50); QCOMPARE(y->value(), 50.0);
                QCOMPARE(session.document(), original); QVERIFY(!session.history().canUndo());
                QTest::qWait(50);
                auto* surface = canvas->findChild<QWidget*>(QStringLiteral("canvasSurface"));
                QVERIFY(surface);
                auto* gl = qobject_cast<QOpenGLWidget*>(surface);
                if (qEnvironmentVariableIsSet("PHOTO_ASTRA_REQUIRE_GPU")) QVERIFY(gl && canvas->rendererInfo().gpuAccelerated);
                const auto frame = gl ? gl->grabFramebuffer() : surface->grab().toImage();
                const auto point = canvas->viewport().imageToView({48, 16});
                const double dpr = frame.width() / double(surface->width());
                QVERIFY(frame.pixelColor(qRound(point.x * dpr), qRound(point.y * dpr)).green() > 100);
                const auto screenshot = qEnvironmentVariable("PHOTO_ASTRA_SCALE_SCREENSHOT");
                if (!screenshot.isEmpty()) {
                    QVERIFY(window.grab().save(screenshot));
                    QVERIFY(dialog->grab().save(screenshot + QStringLiteral(".dialog.png")));
                }
                if (accept) dialog->accept(); else QTest::keyClick(dialog, Qt::Key_Escape);
            });
            button->click();
            if (accept) {
                QVERIFY(session.document()->layers()[0].scale == (core::LayerScale{0.5, 0.5}));
                QCOMPARE(session.document()->layers()[0].image, original->layers()[0].image);
                session.undo(); QCOMPARE(session.document(), original); QVERIFY(!session.history().canUndo());
                session.redo(); QVERIFY(session.document()->layers()[0].scale == (core::LayerScale{0.5, 0.5}));
                session.undo();
            } else QCOMPARE(session.document(), original);
        }
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = window.findChild<QDialog*>(QStringLiteral("layerScaleDialog")); QVERIFY(dialog);
            QTimer::singleShot(3000, dialog, &QDialog::reject);
            auto* x = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("layerScaleX"));
            auto* y = dialog->findChild<QDoubleSpinBox*>(QStringLiteral("layerScaleY"));
            dialog->findChild<QCheckBox*>(QStringLiteral("linkedScale"))->setChecked(false);
            x->setValue(150); QCOMPARE(y->value(), 100.0);
            y->setValue(75); QCOMPARE(x->value(), 150.0);
            dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Reset)->click();
            QCOMPARE(x->value(), 100.0); QCOMPARE(y->value(), 100.0);
            dialog->accept();
        });
        button->click();
        QCOMPARE(session.document(), original); QVERIFY(!session.history().canUndo());
        QVERIFY(window.close());
    }
    void shellLifecycle()
    {
        photoastra::application::DocumentSession session;
        photoastra::ui::MainWindow window(session, std::make_unique<photoastra::render::SkiaRenderer>());
        window.show();
        QTRY_VERIFY(window.isVisible());
        QVERIFY(window.windowTitle().contains(QStringLiteral("Sin título")));
        QVERIFY(window.centralWidget());
        QCOMPARE(window.centralWidget()->objectName(), QStringLiteral("canvas"));
        QCOMPARE(window.menuBar()->actions().size(), 4);
        QVERIFY(window.findChild<QToolBar*>(QStringLiteral("mainToolbar")));
        QVERIFY(window.statusBar()->isVisible());
        auto* layers = window.findChild<QDockWidget*>(QStringLiteral("layersDock"));
        QVERIFY(layers);
        QVERIFY(layers->isVisible());
        layers->toggleViewAction()->trigger();
        QVERIFY(!layers->isVisible());
        layers->toggleViewAction()->trigger();
        QVERIFY(layers->isVisible());
        auto* list = window.findChild<QListWidget*>(QStringLiteral("layerList"));
        QVERIFY(list);
        QCOMPARE(list->count(), 0);
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("inspectorTabs"));
        QVERIFY(tabs);
        QCOMPARE(tabs->count(), 2);
        tabs->setCurrentIndex(1);
        QCOMPARE(tabs->currentIndex(), 1);
        tabs->setCurrentIndex(0);
        QVERIFY(window.findChild<QAction*>(QStringLiteral("openAction"))->isEnabled());
        QVERIFY(window.findChild<QAction*>(QStringLiteral("newAction"))->isEnabled());
        QVERIFY(window.findChild<QAction*>(QStringLiteral("exportAction"))->isEnabled());
        for (const auto* name : {"undoAction", "redoAction"}) {
            auto* action = window.findChild<QAction*>(QString::fromLatin1(name));
            QVERIFY(action);
            QVERIFY(!action->isEnabled());
        }
        const auto screenshot = qEnvironmentVariable("PHOTO_ASTRA_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) {
            QVERIFY(window.grab().save(screenshot));
        }
        QVERIFY(window.close());
        QVERIFY(!window.isVisible());
    }
    void newDocumentDialogAndExport()
    {
        photoastra::application::DocumentSession session;
        photoastra::ui::MainWindow window(session, std::make_unique<photoastra::render::SkiaRenderer>());
        window.show();
        QTimer::singleShot(0, &window, [&window] {
            auto* dialog = window.findChild<QDialog*>(QStringLiteral("newDocumentDialog"));
            QVERIFY(dialog);
            auto* width = dialog->findChild<QSpinBox*>(QStringLiteral("documentWidth"));
            auto* height = dialog->findChild<QSpinBox*>(QStringLiteral("documentHeight"));
            auto* buttons = dialog->findChild<QDialogButtonBox*>();
            width->setValue(32768); height->setValue(32768);
            QVERIFY(!buttons->button(QDialogButtonBox::Ok)->isEnabled());
            width->setValue(320); height->setValue(240);
            dialog->findChild<QLineEdit*>(QStringLiteral("documentName"))->setText(QStringLiteral("Nuevo"));
            QTest::mouseClick(buttons->button(QDialogButtonBox::Ok), Qt::LeftButton);
        });
        window.findChild<QAction*>(QStringLiteral("newAction"))->trigger();
        QCOMPARE(session.document()->extent(), (photoastra::core::ImageExtent{320, 240}));
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("ui-export.png"));
        QTimer::singleShot(0, &window, [&window, path] {
            auto* dialog = window.findChild<QFileDialog*>(QStringLiteral("exportDialog"));
            QVERIFY(dialog);
            QVERIFY(enterDialogFile(dialog, path));
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        });
        QSignalSpy exported(&session, &photoastra::application::DocumentSession::exportFinished);
        window.findChild<QAction*>(QStringLiteral("exportAction"))->trigger();
        QTRY_COMPARE(exported.count(), 1);
        QCOMPARE(QImage(path).size(), QSize(320, 240));
        QCOMPARE(QImage(path).pixelColor(0, 0).alpha(), 0);
        QVERIFY(window.close());
    }
    void exportDialogSafety_data()
    {
        QTest::addColumn<int>("scenario");
        QTest::newRow("cancel") << 0;
        QTest::newRow("wrong-extension") << 1;
        QTest::newRow("decline-overwrite") << 2;
        QTest::newRow("confirm-overwrite") << 3;
        QTest::newRow("default-png-extension") << 4;
        QTest::newRow("default-jpeg-extension") << 5;
    }
    void exportDialogSafety()
    {
        QFETCH(int, scenario);
        photoastra::application::DocumentSession session;
        QVERIFY(session.createDocument({8, 8}, "Export"));
        photoastra::ui::MainWindow window(session, std::make_unique<photoastra::render::SkiaRenderer>());
        window.show();
        QTemporaryDir directory;
        const auto path = directory.filePath(scenario == 1 ? QStringLiteral("existing.bmp") : QStringLiteral("existing.png"));
        const QByteArray original("existing file must survive cancellation");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QCOMPARE(file.write(original), original.size()); file.close();
        QSignalSpy exported(&session, &photoastra::application::DocumentSession::exportFinished);
        bool configured = false, sawMessage = false, timedOut = false;
        QTimer driver;
        connect(&driver, &QTimer::timeout, &window, [&] {
            if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                sawMessage = true;
                if (scenario == 1) QCOMPARE(message->windowTitle(), QStringLiteral("Extensión incompatible"));
                const auto answer = scenario == 2 ? QMessageBox::No : scenario == 3 ? QMessageBox::Yes : QMessageBox::Ok;
                auto* button = message->button(answer);
                if (button) QTest::mouseClick(button, Qt::LeftButton);
                else message->reject();
                return;
            }
            auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
            if (!dialog) return;
            if (scenario == 0 || (scenario == 2 && sawMessage)) { dialog->reject(); return; }
            if (configured) return;
            configured = true;
            if (scenario == 5) {
                auto* types = dialog->findChild<QComboBox*>(QStringLiteral("fileTypeCombo"));
                QVERIFY(types);
                QTest::keyClick(types, Qt::Key_End);
            }
            QVERIFY(enterDialogFile(dialog, scenario >= 4 ? directory.filePath(QStringLiteral("no-extension")) :
                scenario == 1 ? directory.filePath(QStringLiteral("wrong.bmp")) : path));
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &window, [&] {
            timedOut = true;
            for (auto* dialog : window.findChildren<QDialog*>()) dialog->reject();
        });
        driver.start(10); watchdog.start(5000);
        window.findChild<QAction*>(QStringLiteral("exportAction"))->trigger();
        driver.stop(); watchdog.stop();
        QVERIFY(!timedOut);
        if (scenario >= 3) {
            QTRY_COMPARE(exported.count(), 1);
            const auto output = scenario >= 4 ? directory.filePath(scenario == 5 ? QStringLiteral("no-extension.jpg") : QStringLiteral("no-extension.png")) : path;
            const QImage image(output);
            QCOMPARE(image.size(), QSize(8, 8));
            if (scenario == 5) QCOMPARE(image.pixelColor(0, 0), QColor(Qt::white));
        } else {
            QCOMPARE(exported.count(), 0);
            QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(), original);
        }
        if (scenario >= 1 && scenario <= 3) QVERIFY(sawMessage);
        QVERIFY(window.close());
    }
    void effectControlsCanvasAndExport()
    {
        using namespace photoastra;
        QTemporaryDir directory;
        QImage fixture(64, 64, QImage::Format_RGB32); fixture.fill(QColor(128, 128, 128));
        const auto path = directory.filePath(QStringLiteral("gray.png")); QVERIFY(fixture.save(path));
        application::DocumentSession session;
        ui::MainWindow window(session, std::make_unique<render::SkiaRenderer>());
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(session.openImage(path)); QTRY_VERIFY(!session.busy());
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("inspectorTabs")); tabs->setCurrentIndex(1);
        auto* add = window.findChild<QPushButton*>(QStringLiteral("addExposureButton"));
        QTRY_VERIFY(add->isEnabled()); QTest::mouseClick(add, Qt::LeftButton);
        auto* list = window.findChild<QListWidget*>(QStringLiteral("effectList")); QTRY_COMPARE(list->count(), 1);
        auto* exposure = window.findChild<QDoubleSpinBox*>(QStringLiteral("exposureStops")); exposure->setValue(1);
        QTRY_COMPARE(session.document()->layers()[0].effects[0].exposureStops, 1.0F);
        auto* canvas = window.findChild<ui::CanvasWidget*>();
        auto* surface = canvas->findChild<QWidget*>(QStringLiteral("canvasSurface"));
        if (qEnvironmentVariableIsSet("PHOTO_ASTRA_REQUIRE_GPU")) QVERIFY(canvas->rendererInfo().gpuAccelerated);
        QTest::qWait(100);
        auto* gl = qobject_cast<QOpenGLWidget*>(surface);
        const auto frame = gl ? gl->grabFramebuffer() : surface->grab().toImage();
        const auto point = canvas->viewport().imageToView({32, 32});
        const auto ratio = frame.width() / static_cast<double>(surface->width());
        const auto shade = frame.pixelColor(qRound(point.x * ratio), qRound(point.y * ratio)).red();
        QVERIFY(shade >= 173 && shade <= 178);
        const auto screenshot = qEnvironmentVariable("PHOTO_ASTRA_EFFECT_SCREENSHOT");
        if (!screenshot.isEmpty()) QVERIFY(window.grab().save(screenshot));
        const auto output = directory.filePath(QStringLiteral("exposure.png"));
        QSignalSpy exported(&session, &application::DocumentSession::exportFinished);
        QVERIFY(session.exportImage(output, io::ExportFormat::Png)); QTRY_COMPARE(exported.count(), 1);
        QVERIFY(std::abs(QImage(output).pixelColor(32, 32).red() - shade) <= 2);
        list->item(0)->setCheckState(Qt::Unchecked);
        QVERIFY(!session.document()->layers()[0].effects[0].enabled);
        session.undo(); QTRY_VERIFY(session.document()->layers()[0].effects[0].enabled);
        QTest::mouseClick(add, Qt::LeftButton); QTRY_COMPARE(list->count(), 2);
        const auto secondId = session.document()->layers()[0].effects[1].id;
        list->setCurrentRow(1);
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("raiseEffectButton")), Qt::LeftButton);
        QTRY_COMPARE(session.document()->layers()[0].effects[0].id, secondId);
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("removeEffectButton")), Qt::LeftButton);
        QTRY_COMPARE(list->count(), 1);
        session.undo(); QTRY_COMPARE(list->count(), 2);
        while (session.history().canUndo()) session.undo();
        QTRY_COMPARE(list->count(), 0); QVERIFY(window.close());
    }
    void saveProjectFromUiAndReopen()
    {
        using namespace photoastra;
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("editable.pastra"));
        application::DocumentSession session;
        session.addTransparentLayer(); session.addExposure(session.document()->layers()[0].id);
        ui::MainWindow window(session, std::make_unique<render::SkiaRenderer>());
        window.show();
        QTimer::singleShot(0, &window, [&] {
            auto* dialog = window.findChild<QFileDialog*>(QStringLiteral("saveProjectDialog"));
            QVERIFY(dialog); QVERIFY(enterDialogFile(dialog, directory.filePath(QStringLiteral("editable"))));
            QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection);
        });
        QSignalSpy saved(&session, &application::DocumentSession::projectSaved);
        window.findChild<QAction*>(QStringLiteral("saveAction"))->trigger();
        QTRY_COMPARE(saved.count(), 1);
        QCOMPARE(session.projectPath(), path); QVERIFY(!session.history().dirty());
        QVERIFY(!window.windowTitle().contains(QLatin1Char('*')));
        session.addTransparentLayer();
        window.findChild<QAction*>(QStringLiteral("saveAction"))->trigger();
        QTRY_COMPARE(saved.count(), 2);
        QVERIFY(session.createDocument({4, 4}, "Replacement"));
        QVERIFY(session.openImage(path)); QTRY_VERIFY(!session.busy());
        QCOMPARE(session.document()->layers().size(), std::size_t{2});
        QCOMPARE(session.document()->layers()[0].effects.size(), std::size_t{1});
        QVERIFY(window.close());
    }
    void closeWithProjectChanges_data()
    {
        QTest::addColumn<int>("scenario");
        QTest::newRow("save-and-close") << 0;
        QTest::newRow("cancel-save-dialog") << 1;
        QTest::newRow("discard-and-close") << 2;
        QTest::newRow("failed-save-keeps-window") << 3;
    }
    void closeWithProjectChanges()
    {
        using namespace photoastra;
        QFETCH(int, scenario);
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("close.pastra"));
        application::DocumentSession session;
        if (scenario == 3) {
            QVERIFY(session.saveDocument(path)); QTRY_VERIFY(!session.busy());
            QVERIFY(QFile::remove(path)); QVERIFY(QDir().mkdir(path)); // Replace the destination with a directory to force failure.
        }
        session.addTransparentLayer();
        ui::MainWindow window(session, std::make_unique<render::SkiaRenderer>());
        window.show();
        bool sawSave = false, sawError = false, timedOut = false;
        QTimer driver;
        connect(&driver, &QTimer::timeout, &window, [&] {
            if (auto* message = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                if (message->standardButtons().testFlag(QMessageBox::Save))
                    QTest::mouseClick(message->button(scenario == 2 ? QMessageBox::Discard : QMessageBox::Save), Qt::LeftButton);
                else { sawError = true; message->accept(); }
            } else if (auto* dialog = qobject_cast<QFileDialog*>(QApplication::activeModalWidget())) {
                if (sawSave) return;
                sawSave = true;
                if (scenario == 1) dialog->reject();
                else { QVERIFY(enterDialogFile(dialog, path)); QMetaObject::invokeMethod(dialog, "accept", Qt::QueuedConnection); }
            }
        });
        QTimer watchdog;
        watchdog.setSingleShot(true);
        connect(&watchdog, &QTimer::timeout, &window, [&] {
            timedOut = true; for (auto* dialog : window.findChildren<QDialog*>()) dialog->reject();
        });
        driver.start(10); watchdog.start(5000);
        window.close();
        if (scenario == 0 || scenario == 2) QTRY_VERIFY(!window.isVisible());
        else if (scenario == 3) { QTRY_VERIFY(sawError); QTRY_VERIFY(!session.busy()); }
        driver.stop(); watchdog.stop(); QVERIFY(!timedOut);
        if (scenario == 0) {
            QVERIFY(!session.history().dirty()); std::atomic_bool cancel{false};
            QVERIFY(io::loadProject(path, cancel).document);
        } else if (scenario != 2) {
            QVERIFY(window.isVisible()); QVERIFY(session.history().dirty());
            session.undo(); QVERIFY(window.close());
        }
    }
    void dragLayerPreviewCommitCancel()
    {
        using namespace photoastra;
        QTemporaryDir directory;
        QImage fixture(64, 64, QImage::Format_RGB32); fixture.fill(Qt::red);
        const auto path = directory.filePath(QStringLiteral("drag.png")); QVERIFY(fixture.save(path));
        application::DocumentSession session;
        ui::MainWindow window(session, std::make_unique<render::SkiaRenderer>());
        window.show(); QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(session.openImage(path)); QTRY_VERIFY(!session.busy());
        QCoreApplication::processEvents();
        auto* canvas = window.findChild<ui::CanvasWidget*>(); canvas->actualSize();
        auto* surface = canvas->findChild<QWidget*>(QStringLiteral("canvasSurface"));
        const auto center = canvas->viewport().imageToView({32, 32});
        QWheelEvent wheel(QPointF(center.x, center.y), surface->mapToGlobal(QPoint(qRound(center.x), qRound(center.y))),
            {}, {0, 120}, Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(surface, &wheel);
        const auto origin = session.document();
        const auto point = canvas->viewport().imageToView({16, 16});
        const QPoint start(qRound(point.x), qRound(point.y));
        const auto move = [&](QPoint end) {
            QMouseEvent event(QEvent::MouseMove, QPointF(end), surface->mapToGlobal(end), Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
            QApplication::sendEvent(surface, &event);
        };
        QTest::mousePress(surface, Qt::LeftButton, Qt::NoModifier, start);
        move(start + QPoint(12, 12)); move(start + QPoint(24, 12));
        QCOMPARE(session.document(), origin); QVERIFY(!session.history().canUndo());
        QTest::qWait(50);
        const auto sample = canvas->viewport().imageToView({4, 4});
        auto* gl = qobject_cast<QOpenGLWidget*>(surface);
        if (qEnvironmentVariableIsSet("PHOTO_ASTRA_REQUIRE_GPU")) QVERIFY(gl && canvas->rendererInfo().gpuAccelerated);
        const auto frame = gl ? gl->grabFramebuffer() : surface->grab().toImage();
        const auto dpr = frame.width() / static_cast<double>(surface->width());
        const auto vacated = frame.pixelColor(qRound(sample.x * dpr), qRound(sample.y * dpr));
        QVERIFY(vacated.green() > 100); // Preview already exposes the checkerboard at the old position.
        QTest::mouseRelease(surface, Qt::LeftButton, Qt::NoModifier, start + QPoint(24, 12));
        QCOMPARE(session.document()->layers()[0].position.x, 20);
        QCOMPARE(session.document()->layers()[0].position.y, 10);
        session.undo(); QCOMPARE(session.document(), origin); QVERIFY(!session.history().canUndo());
        session.redo(); QCOMPARE(session.document()->layers()[0].position.x, 20);
        session.undo(); QCoreApplication::processEvents();
        QTest::mousePress(surface, Qt::LeftButton, Qt::NoModifier, start); move(start + QPoint(24, 12));
        QTest::keyClick(surface, Qt::Key_Escape);
        QTest::mouseRelease(surface, Qt::LeftButton, Qt::NoModifier, start + QPoint(24, 12));
        QCOMPARE(session.document(), origin); QVERIFY(!session.history().canUndo());
        QTest::mousePress(surface, Qt::LeftButton, Qt::NoModifier, start); move(start + QPoint(24, 12));
        QFocusEvent lost(QEvent::FocusOut); QApplication::sendEvent(surface, &lost);
        QTest::mouseRelease(surface, Qt::LeftButton, Qt::NoModifier, start + QPoint(24, 12));
        QCOMPARE(session.document(), origin);
        QTest::mouseClick(surface, Qt::LeftButton, Qt::NoModifier, start);
        QVERIFY(!session.history().canUndo());
        const auto exported = directory.filePath(QStringLiteral("moved.png"));
        session.redo();
        QVERIFY(session.exportImage(exported, io::ExportFormat::Png)); QTRY_VERIFY(!session.busy());
        QCOMPARE(QImage(exported).pixelColor(0, 0).alpha(), 0);
        QCOMPARE(QImage(exported).pixelColor(21, 11), QColor(Qt::red));
        session.undo(); QVERIFY(window.close());
    }
    void layerPositionControls()
    {
        using namespace photoastra;
        application::DocumentSession session;
        session.addTransparentLayer();
        ui::MainWindow window(session, std::make_unique<render::SkiaRenderer>());
        window.show();
        auto* canvas = window.findChild<ui::CanvasWidget*>(); canvas->actualSize();
        auto* x = window.findChild<QSpinBox*>(QStringLiteral("layerPositionX"));
        auto* y = window.findChild<QSpinBox*>(QStringLiteral("layerPositionY"));
        QVERIFY(x->isEnabled()); x->setValue(-12); y->setValue(23);
        QTRY_COMPARE(session.document()->layers()[0].position.x, -12);
        QCOMPARE(session.document()->layers()[0].position.y, 23);
        QCOMPARE(canvas->viewport().zoom(), 1.0);
        session.undo(); QTRY_COMPARE(y->value(), 0); QCOMPARE(x->value(), -12);
        session.redo(); QTRY_COMPARE(y->value(), 23);
        while (session.history().canUndo()) session.undo();
        QTRY_VERIFY(!x->isEnabled()); QVERIFY(window.close());
    }
    void imageCanvasInteraction()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QImage fixture(640, 480, QImage::Format_RGBA8888);
        fixture.fill(Qt::transparent);
        for (int y = 0; y < 480; ++y) {
            for (int x = 0; x < 640; ++x) {
                if (x < 320) fixture.setPixelColor(x, y, y < 240 ? Qt::red : Qt::blue);
                else if (y < 240) fixture.setPixelColor(x, y, Qt::green);
            }
        }
        const auto path = directory.filePath(QStringLiteral("prueba-ñ.png"));
        QVERIFY(fixture.save(path));
        photoastra::application::DocumentSession session;
        photoastra::ui::MainWindow window(session, std::make_unique<photoastra::render::SkiaRenderer>());
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QSignalSpy loaded(&session, &photoastra::application::DocumentSession::documentChanged);
        QVERIFY(session.openImage(path));
        QTRY_COMPARE_WITH_TIMEOUT(loaded.count(), 1, 10000);
        auto* canvas = window.findChild<photoastra::ui::CanvasWidget*>();
        QVERIFY(canvas);
        auto* surface = canvas->findChild<QWidget*>(QStringLiteral("canvasSurface"));
        QVERIFY(surface);
        QTRY_VERIFY(canvas->rendererInfo().available);
        QTest::qWait(100);
        if (qEnvironmentVariableIsSet("PHOTO_ASTRA_REQUIRE_GPU")) QVERIFY(canvas->rendererInfo().gpuAccelerated);
        if (auto* gl = qobject_cast<QOpenGLWidget*>(surface)) {
            gl->makeCurrent();
            qInfo() << "OpenGL renderer:" << reinterpret_cast<const char*>(gl->context()->functions()->glGetString(GL_RENDERER));
            gl->doneCurrent();
        }
        const auto frame = [&]() {
            if (auto* gl = qobject_cast<QOpenGLWidget*>(surface)) return gl->grabFramebuffer();
            return surface->grab().toImage();
        }();
        QVERIFY(!frame.isNull());
        const double dpr = frame.width() / static_cast<double>(surface->width());
        const auto redPoint = canvas->viewport().imageToView({100, 100});
        const auto bluePoint = canvas->viewport().imageToView({100, 350});
        const auto red = frame.pixelColor(qRound(redPoint.x * dpr), qRound(redPoint.y * dpr));
        const auto blue = frame.pixelColor(qRound(bluePoint.x * dpr), qRound(bluePoint.y * dpr));
        QVERIFY2(red.red() > 240 && red.green() < 10 && red.blue() < 10, "Rendered top quadrant must be red");
        QVERIFY2(blue.blue() > 240 && blue.red() < 10, "Rendered bottom quadrant must be blue, not vertically flipped");
        const QPointF anchor(250, 220);
        const auto oldAnchor = canvas->viewport().viewToImage({anchor.x(), anchor.y()});
        const double oldZoom = canvas->viewport().zoom();
        QWheelEvent wheel(anchor, surface->mapToGlobal(anchor.toPoint()), {}, {0, 120}, Qt::NoButton,
            Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(surface, &wheel);
        QVERIFY(canvas->viewport().zoom() > oldZoom);
        const auto newAnchor = canvas->viewport().viewToImage({anchor.x(), anchor.y()});
        QVERIFY(std::abs(newAnchor.x - oldAnchor.x) < 1e-6);
        QVERIFY(std::abs(newAnchor.y - oldAnchor.y) < 1e-6);
        const auto oldOffset = canvas->viewport().offset();
        QTest::mousePress(surface, Qt::MiddleButton, Qt::NoModifier, QPoint(200, 200));
        QMouseEvent move(QEvent::MouseMove, QPointF(230, 240), surface->mapToGlobal(QPoint(230, 240)),
            Qt::NoButton, Qt::MiddleButton, Qt::NoModifier);
        QApplication::sendEvent(surface, &move);
        QTest::mouseRelease(surface, Qt::MiddleButton, Qt::NoModifier, QPoint(230, 240));
        QCOMPARE(canvas->viewport().offset().x, oldOffset.x + 30);
        QCOMPARE(canvas->viewport().offset().y, oldOffset.y + 40);
        const auto beforeSpace = canvas->viewport().offset();
        QTest::keyPress(surface, Qt::Key_Space);
        QTest::mousePress(surface, Qt::LeftButton, Qt::NoModifier, QPoint(230, 240));
        QMouseEvent spaceMove(QEvent::MouseMove, QPointF(210, 230), surface->mapToGlobal(QPoint(210, 230)),
            Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(surface, &spaceMove);
        QTest::mouseRelease(surface, Qt::LeftButton, Qt::NoModifier, QPoint(210, 230));
        QTest::keyRelease(surface, Qt::Key_Space);
        QCOMPARE(canvas->viewport().offset().x, beforeSpace.x - 20);
        QCOMPARE(canvas->viewport().offset().y, beforeSpace.y - 10);
        canvas->actualSize();
        QCOMPARE(canvas->viewport().zoom(), 1.0);
        canvas->fitToWindow();
        const auto zoomBeforeEdit = canvas->viewport().zoom();
        const auto offsetBeforeEdit = canvas->viewport().offset();
        QImage overlay(640, 480, QImage::Format_RGB32);
        overlay.fill(Qt::blue);
        const auto overlayPath = directory.filePath(QStringLiteral("overlay.png"));
        QVERIFY(overlay.save(overlayPath));
        QVERIFY(session.importLayer(overlayPath));
        QTRY_VERIFY(!session.busy());
        auto* list = window.findChild<QListWidget*>(QStringLiteral("layerList"));
        QTRY_COMPARE(list->count(), 2);
        list->setCurrentRow(0);
        auto* opacity = window.findChild<QDoubleSpinBox*>(QStringLiteral("layerOpacity"));
        QVERIFY(opacity);
        opacity->setValue(50);
        QTRY_COMPARE(session.document()->layers()[1].opacity, 0.5F);
        QCOMPARE(canvas->viewport().zoom(), zoomBeforeEdit);
        QCOMPARE(canvas->viewport().offset().x, offsetBeforeEdit.x);
        QCOMPARE(canvas->viewport().offset().y, offsetBeforeEdit.y);
        QTest::qWait(100);
        auto* gl = qobject_cast<QOpenGLWidget*>(surface);
        const auto composed = gl ? gl->grabFramebuffer() : surface->grab().toImage();
        const auto sample = canvas->viewport().imageToView({100, 100});
        const auto purple = composed.pixelColor(qRound(sample.x * dpr), qRound(sample.y * dpr));
        QVERIFY(purple.red() >= 125 && purple.red() <= 130);
        QVERIFY(purple.blue() >= 125 && purple.blue() <= 130);
        QTRY_COMPARE(list->count(), 2);
        list->item(0)->setCheckState(Qt::Unchecked);
        QTRY_VERIFY(!session.document()->layers()[1].visible);
        window.findChild<QAction*>(QStringLiteral("undoAction"))->trigger();
        QTRY_VERIFY(session.document()->layers()[1].visible);
        window.findChild<QAction*>(QStringLiteral("redoAction"))->trigger();
        QTRY_VERIFY(!session.document()->layers()[1].visible);
        session.undo();
        QTest::qWait(1);
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("lowerLayerButton")), Qt::LeftButton);
        QTRY_COMPARE(session.document()->layers().front().name, std::string("overlay.png"));
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("removeLayerButton")), Qt::LeftButton);
        QTRY_COMPARE(list->count(), 1);
        window.findChild<QAction*>(QStringLiteral("undoAction"))->trigger();
        QTRY_COMPARE(list->count(), 2);
        const auto screenshot = qEnvironmentVariable("PHOTO_ASTRA_TEST_SCREENSHOT");
        if (!screenshot.isEmpty()) { QTest::qWait(100); QVERIFY(window.grab().save(screenshot)); }
        const auto fixtureOutput = qEnvironmentVariable("PHOTO_ASTRA_TEST_FIXTURE");
        if (!fixtureOutput.isEmpty()) QVERIFY(fixture.save(fixtureOutput));
        while (session.history().canUndo()) session.undo();
        QVERIFY(!session.history().dirty());
        QVERIFY(window.close());
    }
    void closingDirtyDocumentCanBeCancelled()
    {
        photoastra::application::DocumentSession session;
        photoastra::ui::MainWindow window(session, std::make_unique<photoastra::render::SkiaRenderer>());
        window.show();
        QTest::mouseClick(window.findChild<QPushButton*>(QStringLiteral("addLayerButton")), Qt::LeftButton);
        QVERIFY(session.history().dirty());
        QTimer::singleShot(0, &window, [&window] {
            if (auto* message = window.findChild<QMessageBox*>()) message->done(QMessageBox::Cancel);
        });
        QVERIFY(!window.close());
        QVERIFY(window.isVisible());
        session.undo();
        QVERIFY(window.close());
    }
    void rendererFailureFallsBackToCpu()
    {
        const auto attempted = std::make_shared<bool>(false);
        photoastra::ui::CanvasWidget canvas(std::make_unique<RefusingGlRenderer>(attempted));
        const auto pixels = std::make_shared<const photoastra::core::RasterImage>(photoastra::core::ImageExtent{1, 1},
            std::vector<std::uint8_t>{255, 0, 0, 255});
        canvas.setDocument(std::make_shared<const photoastra::core::Document>(photoastra::core::ImageExtent{1, 1}, "Fallback", pixels));
        canvas.resize(400, 300);
        canvas.show();
        QVERIFY(QTest::qWaitForWindowExposed(&canvas));
        QTRY_VERIFY(canvas.findChild<QOpenGLWidget*>() == nullptr);
        QVERIFY(!canvas.rendererInfo().gpuAccelerated);
        if (qEnvironmentVariableIsSet("PHOTO_ASTRA_REQUIRE_GPU")) QVERIFY(*attempted);
        const auto frame = canvas.grab().toImage();
        QVERIFY(!frame.isNull());
        QVERIFY(canvas.close());
    }
};

int main(int argc, char** argv)
{
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QSurfaceFormat format;
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setStencilBufferSize(8);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication application(argc, argv);
    MainWindowTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "MainWindowTests.moc"
