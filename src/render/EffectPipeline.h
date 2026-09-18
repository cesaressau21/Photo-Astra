#pragma once
#include <photoastra/core/Effect.h>
#include <include/core/SkColorFilter.h>
#include <include/effects/SkRuntimeEffect.h>
#include <string>

namespace photoastra::render {
class EffectPipeline final {
public:
    bool build(const core::EffectStack& stack, sk_sp<SkColorFilter>& output);
    const std::string& error() const noexcept { return error_; }
private:
    sk_sp<SkRuntimeEffect> exposure_;
    bool compileAttempted_ = false;
    std::string error_;
};
}
