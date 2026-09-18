#pragma once
#include <QWidget>
#include <photoastra/core/Document.h>
class QListWidget;
class QDoubleSpinBox;
class QPushButton;
class QSpinBox;
class QComboBox;
namespace photoastra::application { class DocumentSession; }
namespace photoastra::ui {
class LayersPanel final : public QWidget {
    Q_OBJECT
public:
    explicit LayersPanel(application::DocumentSession& session, QWidget* parent = nullptr);
    core::LayerId selectedId() const;
signals:
    void selectedLayerChanged(qulonglong id);
    void scaleLayerRequested(qulonglong id);
private:
    void refresh();
    void refreshSelection();
    void moveSelected(int direction);
    application::DocumentSession& session_;
    QListWidget* list_;
    QDoubleSpinBox* opacity_;
    QComboBox* blendMode_;
    QSpinBox *positionX_, *positionY_;
    QPushButton *remove_, *up_, *down_, *scale_;
};
}
