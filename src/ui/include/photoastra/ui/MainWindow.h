#pragma once

#include <QMainWindow>
#include <memory>
#include <functional>

namespace photoastra::core { class Document; }
namespace photoastra::render { class Renderer; }
namespace photoastra::application { class DocumentSession; }
class QMenu;
class QLabel;
class QListWidget;
class QAction;

namespace photoastra::ui {
class CanvasWidget;

class MainWindow final : public QMainWindow {
public:
    MainWindow(application::DocumentSession& session, std::unique_ptr<render::Renderer> renderer,
               QWidget* parent = nullptr);

private:
    void closeEvent(QCloseEvent* event) override;
    void requestReplace(std::function<void()> action);
    bool saveDocument(bool saveAs);
    void createDocument();
    void exportDocument();
    void scaleLayer(qulonglong id);
    void createMenusAndToolbar();
    void createPanels();
    void refreshDocument();
    void refreshBackend();
    application::DocumentSession& session_;
    CanvasWidget* canvas_ = nullptr;
    QMenu* viewMenu_ = nullptr;
    QAction* openAction_ = nullptr;
    QAction* cancelAction_ = nullptr;
    QAction* fitAction_ = nullptr;
    QAction* actualSizeAction_ = nullptr;
    QAction* undoAction_ = nullptr;
    QAction* redoAction_ = nullptr;
    QAction* newAction_ = nullptr;
    QAction* exportAction_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* saveAsAction_ = nullptr;
    std::function<void()> afterSave_;
    bool allowClose_ = false;
    bool resetView_ = true;
    QLabel* titleLabel_ = nullptr;
    QLabel* sizeLabel_ = nullptr;
    QLabel* contentLabel_ = nullptr;
    QLabel* rendererLabel_ = nullptr;
    QLabel* gpuLabel_ = nullptr;
    QLabel* zoomLabel_ = nullptr;
    QLabel* resolutionLabel_ = nullptr;
};

} // namespace photoastra::ui
