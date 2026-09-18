#pragma once
#include <photoastra/io/ImageLoader.h>
#include <QObject>
#include <QFutureWatcher>
#include <photoastra/core/History.h>
#include <photoastra/io/ImageWriter.h>

namespace photoastra::application {
class DocumentSession final : public QObject {
    Q_OBJECT
public:
    explicit DocumentSession(QObject* parent = nullptr);
    ~DocumentSession() override;
    [[nodiscard]] const std::shared_ptr<const core::Document>& document() const noexcept { return history_.document(); }
    [[nodiscard]] const core::History& history() const noexcept { return history_; }
    [[nodiscard]] bool busy() const noexcept { return busy_; }
    bool openImage(const QString& path);
    bool importLayer(const QString& path);
    bool createDocument(core::ImageExtent extent, const QString& title);
    bool exportImage(const QString& path, io::ExportFormat format);
    bool saveDocument(const QString& path);
    const QString& projectPath() const noexcept { return projectPath_; }
    void addTransparentLayer();
    void addExposure(core::LayerId layerId);
    void edit(const core::EditCommand& command);
    void undo();
    void redo();
    void cancelImport();
signals:
    void documentChanged();
    void documentReplaced();
    void busyChanged(bool busy);
    void importFailed(const QString& message);
    void importCancelled();
    void exportFinished(const QString& path);
    void exportFailed(const QString& message);
    void exportCancelled();
    void projectSaved(const QString& path);
    void projectSaveFailed(const QString& message);
    void projectSaveCancelled();
private:
    bool load(const QString& path, bool asLayer);
    core::History history_;
    core::LayerId nextLayerId_ = 2;
    core::EffectId nextEffectId_ = 1;
    bool importingLayer_ = false;
    std::shared_ptr<std::atomic_bool> cancellation_;
    QFutureWatcher<io::LoadResult> watcher_;
    QFutureWatcher<io::ExportResult> exportWatcher_;
    QFutureWatcher<io::ExportResult> saveWatcher_;
    QString projectPath_, loadingProjectPath_, savingPath_;
    QString exportPath_;
    bool busy_ = false;
};
}
