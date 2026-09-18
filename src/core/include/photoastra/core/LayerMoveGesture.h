#pragma once
#include <photoastra/core/History.h>
#include <photoastra/core/Viewport.h>
#include <optional>

namespace photoastra::core {
// Preview snapshots share source pixels. Only the final command enters History.
class LayerMoveGesture final {
public:
    bool begin(std::shared_ptr<const Document> document, LayerId id, Point anchor);
    std::shared_ptr<const Document> update(Point point);
    std::optional<SetPosition> command() const;
    const std::shared_ptr<const Document>& original() const noexcept { return original_; }
private:
    std::shared_ptr<const Document> original_, preview_;
    LayerId id_ = 0;
    Point anchor_;
    LayerPosition initial_, position_;
};
}
