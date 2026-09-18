#include <photoastra/ui/CanvasWidget.h>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string_view>

namespace photoastra::ui {
namespace {
bool canUseOpenGl()
{
    if (qEnvironmentVariable("PHOTO_ASTRA_RENDERER") == QStringLiteral("cpu") ||
        QGuiApplication::platformName() == QStringLiteral("offscreen")) return false;
    QOpenGLContext probe;
    probe.setFormat(QSurfaceFormat::defaultFormat());
    if (!probe.create()) return false;
    QOffscreenSurface surface;
    surface.setFormat(probe.format());
    surface.create();
    const bool ready = surface.isValid() && probe.makeCurrent(&surface);
    if (ready) probe.doneCurrent();
    return ready;
}
}

class CpuCanvasSurface final : public QWidget {
public:
    explicit CpuCanvasSurface(CanvasWidget& owner) : QWidget(&owner), owner_(owner) {}
protected:
    void paintEvent(QPaintEvent*) override { QPainter painter(this); owner_.paintCpu(painter); }
private:
    CanvasWidget& owner_;
};

class GlCanvasSurface final : public QOpenGLWidget {
public:
    explicit GlCanvasSurface(CanvasWidget& owner) : QOpenGLWidget(&owner), owner_(owner)
    {
        setTextureFormat(GL_RGBA8);
    }
    ~GlCanvasSurface() override { cleanup(); }
protected:
    void initializeGL() override
    {
        connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] { cleanup(); }, Qt::DirectConnection);
        ready_ = owner_.renderer_->initializeOpenGl(context(), [](void* ctx, const char* name) -> render::GlProc {
            // Qt owns the native display. GLX may return stubs for EGL names;
            // do not expose them through Qt's OpenGL-only procedure resolver.
            if (std::string_view(name).starts_with("egl")) return nullptr;
            return static_cast<QOpenGLContext*>(ctx)->getProcAddress(name);
        });
        emit owner_.backendChanged();
        if (!ready_) fallback();
    }
    void paintGL() override
    {
        if (!ready_ || !owner_.document_) return;
        GLint stencil = 0;
        GLint samples = 0;
        context()->functions()->glGetIntegerv(GL_STENCIL_BITS, &stencil);
        context()->functions()->glGetIntegerv(GL_SAMPLES, &samples);
        const double dpr = devicePixelRatioF();
        const bool rendered = owner_.renderer_->renderOpenGl(*owner_.document_, owner_.viewport_, {
            qRound(width() * dpr), qRound(height() * dpr), defaultFramebufferObject(), samples, stencil, dpr});
        if (!rendered) { fallback(); return; }
        QPainter painter(this);
        owner_.paintHint(painter);
    }
private:
    void cleanup()
    {
        // Disconnect before QOpenGLWidget's base destructor destroys its context.
        if (context()) disconnect(context(), nullptr, this, nullptr);
        if (context()) makeCurrent();
        owner_.renderer_->releaseOpenGl(context() && QOpenGLContext::currentContext() == context());
        if (context()) doneCurrent();
        ready_ = false;
    }
    void fallback()
    {
        if (fallbackPending_) return;
        fallbackPending_ = true;
        QTimer::singleShot(0, &owner_, [owner = &owner_] { owner->useCpu(); });
    }
    CanvasWidget& owner_;
    bool ready_ = false;
    bool fallbackPending_ = false;
};

