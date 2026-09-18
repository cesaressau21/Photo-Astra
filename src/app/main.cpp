#include <photoastra/application/DocumentSession.h>
#include <photoastra/render/SkiaRenderer.h>
#include <photoastra/ui/MainWindow.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <QMessageBox>
#include <cstdlib>
#include <exception>

int main(int argc, char* argv[])
{
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGL);
    format.setVersion(3, 2);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setStencilBufferSize(8);
    format.setSamples(0);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("Photo Astra"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("cpu"), QStringLiteral("Usar Skia CPU sin OpenGL.")});
    parser.addPositionalArgument(QStringLiteral("imagen"), QStringLiteral("Documento .pastra o imagen PNG/JPEG opcional para abrir."), QStringLiteral("[imagen]"));
    parser.process(application);
    if (parser.isSet(QStringLiteral("cpu"))) qputenv("PHOTO_ASTRA_RENDERER", "cpu");

    try {
        photoastra::application::DocumentSession session;
        photoastra::ui::MainWindow window(session, std::make_unique<photoastra::render::SkiaRenderer>());
        window.show();
        if (!parser.positionalArguments().isEmpty()) session.openImage(parser.positionalArguments().first());
        return application.exec();
    } catch (const std::exception& error) {
        QMessageBox::critical(nullptr, QObject::tr("No se pudo iniciar Photo Astra"),
                              QString::fromUtf8(error.what()));
        return EXIT_FAILURE;
    }
}
