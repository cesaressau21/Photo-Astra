#include <photoastra/core/Document.h>
#include <photoastra/render/Renderer.h>
#include <photoastra/core/RasterImage.h>
#include <photoastra/core/Viewport.h>
#include <photoastra/core/History.h>
#include <photoastra/core/LayerMoveGesture.h>
#include <cmath>

#include <cstdlib>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void check(int& failures, bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

template<typename F>
void rejects(int& failures, F&& operation)
{
    try {
        operation();
        check(failures, false, "invalid document was accepted");
    } catch (const std::invalid_argument&) {
    }
}
}

int main()
{
    int failures = 0;
    using photoastra::core::Document;
    const Document document({1920, 1080}, "Test");
    check(failures, document.extent() == photoastra::core::ImageExtent{1920, 1080}, "dimensions");
    check(failures, document.title() == "Test", "title");
    rejects(failures, [] { const Document invalid({0, 10}, "Invalid"); });
    rejects(failures, [] { const Document invalid({10, 0}, "Invalid"); });
    rejects(failures, [] { const Document invalid({10, 10}, ""); });
    rejects(failures, [] { const photoastra::core::RasterImage invalid({2, 2}, {1, 2, 3, 4}); });
    auto image = std::make_shared<const photoastra::core::RasterImage>(photoastra::core::ImageExtent{1, 1},
        std::vector<std::uint8_t>{255, 0, 0, 255});
    const Document withImage({1, 1}, "Source", image);
    const Document snapshot = withImage;
    check(failures, snapshot.layers().front().image.get() == image.get(), "document snapshots share source pixels");
    rejects(failures, [&] { const Document invalid({2, 2}, "Mismatch", image); });
    using namespace photoastra::core;
    History history(std::make_shared<const Document>(withImage), 3);
    const auto initial = history.document();
    check(failures, !history.dirty() && !history.undo() && !history.redo(), "initial history");
    check(failures, history.execute(AddLayer{{2, "Top", image}}), "add layer");
    check(failures, history.document()->layers()[1].image == image, "history shares pixel storage");
    check(failures, history.execute(SetOpacity{2, 0.5F}), "set opacity");
    const auto valid = history.document();
    rejects(failures, [&] { history.execute(SetOpacity{2, std::numeric_limits<float>::quiet_NaN()}); });
    rejects(failures, [&] { history.execute(AddLayer{{2, "Duplicate", image}}); });
    check(failures, history.document() == valid, "invalid commands are atomic");
    check(failures, !history.execute(SetOpacity{2, 0.5F}) && !history.execute(RemoveLayer{999}), "no-op commands ignored");
    check(failures, history.undo() && history.document()->layers()[1].opacity == 1, "undo opacity");
    check(failures, history.undo() && history.document() == initial && !history.dirty(), "undo returns clean snapshot");
    check(failures, history.redo() && history.document()->layers().size() == 2, "redo add");
    check(failures, history.execute(MoveLayer{2, 0}) && !history.canRedo(), "branch invalidates redo");
    check(failures, history.document()->layers()[0].id == 2, "bottom-to-top ordering");
    check(failures, history.execute(SetVisibility{2, false}), "hide layer");
    check(failures, history.execute(RemoveLayer{2}), "remove layer");
    check(failures, history.undo() && !history.document()->layers()[0].visible, "undo restores deleted metadata");
    check(failures, history.undo() && history.undo() && !history.undo(), "history capacity");
    history.reset(initial);
    LayerMoveGesture gesture;
    check(failures, history.execute(SetBlendMode{1, BlendMode::Multiply}), "blend command");
    const auto blended = history.document();
    check(failures, blended->layers()[0].image == image, "blend shares source pixels");
    rejects(failures, [&] { history.execute(SetBlendMode{1, static_cast<BlendMode>(99)}); });
    check(failures, history.document() == blended, "invalid blend is atomic");
    check(failures, !history.execute(SetBlendMode{1, BlendMode::Multiply}), "same blend is no-op");
    check(failures, history.undo() && history.document() == initial, "undo blend");
    check(failures, history.redo() && history.document() == blended, "redo blend");
    history.reset(initial);
    check(failures, history.execute(SetScale{1, {2, 0.5}}), "scale command");
    const auto scaled = history.document();
    check(failures, scaled->layers()[0].image == image, "scale shares source pixels");
    for (const double invalid : {0.0, -1.0, 65.0, 0.001, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        rejects(failures, [&] { history.execute(SetScale{1, {invalid, 1}}); });
        rejects(failures, [&] { history.execute(SetScale{1, {1, invalid}}); });
    }
    check(failures, history.document() == scaled, "invalid scale is atomic");
    check(failures, !history.execute(SetScale{1, {2, 0.5}}), "same scale is no-op");
    check(failures, history.undo() && history.document() == initial, "undo scale");
    check(failures, history.redo() && history.document() == scaled, "redo scale");
    auto wideLayers = scaled->layers();
    const auto wide = std::make_shared<const Document>(Document({4, 4}, "Scaled hit test").withLayers(wideLayers));
    check(failures, gesture.begin(wide, 1, {1.5, 0.25}), "scaled raster hit test");
    check(failures, !gesture.begin(wide, 1, {1.5, 0.75}), "scaled raster rejects outside");
    history.reset(initial);
    check(failures, !gesture.begin(initial, 999, {0, 0}), "gesture requires selected layer");
    check(failures, !gesture.begin(initial, 1, {1, 0}), "gesture rejects outside raster");
    check(failures, gesture.begin(initial, 1, {0.5, 0.5}), "gesture begins inside raster");
    check(failures, !gesture.command(), "click alone is no-op");
    const auto preview = gesture.update({5.5, -2.5});
    check(failures, preview->layers()[0].position == LayerPosition{5, -3}, "gesture uses image coordinates");
    check(failures, initial->layers()[0].position == LayerPosition{}, "preview does not mutate original");
    check(failures, preview->layers()[0].image == initial->layers()[0].image, "preview shares pixels");
    check(failures, gesture.update({std::numeric_limits<double>::quiet_NaN(), 0}) == preview, "nonfinite pointer ignored");
    check(failures, gesture.command()->position == LayerPosition{5, -3}, "gesture returns one final command");
    gesture.update({0.5, 0.5});
    check(failures, !gesture.command(), "drag back to origin is no-op");
    check(failures, history.execute(SetEffects{1, {{10, EffectType::Exposure, true, 1}}}), "add effect command");
    const auto effectSnapshot = history.document();
    rejects(failures, [&] { history.execute(SetEffects{1, {{10, EffectType::Exposure, true, 9}}}); });
    rejects(failures, [&] { history.execute(SetEffects{1, {{10, EffectType::Exposure, true, std::numeric_limits<float>::quiet_NaN()}}}); });
    rejects(failures, [&] { history.execute(SetEffects{1, {{10}, {10}}}); });
    rejects(failures, [&] { history.execute(SetEffects{1, {{0}}}); });
    rejects(failures, [&] { history.execute(SetEffects{1, {{11, static_cast<EffectType>(99)}}}); });
    rejects(failures, [&] { EffectStack large; for (EffectId id = 1; id <= 17; ++id) large.push_back({id}); history.execute(SetEffects{1, large}); });
    check(failures, history.document() == effectSnapshot, "effect validation is atomic");
    check(failures, history.undo() && history.document() == initial, "undo effect");
    check(failures, history.redo() && history.document() == effectSnapshot, "redo effect");
    check(failures, history.document()->layers()[0].image == image, "effects share original pixels");
    history.reset(initial);
    check(failures, !history.dirty() && !history.canUndo() && !history.canRedo(), "reset clears history");
    photoastra::core::Viewport view;
    view.pan({20, -30});
    const auto before = view.viewToImage({120, 90});
    view.zoomAt(2.5, {120, 90});
    const auto after = view.viewToImage({120, 90});
    check(failures, std::abs(before.x - after.x) < 1e-9 && std::abs(before.y - after.y) < 1e-9,
        "zoom preserves cursor anchor");
    const auto roundTrip = view.viewToImage(view.imageToView({37, 51}));
    check(failures, std::abs(roundTrip.x - 37) < 1e-9 && std::abs(roundTrip.y - 51) < 1e-9, "coordinate round trip");
    view.zoomAt(1e9, {0, 0});
    check(failures, view.zoom() == photoastra::core::Viewport::maxZoom, "maximum zoom");
    view.zoomAt(1e-20, {0, 0});
    check(failures, view.zoom() == photoastra::core::Viewport::minZoom, "minimum zoom");
    view.zoomAt(std::numeric_limits<double>::quiet_NaN(), {0, 0});
    check(failures, std::isfinite(view.zoom()), "nonfinite zoom rejected");
    view.fit({800, 600}, 448, 348);
    check(failures, view.zoom() == 0.5 && view.offset().x == 24 && view.offset().y == 24, "fit and centering");
    const auto maximum = std::numeric_limits<std::uint32_t>::max();
    const Document large({maximum, maximum}, "Metadata without pixel allocation");
    check(failures, large.extent().width == maximum, "large metadata without overflow");
    const photoastra::render::UnavailableRenderer backend;
    const photoastra::render::Renderer& renderer = backend;
    check(failures, !renderer.info().available && !renderer.info().gpuAccelerated,
          "placeholder must never claim rendering or GPU support");
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
