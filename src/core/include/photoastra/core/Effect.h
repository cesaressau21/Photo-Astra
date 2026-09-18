#pragma once
#include <cstdint>
#include <vector>

namespace photoastra::core {
using EffectId = std::uint64_t;
enum class EffectType { Exposure };
struct Effect {
    EffectId id;
    EffectType type = EffectType::Exposure;
    bool enabled = true;
    float exposureStops = 0;
    bool operator==(const Effect&) const = default;
};
using EffectStack = std::vector<Effect>;
inline constexpr std::size_t maxEffectsPerLayer = 16;
// Range in stops; IDs are unique within each layer's stack.
void validateEffects(const EffectStack& effects);
}
