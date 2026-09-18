#pragma once
#include <photoastra/core/Document.h>
#include <photoastra/io/ImageWriter.h>
namespace photoastra::render { class Renderer; }
namespace photoastra::application {
// Worker-owned renderer. Does not use the canvas or its OpenGL context.
[[nodiscard]] io::ExportResult exportDocument(const core::Document& document, render::Renderer& renderer,
    const QString& path, io::ExportFormat format, const std::atomic_bool& cancelled);
}
