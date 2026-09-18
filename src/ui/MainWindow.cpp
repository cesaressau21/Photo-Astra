#include <photoastra/ui/MainWindow.h>

#include <photoastra/core/Document.h>
#include <photoastra/application/DocumentSession.h>
#include <photoastra/render/Renderer.h>
#include <photoastra/ui/CanvasWidget.h>

#include <QAction>
#include <QDockWidget>
#include <QFormLayout>
#include <QFileDialog>
#include <QKeySequence>
#include <QLabel>
#include <photoastra/ui/LayersPanel.h>
#include <photoastra/ui/EffectsPanel.h>
#include <QCloseEvent>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>
#include <utility>
#include <QTimer>

namespace photoastra::ui {
namespace {
QAction* pendingAction(QMenu* menu, const QString& text, const QString& name,
                       const QKeySequence& shortcut = {})
{
    auto* action = menu->addAction(text);
    action->setObjectName(name);
    action->setShortcut(shortcut);
    action->setEnabled(false);
    action->setStatusTip(QObject::tr("Pendiente de implementar en V0.1"));
    return action;
}
}

MainWindow::MainWindow(application::DocumentSession& session, std::unique_ptr<render::Renderer> renderer,
                       QWidget* parent) : QMainWindow(parent), session_(session)
{
    setObjectName(QStringLiteral("mainWindow"));
    resize(1200, 800);
    canvas_ = new CanvasWidget(std::move(renderer), this);
    setCentralWidget(canvas_);
    createMenusAndToolbar();
    createPanels();
    zoomLabel_ = new QLabel(this);
    resolutionLabel_ = new QLabel(this);
    statusBar()->addPermanentWidget(zoomLabel_);
    statusBar()->addPermanentWidget(resolutionLabel_);
    connect(canvas_, &CanvasWidget::zoomChanged, this, [this](double zoom) {
        zoomLabel_->setText(tr("Zoom: %1 %").arg(zoom * 100, 0, 'f', 1));
    });
    connect(canvas_, &CanvasWidget::backendChanged, this, &MainWindow::refreshBackend);
    connect(canvas_, &CanvasWidget::layerMoveRequested, this, [this](qulonglong id, int x, int y) {
        session_.edit(core::SetPosition{id, {x, y}});
    });
    connect(&session_, &application::DocumentSession::documentReplaced, this, [this] { resetView_ = true; });
    connect(&session_, &application::DocumentSession::documentChanged, this, &MainWindow::refreshDocument);
    connect(&session_, &application::DocumentSession::busyChanged, this, [this](bool busy) {
        canvas_->setEditingEnabled(!busy);
        openAction_->setEnabled(!busy);
        newAction_->setEnabled(!busy);
        exportAction_->setEnabled(!busy);
        saveAction_->setEnabled(!busy);
        saveAsAction_->setEnabled(!busy);
        cancelAction_->setEnabled(busy);
        undoAction_->setEnabled(!busy && session_.history().canUndo());
        redoAction_->setEnabled(!busy && session_.history().canRedo());
        if (busy) statusBar()->showMessage(tr("Procesando imagen… Esc para cancelar"));
    });
    connect(&session_, &application::DocumentSession::importFailed, this, [this](const QString& message) {
        QMessageBox::warning(this, tr("No se pudo abrir el archivo"), message);
    });
    connect(&session_, &application::DocumentSession::importCancelled, this, [this] {
        statusBar()->showMessage(tr("Importación cancelada"), 5000);
    });
    connect(&session_, &application::DocumentSession::exportFinished, this, [this](const QString& path) {
        statusBar()->showMessage(tr("Imagen exportada: %1 · Usa Guardar para conservar capas y efectos.").arg(path));
    });
    connect(&session_, &application::DocumentSession::exportFailed, this, [this](const QString& error) {
        statusBar()->showMessage(tr("La exportación falló."));
        QMessageBox::warning(this, tr("No se pudo exportar"), error);
    });
    connect(&session_, &application::DocumentSession::exportCancelled, this, [this] {
        statusBar()->showMessage(tr("Exportación cancelada"));
    });
    connect(&session_, &application::DocumentSession::projectSaved, this, [this](const QString& path) {
        statusBar()->showMessage(tr("Documento guardado: %1").arg(path));
        auto action = std::exchange(afterSave_, {});
        if (action) action();
    });
    connect(&session_, &application::DocumentSession::projectSaveFailed, this, [this](const QString& error) {
        afterSave_ = {};
        statusBar()->showMessage(tr("No se guardó el documento."));
        QMessageBox::warning(this, tr("No se pudo guardar"), error);
    });
    connect(&session_, &application::DocumentSession::projectSaveCancelled, this, [this] {
        afterSave_ = {};
        statusBar()->showMessage(tr("Guardado cancelado"));
    });
    refreshDocument();
    refreshBackend();
}

void MainWindow::createMenusAndToolbar()
{
    auto* file = menuBar()->addMenu(tr("&Archivo"));
    auto* create = pendingAction(file, tr("&Nuevo…"), QStringLiteral("newAction"), QKeySequence::New);
    newAction_ = create;
    create->setEnabled(true);
    create->setStatusTip(tr("Crear un documento transparente"));
    connect(create, &QAction::triggered, this, &MainWindow::createDocument);
    openAction_ = file->addAction(tr("&Abrir…"));
    openAction_->setObjectName(QStringLiteral("openAction"));
    openAction_->setShortcut(QKeySequence::Open);
    connect(openAction_, &QAction::triggered, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, tr("Abrir documento o imagen"), {},
            tr("Documentos e imágenes (*.pastra *.png *.jpg *.jpeg);;Photo Astra (*.pastra);;Imágenes PNG/JPEG (*.png *.jpg *.jpeg)"));
        if (!path.isEmpty()) requestReplace([this, path] { session_.openImage(path); });
    });
    saveAction_ = file->addAction(tr("&Guardar"));
    saveAction_->setObjectName(QStringLiteral("saveAction"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveDocument(false); });
    saveAsAction_ = file->addAction(tr("Guardar &como…"));
    saveAsAction_->setObjectName(QStringLiteral("saveAsAction"));
    saveAsAction_->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction_, &QAction::triggered, this, [this] { saveDocument(true); });
    cancelAction_ = file->addAction(tr("Cancelar operación"));
    cancelAction_->setEnabled(false);
    cancelAction_->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(cancelAction_, &QAction::triggered, &session_, &application::DocumentSession::cancelImport);
    exportAction_ = pendingAction(file, tr("&Exportar…"), QStringLiteral("exportAction"), QKeySequence(QStringLiteral("Ctrl+Alt+E")));
    exportAction_->setEnabled(true);
    exportAction_->setStatusTip(tr("Exportar la composición a PNG/JPEG; no conserva las capas editables"));
    connect(exportAction_, &QAction::triggered, this, &MainWindow::exportDocument);
    file->addSeparator();
    auto* quit = file->addAction(tr("&Salir"));
    quit->setShortcut(QKeySequence::Quit);
    connect(quit, &QAction::triggered, this, &QWidget::close);

    auto* edit = menuBar()->addMenu(tr("&Edición"));
    auto* undo = pendingAction(edit, tr("&Deshacer"), QStringLiteral("undoAction"), QKeySequence::Undo);
    auto* redo = pendingAction(edit, tr("&Rehacer"), QStringLiteral("redoAction"), QKeySequence::Redo);
    undoAction_ = undo;
    redoAction_ = redo;
    connect(undo, &QAction::triggered, &session_, &application::DocumentSession::undo);
    connect(redo, &QAction::triggered, &session_, &application::DocumentSession::redo);
    viewMenu_ = menuBar()->addMenu(tr("&Ver"));
    fitAction_ = viewMenu_->addAction(tr("Ajustar imagen al lienzo"));
    fitAction_->setObjectName(QStringLiteral("fitAction"));
    fitAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(fitAction_, &QAction::triggered, canvas_, &CanvasWidget::fitToWindow);
    actualSizeAction_ = viewMenu_->addAction(tr("Tamaño 100 %"));
    actualSizeAction_->setShortcut(QKeySequence(QStringLiteral("Ctrl+1")));
    connect(actualSizeAction_, &QAction::triggered, canvas_, &CanvasWidget::actualSize);
    viewMenu_->addSeparator();
    auto* help = menuBar()->addMenu(tr("A&yuda"));
    auto* about = help->addAction(tr("Acerca de Photo Astra"));
    connect(about, &QAction::triggered, this, [this] {
        QMessageBox::about(this, tr("Photo Astra"),
            tr("Photo Astra · V0.1 en desarrollo\nC++20 · Qt 6 · Skia m144\n\n"
               "PNG/JPEG, capas raster, zoom y desplazamiento.\nHistorial reversible · Exposición no destructiva (SkSL)."));
    });

    auto* toolbar = addToolBar(tr("Principal"));
    toolbar->setObjectName(QStringLiteral("mainToolbar"));
    toolbar->addAction(create);
    toolbar->addAction(openAction_);
    toolbar->addAction(saveAction_);
    toolbar->addAction(fitAction_);
    toolbar->addAction(actualSizeAction_);
    toolbar->addSeparator();
    toolbar->addAction(undo);
    toolbar->addAction(redo);
    viewMenu_->addAction(toolbar->toggleViewAction());
}

