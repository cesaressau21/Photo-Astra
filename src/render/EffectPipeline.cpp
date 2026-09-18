#include "EffectPipeline.h"
#include <cmath>

namespace photoastra::render {
bool EffectPipeline::build(const core::EffectStack& stack, sk_sp<SkColorFilter>& output)
{
    output.reset();
    for (const auto& effect : stack) {
        if (!effect.enabled || effect.exposureStops == 0) continue;
        if (!compileAttempted_) {
            compileAttempted_ = true;
            const auto result = SkRuntimeEffect::MakeForColorFilter(SkString(R"(
                uniform float gain;
                half4 main(half4 color) {
                    if (color.a <= 0) return half4(0);
                    float3 straight = color.rgb / color.a;
                    float3 linear = toLinearSrgb(straight);
                    float3 adjusted = fromLinearSrgb(clamp(linear * gain, 0.0, 1.0));
                    return half4(adjusted * color.a, color.a);
                }
            )"));
            exposure_ = result.effect;
            if (!exposure_) error_ = "Exposure SkSL: " + std::string(result.errorText.c_str());
        }
        if (!exposure_) return false;
        SkRuntimeColorFilterBuilder builder(exposure_);
        builder.uniform("gain") = std::exp2(effect.exposureStops);
        auto filter = builder.makeColorFilter();
        if (!filter) { error_ = "No se pudo crear el filtro Exposure."; return false; }
        output = output ? SkColorFilters::Compose(std::move(filter), std::move(output)) : std::move(filter);
        if (!output) { error_ = "No se pudo componer el stack de efectos."; return false; }
    }
    return true;
}
}
