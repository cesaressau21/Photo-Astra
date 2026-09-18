#include <photoastra/application/DocumentSession.h>
#include <QtConcurrentRun>
#include <photoastra/application/ImageExport.h>
#include <photoastra/core/RasterImage.h>
#include <photoastra/render/SkiaRenderer.h>
#include <exception>
#include <photoastra/io/ProjectFile.h>
#include <QFileInfo>

namespace photoastra::application {
DocumentSession::DocumentSession(QObject* parent) : QObject(parent),
    history_(std::make_shared<const core::Document>(core::ImageExtent{1920, 1080}, "Sin título"))
{
    connect(&watcher_, &QFutureWatcher<io::LoadResult>::finished, this, [this] {
        const auto result = watcher_.result();
        const bool cancelled = cancellation_->load(std::memory_order_relaxed) || result.cancelled;
        busy_ = false;
        if (cancelled) emit importCancelled();
        else if (result.document) {
            if (importingLayer_) {
                auto layer = result.document->layers().front();
                layer.id = nextLayerId_++;
                edit(core::AddLayer{std::move(layer)});
            } else {
                history_.reset(result.document);
                projectPath_ = loadingProjectPath_;
                for (const auto& layer : document()->layers()) nextLayerId_ = std::max(nextLayerId_, layer.id + 1);
                emit documentReplaced();
                emit documentChanged();
            }
        }
        else emit importFailed(result.error);
        emit busyChanged(busy_);
    });
    connect(&exportWatcher_, &QFutureWatcher<io::ExportResult>::finished, this, [this] {
        const auto result = exportWatcher_.result();
        busy_ = false;
        if (result.succeeded) emit exportFinished(exportPath_);
        else if (result.cancelled) emit exportCancelled();
        else emit exportFailed(result.error);
        emit busyChanged(busy_);
    });
    connect(&saveWatcher_, &QFutureWatcher<io::ExportResult>::finished, this, [this] {
        const auto result = saveWatcher_.result();
        busy_ = false;
        if (result.succeeded) {
            projectPath_ = savingPath_;
            history_.markSaved();
            emit documentChanged();
            emit projectSaved(projectPath_);
        } else if (result.cancelled) emit projectSaveCancelled();
        else emit projectSaveFailed(result.error);
        emit busyChanged(busy_);
    });
}
bool DocumentSession::saveDocument(const QString& path)
{
    if (busy_ || path.isEmpty()) return false;
    savingPath_ = QFileInfo(path).absoluteFilePath();
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    busy_ = true;
    emit busyChanged(true);
    saveWatcher_.setFuture(QtConcurrent::run([snapshot = document(), path = savingPath_, cancellation = cancellation_] {
        return io::saveProject(*snapshot, path, *cancellation);
    }));
    return true;
}
bool DocumentSession::createDocument(core::ImageExtent extent, const QString& title)
{
    if (busy_ || extent.width == 0 || extent.height == 0 || title.trimmed().isEmpty() ||
        static_cast<std::uint64_t>(extent.width) * extent.height > core::RasterImage::maxPixels) return false;
    history_.reset(std::make_shared<const core::Document>(extent, title.trimmed().toStdString()));
    projectPath_.clear();
    emit documentReplaced();
    emit documentChanged();
    return true;
}
bool DocumentSession::exportImage(const QString& path, io::ExportFormat format)
{
    if (busy_ || path.isEmpty() || (format != io::ExportFormat::Png && format != io::ExportFormat::Jpeg)) return false;
    exportPath_ = path;
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    busy_ = true;
    emit busyChanged(true);
    exportWatcher_.setFuture(QtConcurrent::run([snapshot = document(), path, format, cancellation = cancellation_] {
        try {
            render::SkiaRenderer renderer;
            return exportDocument(*snapshot, renderer, path, format, *cancellation);
        } catch (const std::exception& error) {
            return io::ExportResult{QString::fromUtf8(error.what())};
        } catch (...) {
            return io::ExportResult{QStringLiteral("Error inesperado al preparar la exportación.")};
        }
    }));
    return true;
}
DocumentSession::~DocumentSession() { cancelImport(); }
bool DocumentSession::openImage(const QString& path)
{
    return load(path, false);
}
bool DocumentSession::importLayer(const QString& path)
{
    return load(path, true);
}
void DocumentSession::edit(const core::EditCommand& command)
{
    if (!busy_ && history_.execute(command)) emit documentChanged();
}
void DocumentSession::addTransparentLayer()
{
    if (!busy_) edit(core::AddLayer{{nextLayerId_++, "Capa transparente", {}}});
}
void DocumentSession::addExposure(core::LayerId layerId)
{
    if (busy_) return;
    for (const auto& layer : document()->layers()) {
        if (layer.id != layerId || layer.effects.size() >= core::maxEffectsPerLayer) continue;
        auto stack = layer.effects;
        for (const auto& effect : stack) if (effect.id >= nextEffectId_) nextEffectId_ = effect.id + 1;
        stack.push_back({nextEffectId_++});
        edit(core::SetEffects{layerId, std::move(stack)});
        return;
    }
}
void DocumentSession::undo() { if (!busy_ && history_.undo()) emit documentChanged(); }
void DocumentSession::redo() { if (!busy_ && history_.redo()) emit documentChanged(); }
bool DocumentSession::load(const QString& path, bool asLayer)
{
    if (busy_ || path.isEmpty()) return false;
    cancellation_ = std::make_shared<std::atomic_bool>(false);
    busy_ = true;
    importingLayer_ = asLayer;
    const bool project = !asLayer && QFileInfo(path).suffix().compare(QStringLiteral("pastra"), Qt::CaseInsensitive) == 0;
    loadingProjectPath_ = project ? QFileInfo(path).absoluteFilePath() : QString{};
    emit busyChanged(true);
    watcher_.setFuture(QtConcurrent::run([path, project, cancellation = cancellation_] {
        return project ? io::loadProject(path, *cancellation) : io::loadImage(path, *cancellation);
    }));
    return true;
}
void DocumentSession::cancelImport()
{
    if (cancellation_) cancellation_->store(true, std::memory_order_relaxed);
}
}
