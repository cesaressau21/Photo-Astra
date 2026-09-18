#include <photoastra/ui/EffectsPanel.h>
#include <photoastra/application/DocumentSession.h>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <algorithm>

namespace photoastra::ui {
EffectsPanel::EffectsPanel(application::DocumentSession& session, QWidget* parent) : QWidget(parent), session_(session)
{
    auto* box = new QVBoxLayout(this);
    box->addWidget(new QLabel(tr("Efectos de la capa · Se aplican de arriba abajo"), this));
    add_ = new QPushButton(tr("Añadir exposición"), this);
    add_->setObjectName(QStringLiteral("addExposureButton"));
    box->addWidget(add_);
    list_ = new QListWidget(this);
    list_->setObjectName(QStringLiteral("effectList"));
    box->addWidget(list_);
    auto* buttons = new QHBoxLayout;
    const auto button = [&](const QString& label, const char* name) {
        auto* result = new QPushButton(label, this);
        result->setObjectName(QString::fromLatin1(name)); buttons->addWidget(result); return result;
    };
    up_ = button(tr("Subir"), "raiseEffectButton");
    down_ = button(tr("Bajar"), "lowerEffectButton");
    remove_ = button(tr("Eliminar"), "removeEffectButton");
    box->addLayout(buttons);
    exposure_ = new QDoubleSpinBox(this);
    exposure_->setObjectName(QStringLiteral("exposureStops"));
    exposure_->setRange(-8, 8); exposure_->setDecimals(2); exposure_->setSingleStep(0.25);
    exposure_->setSuffix(QStringLiteral(" EV")); exposure_->setKeyboardTracking(false);
    auto* form = new QFormLayout;
    form->addRow(tr("Exposición"), exposure_); box->addLayout(form);
    connect(add_, &QPushButton::clicked, this, [this] {
        if (!layer() || layer()->effects.size() >= core::maxEffectsPerLayer) return;
        selectLast_ = true;
        session_.addExposure(layerId_);
    });
    connect(remove_, &QPushButton::clicked, this, [this] {
        const auto id = selectedId(); modify([id](auto& stack) { std::erase_if(stack, [id](const auto& effect) { return effect.id == id; }); });
    });
    connect(up_, &QPushButton::clicked, this, [this] { move(-1); });
    connect(down_, &QPushButton::clicked, this, [this] { move(1); });
    connect(exposure_, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        const auto id = selectedId(); modify([=](auto& stack) {
            for (auto& effect : stack) if (effect.id == id) effect.exposureStops = static_cast<float>(value);
        });
    });
    connect(list_, &QListWidget::itemChanged, this, [this](QListWidgetItem* item) {
        const auto id = item->data(Qt::UserRole).toULongLong(); const bool enabled = item->checkState() == Qt::Checked;
        modify([=](auto& stack) { for (auto& effect : stack) if (effect.id == id) effect.enabled = enabled; });
    });
    connect(list_, &QListWidget::currentRowChanged, this, [this] { refreshSelection(); });
    connect(&session_, &application::DocumentSession::documentChanged, this, &EffectsPanel::refresh, Qt::QueuedConnection);
    connect(&session_, &application::DocumentSession::busyChanged, this, [this](bool busy) { setEnabled(!busy); });
    refresh();
}
const core::RasterLayer* EffectsPanel::layer() const
{
    for (const auto& current : session_.document()->layers()) if (current.id == layerId_) return &current;
    return nullptr;
}
core::EffectId EffectsPanel::selectedId() const
{
    return list_->currentItem() ? list_->currentItem()->data(Qt::UserRole).toULongLong() : 0;
}
void EffectsPanel::setLayer(qulonglong id)
{
    if (layerId_ == id) return;
    layerId_ = id;
    const QSignalBlocker blocker(list_);
    list_->clear(); refresh();
}
void EffectsPanel::refresh()
{
    const auto selected = selectLast_ ? 0 : selectedId();
    selectLast_ = false;
    const QSignalBlocker blocker(list_);
    list_->clear();
    const auto* current = layer();
    add_->setEnabled(current && current->effects.size() < core::maxEffectsPerLayer);
    if (current) for (const auto& effect : current->effects) {
        auto* item = new QListWidgetItem(tr("Exposición: %1 EV").arg(effect.exposureStops, 0, 'f', 2), list_);
        item->setData(Qt::UserRole, QVariant::fromValue<qulonglong>(effect.id));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(effect.enabled ? Qt::Checked : Qt::Unchecked);
        if (selected == effect.id) list_->setCurrentItem(item);
    }
    if (!list_->currentItem() && list_->count()) list_->setCurrentRow(list_->count() - 1);
    refreshSelection();
}
void EffectsPanel::refreshSelection()
{
    const auto* current = layer();
    const core::Effect* selected = nullptr;
    if (current) for (const auto& effect : current->effects) if (effect.id == selectedId()) selected = &effect;
    remove_->setEnabled(selected != nullptr); exposure_->setEnabled(selected != nullptr);
    up_->setEnabled(selected && list_->currentRow() > 0);
    down_->setEnabled(selected && list_->currentRow() + 1 < list_->count());
    const QSignalBlocker blocker(exposure_);
    exposure_->setValue(selected ? selected->exposureStops : 0);
}
void EffectsPanel::modify(const std::function<void(core::EffectStack&)>& change)
{
    const auto* current = layer();
    if (!current || session_.busy()) return;
    auto stack = current->effects; change(stack);
    session_.edit(core::SetEffects{layerId_, std::move(stack)});
}
void EffectsPanel::move(int direction)
{
    const auto id = selectedId();
    modify([=](auto& stack) {
        const auto found = std::find_if(stack.begin(), stack.end(), [=](const auto& effect) { return effect.id == id; });
        if (found == stack.end()) return;
        const auto index = found - stack.begin() + direction;
        if (index >= 0 && static_cast<std::size_t>(index) < stack.size()) std::iter_swap(found, stack.begin() + index);
    });
}
}
