#include "switchcontrolwidget.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QStyle>

SwitchControlWidget::SwitchControlWidget(QWidget *parent)
    : QWidget(parent)
    , m_manualMode(false)
    , m_modeCheckBox(nullptr)
{
    setAttribute(Qt::WA_StyledBackground, true);
    // 初始化列表：顺序即 switchId 1~6
    m_switches = {
        {nullptr, nullptr, false, QStringLiteral("制冷")},
        {nullptr, nullptr, false, QStringLiteral("加热")},
        {nullptr, nullptr, false, QStringLiteral("加湿")},
        {nullptr, nullptr, false, QStringLiteral("除湿")},
        {nullptr, nullptr, false, QStringLiteral("补光")},
        {nullptr, nullptr, false, QStringLiteral("报警")}
    };

    setupUI();
}

void SwitchControlWidget::setupUI()
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 模式切换
    m_modeCheckBox = new QCheckBox(tr("手动模式"), this);
    m_modeCheckBox->setChecked(m_manualMode);
    m_modeCheckBox->setProperty("class", "modeCheckBox");
    connect(m_modeCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        setManualMode(checked);
    });
    mainLayout->addWidget(m_modeCheckBox, 0, Qt::AlignCenter);

    mainLayout->addSpacing(4);

    // 开关网格布局
    QGridLayout *gridLayout = new QGridLayout();
    gridLayout->setSpacing(6);
    gridLayout->setContentsMargins(2, 2, 2, 2);

    for (int i = 0; i < 6; ++i) {
        int row = i / 2;
        int col = i % 2;

        QPushButton *button = new QPushButton(this);
        button->setFixedSize(48, 48);
        button->setCheckable(true);
        button->setChecked(m_switches[i].state);
        button->setProperty("switchId", i + 1);
        button->setProperty("class", "switchButton");
        connect(button, &QPushButton::clicked, this, &SwitchControlWidget::onSwitchButtonClicked);

        QLabel *label = new QLabel(m_switches[i].name, this);
        label->setAlignment(Qt::AlignCenter);
        label->setProperty("class", "thresholdLabel");

        QVBoxLayout *switchLayout = new QVBoxLayout();
        switchLayout->setSpacing(2);
        switchLayout->addWidget(button, 0, Qt::AlignCenter);
        switchLayout->addWidget(label, 0, Qt::AlignCenter);

        QWidget *switchWidget = new QWidget(this);
        switchWidget->setLayout(switchLayout);

        gridLayout->addWidget(switchWidget, row, col);

        m_switches[i].button = button;
        m_switches[i].label = label;
    }

    mainLayout->addLayout(gridLayout);

    for (int i = 0; i < 6; ++i) {
        updateSwitchAppearance(i + 1);
    }

    updateSwitchesVisibility();
}

void SwitchControlWidget::setSwitchState(int switchId, bool state)
{
    if (switchId < 1 || switchId > 6) {
        qWarning() << "Invalid switch ID:" << switchId;
        return;
    }

    int index = switchId - 1;
    if (m_switches[index].state != state) {
        m_switches[index].state = state;
        m_switches[index].button->setChecked(state);
        updateSwitchAppearance(switchId);
    }
}

bool SwitchControlWidget::switchState(int switchId) const
{
    if (switchId < 1 || switchId > 6) {
        qWarning() << "Invalid switch ID:" << switchId;
        return false;
    }
    return m_switches[switchId - 1].state;
}

void SwitchControlWidget::onSwitchButtonClicked()
{
    QPushButton *button = qobject_cast<QPushButton*>(sender());
    if (!button) {
        return;
    }

    int switchId = button->property("switchId").toInt();
    if (switchId < 1 || switchId > 6) {
        return;
    }

    int index = switchId - 1;
    bool newState = button->isChecked();

    m_switches[index].state = newState;
    updateSwitchAppearance(switchId);

    emit switchToggled(switchId, newState);

    qDebug() << "Switch" << switchId << "toggled to" << (newState ? "ON" : "OFF");
}

void SwitchControlWidget::updateSwitchAppearance(int switchId)
{
    if (switchId < 1 || switchId > 6) {
        return;
    }

    int index = switchId - 1;

    bool state = m_switches[index].state;
    QPushButton *button = m_switches[index].button;

    if (state) {
        button->setProperty("state", "on");
        button->setText("ON");
    } else {
        button->setProperty("state", "off");
        button->setText("OFF");
    }

    m_switches[index].label->setProperty("state", state ? "on" : "off");

    // 强制刷新按钮样式，确保动态属性变化后主题正确应用
    button->style()->unpolish(button);
    button->style()->polish(button);
    button->update();

    m_switches[index].label->style()->unpolish(m_switches[index].label);
    m_switches[index].label->style()->polish(m_switches[index].label);
    m_switches[index].label->update();
}

void SwitchControlWidget::setManualMode(bool manual)
{
    if (m_manualMode != manual) {
        m_manualMode = manual;
        m_modeCheckBox->setChecked(manual);

        if (manual) {
            for (int i = 0; i < m_switches.size(); ++i) {
                if (m_switches[i].state) {
                    m_switches[i].state = false;
                    m_switches[i].button->setChecked(false);
                    updateSwitchAppearance(i + 1);
                }
            }
        }

        updateSwitchesVisibility();
        emit modeChanged(m_manualMode);
        qDebug() << "Manual mode changed to:" << m_manualMode;
    }
}

void SwitchControlWidget::updateSwitchesVisibility()
{
    bool enabled = m_manualMode;
    for (int i = 0; i < m_switches.size(); ++i) {
        m_switches[i].button->setEnabled(enabled);
        m_switches[i].button->setVisible(true);
    }
}
