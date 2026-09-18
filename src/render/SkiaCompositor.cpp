#include "SkiaCompositor.h"
#include <photoastra/core/RasterImage.h>
#include <include/core/SkCanvas.h>
#include <include/core/SkColorSpace.h>
#include <include/core/SkData.h>
#include <include/core/SkPaint.h>
#include <unordered_set>

namespace photoastra::render {
bool SkiaCompositor::draw(SkCanvas& canvas, const core::Document& document)
{
    std::unordered_set<const core::RasterImage*> retained;
    for (const auto& layer : document.layers()) if (layer.image) retained.insert(layer.image.get());
    std::erase_if(cache_, [&](const auto& entry) { return !retained.contains(entry.first); });
    std::unordered_set<core::LayerId> layerIds;
    for (const auto& layer : document.layers()) layerIds.insert(layer.id);
    std::erase_if(effects_, [&](const auto& entry) { return !layerIds.contains(entry.first); });
    const auto extent = document.extent();
    canvas.save();
    canvas.clipRect(SkRect::MakeWH(static_cast<float>(extent.width), static_cast<float>(extent.height)));
    for (const auto& layer : document.layers()) {
        if (!layer.visible || layer.opacity == 0 || !layer.image) continue;
        auto& cached = cache_[layer.image.get()];
        if (!cached.image) {
            cached.source = layer.image;
            const auto& source = cached.source;
            auto* owner = new std::shared_ptr<const core::RasterImage>(source);
            auto data = SkData::MakeWithProc(source->pixels().data(), source->pixels().size(),
                [](const void*, void* context) { delete static_cast<std::shared_ptr<const core::RasterImage>*>(context); }, owner);
            const auto size = source->extent();
            cached.image = SkImages::RasterFromData(SkImageInfo::Make(static_cast<int>(size.width),
                static_cast<int>(size.height), kRGBA_8888_SkColorType, kPremul_SkAlphaType,
                SkColorSpace::MakeSRGB()), std::move(data), source->rowBytes());
        }
        if (!cached.image) { canvas.restore(); return false; }
        SkPaint paint;
        auto& effects = effects_[layer.id];
        if (effects.stack != layer.effects) {
            sk_sp<SkColorFilter> filter;
            if (!pipeline_.build(layer.effects, filter)) { canvas.restore(); return false; }
            effects = {layer.effects, std::move(filter)};
        }
        paint.setColorFilter(effects.filter);
        paint.setAlphaf(layer.opacity);
        switch (layer.blendMode) {
        case core::BlendMode::Normal: paint.setBlendMode(SkBlendMode::kSrcOver); break;
        case core::BlendMode::Multiply: paint.setBlendMode(SkBlendMode::kMultiply); break;
        case core::BlendMode::Screen: paint.setBlendMode(SkBlendMode::kScreen); break;
        }
        canvas.save();
        canvas.translate(static_cast<float>(layer.position.x), static_cast<float>(layer.position.y));
        canvas.scale(static_cast<float>(layer.scale.x), static_cast<float>(layer.scale.y));
        canvas.drawImage(cached.image, 0, 0,
            SkSamplingOptions(SkFilterMode::kLinear), &paint);
        canvas.restore();
    }
    canvas.restore();
    return true;
}
}
