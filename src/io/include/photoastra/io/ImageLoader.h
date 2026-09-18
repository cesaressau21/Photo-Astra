#pragma once
#include <photoastra/core/Document.h>
#include <QString>
#include <atomic>
#include <memory>

namespace photoastra::io {
struct LoadResult {
    std::shared_ptr<const core::Document> document;
    QString error;
    bool cancelled = false;
};
// May run on a worker thread. No widgets or application state are accessed.
[[nodiscard]] LoadResult loadImage(const QString& path, const std::atomic_bool& cancelled);
}
