#ifndef SWITCHCONTROLWIDGET_H
#define SWITCHCONTROLWIDGET_H

#include <QWidget>
#include <QPushButton>
#include <QCheckBox>
#include <QGridLayout>
#include <QLabel>

class SwitchControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SwitchControlWidget(QWidget *parent = nullptr);

    void setSwitchState(int switchId, bool state);
    bool switchState(int switchId) const;

    // 模式控制
    bool isManualMode() const { return m_manualMode; }
    void setManualMode(bool manual);

signals:
    void switchToggled(int switchId, bool state);
    void modeChanged(bool manualMode);

private slots:
    void onSwitchButtonClicked();

private:
    void setupUI();
    void updateSwitchAppearance(int switchId);
    void updateSwitchesVisibility();

    struct SwitchInfo {
        QPushButton *button;
        QLabel *label;
        bool state;
        QString name;
    };

    QVector<SwitchInfo> m_switches;
    bool m_manualMode;
    QCheckBox *m_modeCheckBox;
};

#endif // SWITCHCONTROLWIDGET_H