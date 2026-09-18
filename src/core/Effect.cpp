#include <photoastra/core/Effect.h>
#include <cmath>
#include <stdexcept>
#include <unordered_set>
namespace photoastra::core {
void validateEffects(const EffectStack& effects)
{
    if (effects.size() > maxEffectsPerLayer) throw std::invalid_argument("Too many effects in layer");
    std::unordered_set<EffectId> ids;
    for (const auto& effect : effects) {
        if (effect.id == 0 || !ids.insert(effect.id).second || effect.type != EffectType::Exposure ||
            !std::isfinite(effect.exposureStops) || effect.exposureStops < -8 || effect.exposureStops > 8)
            throw std::invalid_argument("Invalid effect metadata");
    }
}
}
