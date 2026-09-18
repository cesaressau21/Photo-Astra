#pragma once
#include <photoastra/core/Document.h>
#include <include/core/SkImage.h>
#include <unordered_map>
#include "EffectPipeline.h"

class SkCanvas;
namespace photoastra::render {
// Backend-private cache; document snapshots never contain graphics resources.
class SkiaCompositor final {
public:
    bool draw(SkCanvas& canvas, const core::Document& document);
    const std::string& error() const noexcept { return pipeline_.error(); }
private:
    struct CachedImage {
        std::shared_ptr<const core::RasterImage> source;
        sk_sp<SkImage> image;
    };
    std::unordered_map<const core::RasterImage*, CachedImage> cache_;
    struct CachedEffects { core::EffectStack stack; sk_sp<SkColorFilter> filter; };
    std::unordered_map<core::LayerId, CachedEffects> effects_;
    EffectPipeline pipeline_;
};
}