CanvasWidget::CanvasWidget(std::unique_ptr<render::Renderer> renderer, QWidget* parent)
    : QWidget(parent), renderer_(std::move(renderer))
{
    if (!renderer_) throw std::invalid_argument("Canvas requires a renderer");
    setObjectName(QStringLiteral("canvas"));
    setAccessibleName(tr("Lienzo de imagen"));
    setMinimumSize(320, 240);
    auto* box = new QVBoxLayout(this);
    box->setContentsMargins(0, 0, 0, 0);
    if (canUseOpenGl()) surface_ = new GlCanvasSurface(*this);
    else surface_ = new CpuCanvasSurface(*this);
    surface_->setObjectName(QStringLiteral("canvasSurface"));
    surface_->setFocusPolicy(Qt::StrongFocus);
    surface_->installEventFilter(this);
    box->addWidget(surface_);
}
CanvasWidget::~CanvasWidget() { delete surface_; }
void CanvasWidget::useCpu()
{
    if (!qobject_cast<QOpenGLWidget*>(surface_)) return;
    cancelLayerMove();
    delete surface_;
    surface_ = new CpuCanvasSurface(*this);
    surface_->setObjectName(QStringLiteral("canvasSurface"));
    surface_->setFocusPolicy(Qt::StrongFocus);
    surface_->installEventFilter(this);
    layout()->addWidget(surface_);
    surface_->show();
    emit backendChanged();
}
void CanvasWidget::setDocument(std::shared_ptr<const core::Document> document, bool resetView)
{
    cancelLayerMove();
    document_ = std::move(document);
    if (resetView) { fitPending_ = true; fitToWindow(); }
    else updateView();
}
void CanvasWidget::fitToWindow()
{
    cancelLayerMove();
    if (!document_) return;
    viewport_.fit(document_->extent(), surface_->width(), surface_->height());
    fitPending_ = !isVisible();
    updateView();
}
void CanvasWidget::actualSize()
{
    cancelLayerMove();
    viewport_.zoomAt(1.0 / viewport_.zoom(), {surface_->width() / 2.0, surface_->height() / 2.0});
    fitPending_ = false;
    updateView();
}
void CanvasWidget::setSelectedLayer(qulonglong id)
{
    if (selectedLayer_ != id) cancelLayerMove();
    selectedLayer_ = id;
}
void CanvasWidget::setEditingEnabled(bool enabled)
{
    editingEnabled_ = enabled;
    if (!enabled) cancelLayerMove();
}
void CanvasWidget::cancelLayerMove()
{
    if (!layerMove_) return;
    document_ = layerMove_->original();
    layerMove_.reset(); surface_->unsetCursor(); surface_->update();
}
void CanvasWidget::finishLayerMove()
{
    if (!layerMove_) return;
    const auto command = layerMove_->command();
    cancelLayerMove();
    if (command) emit layerMoveRequested(command->id, command->position.x, command->position.y);
}
void CanvasWidget::updateView() { surface_->update(); emit zoomChanged(viewport_.zoom()); }
void CanvasWidget::paintHint(QPainter& painter)
{
    if (!document_) {
        painter.setPen(Qt::white);
        painter.drawText(surface_->rect().adjusted(24, 24, -24, -24), Qt::AlignCenter | Qt::TextWordWrap,
                         tr("Abre una imagen PNG o JPEG\nRueda: zoom · Botón central o Espacio + arrastrar: desplazar"));
    }
}
void CanvasWidget::paintCpu(QPainter& painter)
{
    const double dpr = surface_->devicePixelRatioF();
    const QSize size(qRound(surface_->width() * dpr), qRound(surface_->height() * dpr));
    if (size.width() <= 0 || size.height() <= 0) return;
    if (static_cast<qint64>(size.width()) * size.height() > 64LL * 1024 * 1024) {
        painter.fillRect(surface_->rect(), Qt::darkGray);
        painter.drawText(surface_->rect(), Qt::AlignCenter, tr("Canvas demasiado grande para la memoria disponible."));
        return;
    }
    if (buffer_.size() != size) buffer_ = QImage(size, QImage::Format_RGBA8888_Premultiplied);
    if (buffer_.isNull() || !document_ || !renderer_->renderRaster(*document_, viewport_, {
        size.width(), size.height(), static_cast<std::size_t>(buffer_.bytesPerLine()),
        {buffer_.bits(), static_cast<std::size_t>(buffer_.sizeInBytes())}, dpr})) {
        painter.fillRect(surface_->rect(), Qt::darkGray);
        painter.setPen(Qt::white);
        const auto error = renderer_->lastError();
        painter.drawText(surface_->rect().adjusted(16, 16, -16, -16), Qt::AlignCenter | Qt::TextWordWrap,
            tr("No se pudo renderizar el lienzo. ") + QString::fromUtf8(error.data(), static_cast<qsizetype>(error.size())));
        return;
    }
    buffer_.setDevicePixelRatio(dpr);
    painter.drawImage(QPoint(0, 0), buffer_);
    paintHint(painter);
}
bool CanvasWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched != surface_) return QWidget::eventFilter(watched, event);
    switch (event->type()) {
    case QEvent::Show:
        if (fitPending_) fitToWindow();
        QTimer::singleShot(0, this, [this] {
            if (auto* gl = qobject_cast<QOpenGLWidget*>(surface_); gl && !gl->isValid()) useCpu();
        });
        break;
    case QEvent::Resize: {
        cancelLayerMove();
        auto* resized = static_cast<QResizeEvent*>(event);
        if (fitPending_) fitToWindow();
        else if (resized->oldSize().isValid()) {
            viewport_.pan({(resized->size().width() - resized->oldSize().width()) / 2.0,
                           (resized->size().height() - resized->oldSize().height()) / 2.0});
            updateView();
        }
        break;
    }
    case QEvent::Wheel: {
        if (layerMove_) { event->accept(); return true; }
        if (!document_) break;
        auto* wheel = static_cast<QWheelEvent*>(event);
        const double delta = wheel->angleDelta().y() != 0 ? wheel->angleDelta().y() : wheel->pixelDelta().y();
        viewport_.zoomAt(std::pow(1.2, std::clamp(delta, -2400.0, 2400.0) / 120.0),
                         {wheel->position().x(), wheel->position().y()});
        fitPending_ = false;
        updateView();
        event->accept();
        return true;
    }
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        surface_->setFocus();
        if (mouse->button() == Qt::MiddleButton || (spaceHeld_ && mouse->button() == Qt::LeftButton)) {
            cancelLayerMove();
            dragging_ = true;
            lastPosition_ = mouse->position();
            surface_->setCursor(Qt::ClosedHandCursor);
            return true;
        }
        if (mouse->button() == Qt::LeftButton && editingEnabled_ && !dragging_) {
            auto gesture = std::make_unique<core::LayerMoveGesture>();
            const auto point = viewport_.viewToImage({mouse->position().x(), mouse->position().y()});
            if (gesture->begin(document_, selectedLayer_, point)) {
                layerMove_ = std::move(gesture); surface_->setCursor(Qt::SizeAllCursor); return true;
            }
        }
        break;
    }
    case QEvent::MouseMove:
        if (layerMove_) {
            const auto point = static_cast<QMouseEvent*>(event)->position();
            document_ = layerMove_->update(viewport_.viewToImage({point.x(), point.y()}));
            surface_->update(); return true;
        }
        if (dragging_) {
            const auto position = static_cast<QMouseEvent*>(event)->position();
            const auto delta = position - lastPosition_;
            viewport_.pan({delta.x(), delta.y()});
            lastPosition_ = position;
            fitPending_ = false;
            updateView();
            return true;
        }
        break;
    case QEvent::MouseButtonRelease:
        if (layerMove_ && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
            const auto point = static_cast<QMouseEvent*>(event)->position();
            document_ = layerMove_->update(viewport_.viewToImage({point.x(), point.y()}));
            finishLayerMove(); return true;
        }
        if (dragging_) { dragging_ = false; surface_->setCursor(spaceHeld_ ? Qt::OpenHandCursor : Qt::ArrowCursor); return true; }
        break;
    case QEvent::ShortcutOverride:
        if (layerMove_ && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) { event->accept(); return true; }
        break;
    case QEvent::KeyPress:
        if (layerMove_ && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) { cancelLayerMove(); return true; }
        if (static_cast<QKeyEvent*>(event)->key() == Qt::Key_Space) { spaceHeld_ = true; surface_->setCursor(Qt::OpenHandCursor); return true; }
        break;
    case QEvent::KeyRelease:
        if (auto* key = static_cast<QKeyEvent*>(event); key->key() == Qt::Key_Space) {
            if (!key->isAutoRepeat()) { spaceHeld_ = false; if (!dragging_) surface_->unsetCursor(); }
            return true;
        }
        break;
    case QEvent::FocusOut:
        cancelLayerMove();
        dragging_ = false;
        spaceHeld_ = false;
        surface_->unsetCursor();
        break;
    default: break;
    }
    return QWidget::eventFilter(watched, event);
}
}