void MainWindow::createPanels()
{
    auto* layersDock = new QDockWidget(tr("Layers / Capas"), this);
    layersDock->setObjectName(QStringLiteral("layersDock"));
    layersDock->setMinimumWidth(250);
    auto* layersPanel = new LayersPanel(session_, layersDock);
    layersDock->setWidget(layersPanel);
    connect(layersPanel, &LayersPanel::scaleLayerRequested, this, &MainWindow::scaleLayer);
    connect(layersPanel, &LayersPanel::selectedLayerChanged, canvas_, &CanvasWidget::setSelectedLayer);
    canvas_->setSelectedLayer(layersPanel->selectedId());
    addDockWidget(Qt::RightDockWidgetArea, layersDock);
    viewMenu_->addAction(layersDock->toggleViewAction());

    auto* inspectorDock = new QDockWidget(tr("Properties / Effects"), this);
    inspectorDock->setObjectName(QStringLiteral("inspectorDock"));
    auto* tabs = new QTabWidget(inspectorDock);
    tabs->setObjectName(QStringLiteral("inspectorTabs"));
    auto* properties = new QWidget(tabs);
    auto* form = new QFormLayout(properties);
    titleLabel_ = new QLabel(properties);
    titleLabel_->setWordWrap(true);
    sizeLabel_ = new QLabel(properties);
    contentLabel_ = new QLabel(properties);
    rendererLabel_ = new QLabel(properties);
    gpuLabel_ = new QLabel(properties);
    form->addRow(tr("Documento"), titleLabel_);
    form->addRow(tr("Dimensiones"), sizeLabel_);
    form->addRow(tr("Contenido"), contentLabel_);
    form->addRow(tr("Renderer"), rendererLabel_);
    form->addRow(tr("GPU"), gpuLabel_);
    auto* effects = new EffectsPanel(session_, tabs);
    connect(layersPanel, &LayersPanel::selectedLayerChanged, effects, &EffectsPanel::setLayer);
    effects->setLayer(layersPanel->selectedId());
    tabs->addTab(properties, tr("Propiedades"));
    tabs->addTab(effects, tr("Efectos"));
    inspectorDock->setWidget(tabs);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    splitDockWidget(layersDock, inspectorDock, Qt::Vertical);
    viewMenu_->addAction(inspectorDock->toggleViewAction());
}

