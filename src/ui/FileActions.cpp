#include <photoastra/ui/MainWindow.h>
#include <photoastra/application/DocumentSession.h>
#include <photoastra/core/RasterImage.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>

namespace photoastra::ui {
void MainWindow::createDocument()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Nuevo documento"));
    dialog.setObjectName(QStringLiteral("newDocumentDialog"));
    auto* form = new QFormLayout(&dialog);
    auto* title = new QLineEdit(tr("Sin título"), &dialog);
    title->setObjectName(QStringLiteral("documentName"));
    auto* width = new QSpinBox(&dialog);
    auto* height = new QSpinBox(&dialog);
    width->setObjectName(QStringLiteral("documentWidth"));
    height->setObjectName(QStringLiteral("documentHeight"));
    for (auto* size : {width, height}) { size->setRange(1, 32768); size->setSuffix(tr(" px")); }
    width->setValue(1920);
    height->setValue(1080);
    form->addRow(tr("Nombre"), title);
    form->addRow(tr("Ancho"), width);
    form->addRow(tr("Alto"), height);
    auto* note = new QLabel(tr("Fondo transparente · sRGB\nMáximo: 33.554.432 píxeles."), &dialog);
    form->addRow(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form->addRow(buttons);
    const auto validate = [=] {
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!title->text().trimmed().isEmpty() &&
            static_cast<std::uint64_t>(width->value()) * height->value() <= core::RasterImage::maxPixels);
    };
    connect(width, &QSpinBox::valueChanged, &dialog, validate);
    connect(height, &QSpinBox::valueChanged, &dialog, validate);
    connect(title, &QLineEdit::textChanged, &dialog, validate);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() == QDialog::Accepted) {
        const core::ImageExtent extent{static_cast<std::uint32_t>(width->value()), static_cast<std::uint32_t>(height->value())};
        const auto name = title->text();
        requestReplace([this, extent, name] { session_.createDocument(extent, name); });
    }
}
bool MainWindow::saveDocument(bool saveAs)
{
    if (session_.busy()) return false;
    auto path = session_.projectPath();
    if (saveAs || path.isEmpty()) {
        QFileDialog dialog(this, tr("Guardar documento editable"));
        dialog.setObjectName(QStringLiteral("saveProjectDialog"));
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setFileMode(QFileDialog::AnyFile);
        dialog.setNameFilter(tr("Photo Astra (*.pastra)"));
        dialog.setDefaultSuffix(QStringLiteral("pastra"));
        if (!path.isEmpty()) dialog.selectFile(path);
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return false;
        path = dialog.selectedFiles().first();
        if (QFileInfo(path).suffix().compare(QStringLiteral("pastra"), Qt::CaseInsensitive) != 0) {
            QMessageBox::warning(this, tr("Extensión incompatible"), tr("Usa la extensión .pastra para guardar el documento editable."));
            return false;
        }
    }
    return session_.saveDocument(path);
}
void MainWindow::exportDocument()
{
    QFileDialog dialog(this, tr("Exportar composición"));
    dialog.setObjectName(QStringLiteral("exportDialog"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    const auto pngFilter = tr("PNG con transparencia (*.png)");
    const auto jpegFilter = tr("JPEG con fondo blanco (*.jpg *.jpeg)");
    dialog.setNameFilters({pngFilter, jpegFilter});
    dialog.setDefaultSuffix(QStringLiteral("png"));
    connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog, pngFilter](const QString& filter) {
        dialog.setDefaultSuffix(filter == pngFilter ? QStringLiteral("png") : QStringLiteral("jpg"));
    });
    if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty()) return;
    const auto path = dialog.selectedFiles().first();
    const auto suffix = QFileInfo(path).suffix().toLower();
    const bool png = dialog.selectedNameFilter() == pngFilter;
    if ((png && suffix != QStringLiteral("png")) ||
        (!png && suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg"))) {
        QMessageBox::warning(this, tr("Extensión incompatible"), tr("La extensión del archivo debe coincidir con el formato elegido."));
        return;
    }
    session_.exportImage(path, png ? io::ExportFormat::Png : io::ExportFormat::Jpeg);
}
}
