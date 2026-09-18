#include <photoastra/ui/LayersPanel.h>
#include <photoastra/application/DocumentSession.h>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>
#include <QSpinBox>
#include <QComboBox>

namespace photoastra::ui {
LayersPanel::LayersPanel(application::DocumentSession& session, QWidget* parent)
    : QWidget(parent), session_(session)
{
    auto* box = new QVBoxLayout(this);
    auto* buttons = new QHBoxLayout;
    auto button = [&](const QString& text, const char* name) {
        auto* result = new QPushButton(text, this);
        result->setObjectName(QString::fromLatin1(name));
        buttons->addWidget(result);
        return result;
    };
    auto* add = button(tr("+ Vacía"), "addLayerButton");
    auto* import = button(tr("Importar…"), "importLayerButton");
    remove_ = button(tr("Eliminar"), "removeLayerButton");
    box->addLayout(buttons);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("layerList"));
    list_->setAccessibleName(tr("Capas, de arriba hacia abajo"));
    box->addWidget(list_);
    buttons = new QHBoxLayout;
    up_ = button(tr("Subir"), "raiseLayerButton");
    down_ = button(tr("Bajar"), "lowerLayerButton");
    box->addLayout(buttons);
    opacity_ = new QDoubleSpinBox(this);
    opacity_->setObjectName(QStringLiteral("layerOpacity"));
    opacity_->setRange(0, 100);
    opacity_->setDecimals(1);
    opacity_->setSuffix(tr(" %"));
    opacity_->setKeyboardTracking(false);
    auto* form = new QFormLayout;
    blendMode_ = new QComboBox(this);
    blendMode_->setObjectName(QStringLiteral("layerBlendMode"));
    blendMode_->addItem(tr("Normal"), static_cast<int>(core::BlendMode::Normal));
    blendMode_->addItem(tr("Multiplicar"), static_cast<int>(core::BlendMode::Multiply));
    blendMode_->addItem(tr("Trama"), static_cast<int>(core::BlendMode::Screen));
    form->addRow(tr("Fusión"), blendMode_);
    connect(blendMode_, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (index >= 0) session_.edit(core::SetBlendMode{selectedId(), static_cast<core::BlendMode>(blendMode_->itemData(index).toInt())});
    });
    form->addRow(tr("Opacidad"), opacity_);
    positionX_ = new QSpinBox(this); positionY_ = new QSpinBox(this);
    positionX_->setObjectName(QStringLiteral("layerPositionX"));
    positionY_->setObjectName(QStringLiteral("layerPositionY"));
    for (auto* field : {positionX_, positionY_}) {
        field->setRange(-core::maxLayerOffset, core::maxLayerOffset);
        field->setSuffix(tr(" px")); field->setKeyboardTracking(false);
    }
    form->addRow(tr("Posición X"), positionX_);
    form->addRow(tr("Posición Y"), positionY_);
    const auto move = [this] { session_.edit(core::SetPosition{selectedId(), {positionX_->value(), positionY_->value()}}); };
    connect(positionX_, &QSpinBox::valueChanged, this, move);
    connect(positionY_, &QSpinBox::valueChanged, this, move);
    box->addLayout(form);
    scale_ = new QPushButton(tr("Escalar…"), this);
    scale_->setObjectName(QStringLiteral("scaleLayerButton"));
    box->addWidget(scale_);
    connect(scale_, &QPushButton::clicked, this, [this] { emit scaleLayerRequested(selectedId()); });
    connect(add, &QPushButton::clicked, &session_, &application::DocumentSession::addTransparentLayer);
    connect(import, &QPushButton::clicked, this, [this] {
        const auto path = QFileDialog::getOpenFileName(this, tr("Importar como capa"), {}, tr("Imágenes PNG/JPEG (*.png *.jpg *.jpeg)"));
        if (!path.isEmpty()) session_.importLayer(path);
    });
    connect(remove_, &QPushButton::clicked, this, [this] { session_.edit(core::RemoveLayer{selectedId()}); });
    connect(up_, &QPushButton::clicked, this, [this] { moveSelected(1); });
    connect(down_, &QPushButton::clicked, this, [this] { moveSelected(-1); });
    connect(opacity_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        session_.edit(core::SetOpacity{selectedId(), static_cast<float>(value / 100.0)});
    });
    connect(list_, &QListWidget::currentRowChanged, this, [this] { refreshSelection(); });
    connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        session_.edit(core::SetVisibility{item->data(Qt::UserRole).toULongLong(), item->checkState() == Qt::Checked});
    });
    // Rebuild after itemChanged has returned, so its item remains alive during signal dispatch.
    connect(&session_, &application::DocumentSession::documentChanged, this, &LayersPanel::refresh, Qt::QueuedConnection);
    connect(&session_, &application::DocumentSession::busyChanged, this, [this](bool busy) { setEnabled(!busy); });
    refresh();
}
core::LayerId LayersPanel::selectedId() const
{
    const auto* item = list_->currentItem();
    return item ? item->data(Qt::UserRole).toULongLong() : 0;
}
void LayersPanel::refresh()
{
    const auto selected = selectedId();
    const QSignalBlocker blocker(list_);
    list_->clear();
    const auto& layers = session_.document()->layers();
    for (auto layer = layers.rbegin(); layer != layers.rend(); ++layer) {
        auto* item = new QListWidgetItem(QString::fromStdString(layer->name), list_);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(layer->id));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer->visible ? Qt::Checked : Qt::Unchecked);
        if (layer->id == selected) list_->setCurrentItem(item);
    }
    if (!list_->currentItem() && list_->count() > 0) list_->setCurrentRow(0);
    refreshSelection();
}
void LayersPanel::refreshSelection()
{
    const auto& layers = session_.document()->layers();
    const auto found = std::find_if(layers.begin(), layers.end(), [&](const auto& layer) { return layer.id == selectedId(); });
    const bool valid = found != layers.end();
    remove_->setEnabled(valid);
    scale_->setEnabled(valid && bool(found->image));
    up_->setEnabled(valid && list_->currentRow() > 0);
    down_->setEnabled(valid && list_->currentRow() + 1 < list_->count());
    opacity_->setEnabled(valid);
    blendMode_->setEnabled(valid);
    const QSignalBlocker blockBlend(blendMode_);
    blendMode_->setCurrentIndex(blendMode_->findData(static_cast<int>(valid ? found->blendMode : core::BlendMode::Normal)));
    const QSignalBlocker blocker(opacity_);
    opacity_->setValue(valid ? found->opacity * 100.0 : 100.0);
    const QSignalBlocker blockX(positionX_), blockY(positionY_);
    positionX_->setEnabled(valid); positionY_->setEnabled(valid);
    positionX_->setValue(valid ? found->position.x : 0);
    positionY_->setValue(valid ? found->position.y : 0);
    emit selectedLayerChanged(selectedId());
}
void LayersPanel::moveSelected(int direction)
{
    const auto& layers = session_.document()->layers();
    const auto found = std::find_if(layers.begin(), layers.end(), [&](const auto& layer) { return layer.id == selectedId(); });
    if (found == layers.end()) return;
    const auto index = found - layers.begin() + direction;
    if (index >= 0 && static_cast<std::size_t>(index) < layers.size())
        session_.edit(core::MoveLayer{found->id, static_cast<std::size_t>(index)});
}
}
