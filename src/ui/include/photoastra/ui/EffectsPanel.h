#pragma once
#include <QWidget>
#include <photoastra/core/Document.h>
#include <functional>
class QListWidget;
class QDoubleSpinBox;
class QPushButton;
namespace photoastra::application { class DocumentSession; }
namespace photoastra::ui {
class EffectsPanel final : public QWidget {
public:
    EffectsPanel(application::DocumentSession& session, QWidget* parent = nullptr);
    void setLayer(qulonglong id);
private:
    void refresh();
    void refreshSelection();
    const core::RasterLayer* layer() const;
    core::EffectId selectedId() const;
    void modify(const std::function<void(core::EffectStack&)>& change);
    void move(int direction);
    application::DocumentSession& session_;
    core::LayerId layerId_ = 0;
    bool selectLast_ = false;
    QListWidget* list_;
    QDoubleSpinBox* exposure_;
    QPushButton *add_, *remove_, *up_, *down_;
};
}
