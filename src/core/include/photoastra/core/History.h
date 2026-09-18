#pragma once
#include <photoastra/core/Document.h>
#include <variant>

namespace photoastra::core {
struct AddLayer { RasterLayer layer; };
struct RemoveLayer { LayerId id; };
struct MoveLayer { LayerId id; std::size_t index; };
struct SetOpacity { LayerId id; float opacity; };
struct SetVisibility { LayerId id; bool visible; };
struct SetEffects { LayerId id; EffectStack effects; };
struct SetPosition { LayerId id; LayerPosition position; };
struct SetScale { LayerId id; LayerScale scale; };
struct SetBlendMode { LayerId id; BlendMode blendMode; };
using EditCommand = std::variant<AddLayer, RemoveLayer, MoveLayer, SetOpacity, SetVisibility, SetEffects, SetPosition, SetScale, SetBlendMode>;

// Snapshots copy metadata only. The capacity bounds retained history entries, not pixel bytes.
class History final {
public:
    explicit History(std::shared_ptr<const Document> initial, std::size_t capacity = 100);
    const std::shared_ptr<const Document>& document() const noexcept { return current_; }
    bool execute(const EditCommand& command);
    bool undo();
    bool redo();
    bool canUndo() const noexcept { return !undo_.empty(); }
    bool canRedo() const noexcept { return !redo_.empty(); }
    bool dirty() const noexcept { return current_ != clean_.lock(); }
    void markSaved() noexcept { clean_ = current_; }
    void reset(std::shared_ptr<const Document> document);
private:
    std::shared_ptr<const Document> current_;
    std::weak_ptr<const Document> clean_;
    std::vector<std::shared_ptr<const Document>> undo_, redo_;
    std::size_t capacity_;
};
}
