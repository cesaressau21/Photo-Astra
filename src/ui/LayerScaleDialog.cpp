#include <photoastra/ui/MainWindow.h>
#include <photoastra/ui/CanvasWidget.h>
#include <photoastra/application/DocumentSession.h>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

namespace photoastra::ui {
void MainWindow::scaleLayer(qulonglong id)
{
    if (session_.busy()) return;
    const auto original = session_.document();
    const auto& layers = original->layers();
    const auto found = std::find_if(layers.begin(), layers.end(), [id](const auto& layer) { return layer.id == id; });
    if (found == layers.end() || !found->image) return;
    auto scale = found->scale;
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("layerScaleDialog"));
    dialog.setWindowTitle(tr("Escalar capa"));
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(tr("Anclaje: esquina superior izquierda.\nLos píxeles originales se conservan."), &dialog));
    auto* form = new QFormLayout;
    const auto field = [&](const char* name, double value) {
        auto* spin = new QDoubleSpinBox(&dialog);
        spin->setObjectName(QString::fromLatin1(name));
        spin->setDecimals(6);
        spin->setRange(core::minLayerScale * 100, core::maxLayerScale * 100);
        spin->setSuffix(tr(" %"));
        spin->setKeyboardTracking(false);
        spin->setValue(value * 100);
        return spin;
    };
    auto* x = field("layerScaleX", scale.x);
    auto* y = field("layerScaleY", scale.y);
    form->addRow(tr("Horizontal"), x);
    form->addRow(tr("Vertical"), y);
    layout->addLayout(form);
    auto* linked = new QCheckBox(tr("Misma escala en ambos ejes"), &dialog);
    linked->setObjectName(QStringLiteral("linkedScale"));
    linked->setChecked(scale.x == scale.y);
    layout->addWidget(linked);
    const auto preview = [&] {
        // Reuse core commands for validation; preview history never touches the session.
        core::History previewHistory(original, 1);
        previewHistory.execute(core::SetScale{id, scale});
        canvas_->setDocument(previewHistory.document(), false);
    };
    connect(x, &QDoubleSpinBox::valueChanged, &dialog, [&](double value) {
        scale.x = value / 100;
        if (linked->isChecked()) { const QSignalBlocker blocker(y); y->setValue(value); scale.y = scale.x; }
        preview();
    });
    connect(y, &QDoubleSpinBox::valueChanged, &dialog, [&](double value) {
        scale.y = value / 100;
        if (linked->isChecked()) { const QSignalBlocker blocker(x); x->setValue(value); scale.x = scale.y; }
        preview();
    });
    connect(linked, &QCheckBox::toggled, &dialog, [&](bool checked) {
        if (checked) { const QSignalBlocker blocker(y); y->setValue(x->value()); scale.y = scale.x; preview(); }
    });
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Reset, &dialog);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Reset), &QPushButton::clicked, &dialog, [&] {
        const QSignalBlocker blockX(x), blockY(y);
        x->setValue(100); y->setValue(100); scale = {}; preview();
    });
    const bool accepted = dialog.exec() == QDialog::Accepted;
    canvas_->setDocument(session_.document(), false);
    if (accepted && session_.document() == original) session_.edit(core::SetScale{id, scale});
}
}
