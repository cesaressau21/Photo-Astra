#pragma once

#include <QWidget>
#include <QImage>
#include <photoastra/core/Viewport.h>
#include <photoastra/render/Renderer.h>
#include <memory>
#include <photoastra/core/LayerMoveGesture.h>

class QPainter;

namespace photoastra::ui {

class CanvasWidget final : public QWidget {
    Q_OBJECT
public:
    CanvasWidget(std::unique_ptr<render::Renderer> renderer, QWidget* parent = nullptr);
    ~CanvasWidget() override;
    void setDocument(std::shared_ptr<const core::Document> document, bool resetView = true);
    void fitToWindow();
    void actualSize();
    void setSelectedLayer(qulonglong id);
    void setEditingEnabled(bool enabled);
    [[nodiscard]] const core::Viewport& viewport() const noexcept { return viewport_; }
    [[nodiscard]] render::RendererInfo rendererInfo() const noexcept { return renderer_->info(); }
signals:
    void zoomChanged(double zoom);
    void backendChanged();
    void layerMoveRequested(qulonglong id, int x, int y);
protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
private:
    friend class CpuCanvasSurface;
    friend class GlCanvasSurface;
    void useCpu();
    void updateView();
    void paintCpu(QPainter& painter);
    void paintHint(QPainter& painter);
    void cancelLayerMove();
    void finishLayerMove();
    std::unique_ptr<render::Renderer> renderer_;
    std::shared_ptr<const core::Document> document_;
    core::Viewport viewport_;
    QWidget* surface_ = nullptr;
    QImage buffer_;
    bool dragging_ = false;
    bool spaceHeld_ = false;
    bool fitPending_ = true;
    QPointF lastPosition_;
    core::LayerId selectedLayer_ = 0;
    bool editingEnabled_ = true;
    std::unique_ptr<core::LayerMoveGesture> layerMove_;
};

} // namespace photoastra::ui