void MainWindow::refreshDocument()
{
    const auto& document = *session_.document();
    const auto extent = document.extent();
    const auto title = QString::fromStdString(document.title());
    setWindowTitle(tr("%1 — Photo Astra · V0.1").arg(title + (session_.history().dirty() ? QStringLiteral(" *") : QString{})));
    titleLabel_->setText(title);
    sizeLabel_->setText(tr("%1 × %2 px").arg(extent.width).arg(extent.height));
    resolutionLabel_->setText(sizeLabel_->text());
    contentLabel_->setText(tr("%1 capas · RGBA8 · sRGB").arg(document.layers().size()));
    fitAction_->setEnabled(true);
    actualSizeAction_->setEnabled(true);
    undoAction_->setEnabled(!session_.busy() && session_.history().canUndo());
    redoAction_->setEnabled(!session_.busy() && session_.history().canRedo());
    canvas_->setDocument(session_.document(), resetView_);
    resetView_ = false;
    statusBar()->showMessage(tr("Arrastrar: mover capa · Rueda: zoom · Espacio o botón central: desplazar · Esc: cancelar movimiento"));
}
void MainWindow::requestReplace(std::function<void()> action)
{
    if (session_.busy()) return;
    if (!session_.history().dirty()) { action(); return; }
    const auto answer = QMessageBox::warning(this, tr("Cambios sin guardar"),
        tr("¿Guardar el documento editable antes de continuar?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel);
    if (answer == QMessageBox::Discard) action();
    else if (answer == QMessageBox::Save && saveDocument(false)) afterSave_ = std::move(action);
}
void MainWindow::closeEvent(QCloseEvent* event)
{
    if (session_.busy()) {
        statusBar()->showMessage(tr("Espera a que termine la operación o cancélala con Escape antes de cerrar."));
        event->ignore();
        return;
    }
    if (allowClose_ || !session_.history().dirty()) { allowClose_ = false; event->accept(); return; }
    event->ignore();
    requestReplace([this] { allowClose_ = true; QTimer::singleShot(0, this, [this] { close(); }); });
}

void MainWindow::refreshBackend()
{
    const auto info = canvas_->rendererInfo();
    rendererLabel_->setText(QString::fromUtf8(info.name.data(), static_cast<qsizetype>(info.name.size())));
    gpuLabel_->setText(info.gpuAccelerated ? tr("OpenGL activo") : tr("CPU"));
}

} // namespace photoastra::ui
