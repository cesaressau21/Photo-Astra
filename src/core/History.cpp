#include <photoastra/core/History.h>
#include <algorithm>
#include <stdexcept>
#include <type_traits>

namespace photoastra::core {
History::History(std::shared_ptr<const Document> initial, std::size_t capacity) : capacity_(capacity)
{
    if (capacity == 0) throw std::invalid_argument("History capacity must be positive");
    reset(std::move(initial));
}
void History::reset(std::shared_ptr<const Document> document)
{
    if (!document) throw std::invalid_argument("History requires a document");
    current_ = std::move(document);
    clean_ = current_;
    undo_.clear();
    redo_.clear();
}
bool History::execute(const EditCommand& command)
{
    auto layers = current_->layers();
    const bool changed = std::visit([&](const auto& edit) {
        using T = std::decay_t<decltype(edit)>;
        if constexpr (std::is_same_v<T, AddLayer>) {
            layers.push_back(edit.layer);
        } else {
            const auto found = std::find_if(layers.begin(), layers.end(), [&](const auto& layer) { return layer.id == edit.id; });
            if (found == layers.end()) return false;
            if constexpr (std::is_same_v<T, RemoveLayer>) layers.erase(found);
            else if constexpr (std::is_same_v<T, MoveLayer>) {
                if (edit.index >= layers.size() || edit.index == static_cast<std::size_t>(found - layers.begin())) return false;
                auto layer = *found;
                layers.erase(found);
                layers.insert(layers.begin() + static_cast<std::ptrdiff_t>(edit.index), std::move(layer));
            } else if constexpr (std::is_same_v<T, SetOpacity>) {
                if (found->opacity == edit.opacity) return false;
                found->opacity = edit.opacity;
            } else if constexpr (std::is_same_v<T, SetVisibility>) {
                if (found->visible == edit.visible) return false;
                found->visible = edit.visible;
            } else if constexpr (std::is_same_v<T, SetEffects>) {
                if (found->effects == edit.effects) return false;
                found->effects = edit.effects;
            } else if constexpr (std::is_same_v<T, SetPosition>) {
                if (found->position == edit.position) return false;
                found->position = edit.position;
            } else if constexpr (std::is_same_v<T, SetScale>) {
                if (found->scale == edit.scale) return false;
                found->scale = edit.scale;
            } else if constexpr (std::is_same_v<T, SetBlendMode>) {
                if (found->blendMode == edit.blendMode) return false;
                found->blendMode = edit.blendMode;
            }
        }
        return true;
    }, command);
    if (!changed) return false;
    auto next = std::make_shared<const Document>(current_->withLayers(std::move(layers)));
    undo_.push_back(current_);
    if (undo_.size() > capacity_) undo_.erase(undo_.begin());
    current_ = std::move(next);
    redo_.clear();
    return true;
}
bool History::undo()
{
    if (!canUndo()) return false;
    redo_.push_back(current_);
    current_ = undo_.back();
    undo_.pop_back();
    return true;
}
bool History::redo()
{
    if (!canRedo()) return false;
    undo_.push_back(current_);
    current_ = redo_.back();
    redo_.pop_back();
    return true;
}
}
