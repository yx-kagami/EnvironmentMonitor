#include "thresholdeditorwidget.h"
#include <QGridLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>

ThresholdEditorWidget::ThresholdEditorWidget(QWidget *parent)
    : QWidget(parent)
    , m_tempUpperSpinBox(nullptr)
    , m_tempLowerSpinBox(nullptr)
    , m_tempUpperAddButton(nullptr)
    , m_tempUpperDownButton(nullptr)
    , m_tempLowerAddButton(nullptr)
    , m_tempLowerDownButton(nullptr)
    , m_humUpperSpinBox(nullptr)
    , m_humLowerSpinBox(nullptr)
    , m_humUpperAddButton(nullptr)
    , m_humUpperDownButton(nullptr)
    , m_humLowerAddButton(nullptr)
    , m_humLowerDownButton(nullptr)
    , m_lightThresholdSpinBox(nullptr)
    , m_lightThresholdAddButton(nullptr)
    , m_lightThresholdDownButton(nullptr)
    , m_tempUpper(30.0)
    , m_tempLower(10.0)
    , m_humUpper(80.0)
    , m_humLower(30.0)
    , m_lightThreshold(500.0)
    , m_thresholdCheckBox(nullptr)
    , m_thresholdMode(false)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

void ThresholdEditorWidget::setupUI()
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 4, 8, 4);

    auto createControl = [&](const QString &labelText, QPushButton *&downBtn, QDoubleSpinBox *&spinBox, QPushButton *&upBtn) -> QHBoxLayout* {
        QLabel *label = new QLabel(labelText, this);
        label->setProperty("class", "thresholdLabel");
        label->setMinimumWidth(60);

        downBtn = new QPushButton("-", this);
        downBtn->setFixedSize(22, 22);
        downBtn->setProperty("class", "thresholdButton");

        upBtn = new QPushButton("+", this);
        upBtn->setFixedSize(22, 22);
        upBtn->setProperty("class", "thresholdButton");

        spinBox = new QDoubleSpinBox(this);
        spinBox->setReadOnly(true);
        spinBox->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spinBox->setProperty("class", "thresholdSpinBox");
        spinBox->setFixedWidth(75);
        spinBox->setFixedHeight(22);

        QHBoxLayout *rowLayout = new QHBoxLayout();
        rowLayout->setSpacing(3);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->addWidget(label);
        rowLayout->addWidget(downBtn);
        rowLayout->addWidget(spinBox);
        rowLayout->addWidget(upBtn);
        return rowLayout;
    };

    // 左侧：温度上限、湿度上限、光照阈值
    QVBoxLayout *leftCol = new QVBoxLayout();
    leftCol->setSpacing(4);
    leftCol->setContentsMargins(0, 0, 0, 0);

    leftCol->addLayout(createControl(tr("温度上限:"), m_tempUpperDownButton, m_tempUpperSpinBox, m_tempUpperAddButton));
    connect(m_tempUpperDownButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onTempUpperDownClicked);
    connect(m_tempUpperAddButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onTempUpperAddClicked);
    m_tempUpperSpinBox->setRange(-50.0, 100.0);
    m_tempUpperSpinBox->setSingleStep(0.5);
    m_tempUpperSpinBox->setValue(m_tempUpper);
    m_tempUpperSpinBox->setSuffix(" °C");

    leftCol->addLayout(createControl(tr("湿度上限:"), m_humUpperDownButton, m_humUpperSpinBox, m_humUpperAddButton));
    connect(m_humUpperDownButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onHumUpperDownClicked);
    connect(m_humUpperAddButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onHumUpperAddClicked);
    m_humUpperSpinBox->setRange(0.0, 100.0);
    m_humUpperSpinBox->setSingleStep(1.0);
    m_humUpperSpinBox->setValue(m_humUpper);
    m_humUpperSpinBox->setSuffix(" %");

    leftCol->addLayout(createControl(tr("光照阈值:"), m_lightThresholdDownButton, m_lightThresholdSpinBox, m_lightThresholdAddButton));
    connect(m_lightThresholdDownButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onLightThresholdDownClicked);
    connect(m_lightThresholdAddButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onLightThresholdAddClicked);
    m_lightThresholdSpinBox->setRange(0.0, 10000.0);
    m_lightThresholdSpinBox->setSingleStep(10.0);
    m_lightThresholdSpinBox->setValue(m_lightThreshold);
    m_lightThresholdSpinBox->setSuffix(" Lux");

    // 右侧：温度下限、湿度下限、阈值模式
    QVBoxLayout *rightCol = new QVBoxLayout();
    rightCol->setSpacing(4);
    rightCol->setContentsMargins(0, 0, 0, 0);

    rightCol->addLayout(createControl(tr("温度下限:"), m_tempLowerDownButton, m_tempLowerSpinBox, m_tempLowerAddButton));
    connect(m_tempLowerDownButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onTempLowerDownClicked);
    connect(m_tempLowerAddButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onTempLowerAddClicked);
    m_tempLowerSpinBox->setRange(-50.0, 100.0);
    m_tempLowerSpinBox->setSingleStep(0.5);
    m_tempLowerSpinBox->setValue(m_tempLower);
    m_tempLowerSpinBox->setSuffix(" °C");

    rightCol->addLayout(createControl(tr("湿度下限:"), m_humLowerDownButton, m_humLowerSpinBox, m_humLowerAddButton));
    connect(m_humLowerDownButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onHumLowerDownClicked);
    connect(m_humLowerAddButton, &QPushButton::clicked, this, &ThresholdEditorWidget::onHumLowerAddClicked);
    m_humLowerSpinBox->setRange(0.0, 100.0);
    m_humLowerSpinBox->setSingleStep(1.0);
    m_humLowerSpinBox->setValue(m_humLower);
    m_humLowerSpinBox->setSuffix(" %");

    QHBoxLayout *modeLayout = new QHBoxLayout();
    modeLayout->setSpacing(4);
    modeLayout->setContentsMargins(0, 0, 0, 0);
    QLabel *thresholdModeLabel = new QLabel(tr("阈值模式:"), this);
    thresholdModeLabel->setProperty("class", "thresholdLabel");
    thresholdModeLabel->setMinimumWidth(50);
    modeLayout->addWidget(thresholdModeLabel);

    m_thresholdCheckBox = new QCheckBox(tr("启用"), this);
    m_thresholdCheckBox->setChecked(m_thresholdMode);
    m_thresholdCheckBox->setProperty("class", "modeCheckBox");
    connect(m_thresholdCheckBox, &QCheckBox::toggled,
            this, &ThresholdEditorWidget::onThresholdCheckBoxToggled);
    modeLayout->addWidget(m_thresholdCheckBox);
    modeLayout->addStretch();
    rightCol->addLayout(modeLayout);

    mainLayout->addLayout(leftCol);
    mainLayout->addSpacing(20);
    mainLayout->addLayout(rightCol);
    mainLayout->addStretch();

    updateDisplay();
}

void ThresholdEditorWidget::setTemperatureLimits(double upper, double lower)
{
    if (qAbs(m_tempUpper - upper) > 0.01 || qAbs(m_tempLower - lower) > 0.01) {
        m_tempUpper = upper;
        m_tempLower = lower;

        m_tempUpperSpinBox->setValue(upper);
        m_tempLowerSpinBox->setValue(lower);

        updateDisplay();
        emit temperatureLimitsChanged(upper, lower);
    }
}

void ThresholdEditorWidget::setHumidityLimits(double upper, double lower)
{
    if (qAbs(m_humUpper - upper) > 0.01 || qAbs(m_humLower - lower) > 0.01) {
        m_humUpper = upper;
        m_humLower = lower;

        m_humUpperSpinBox->setValue(upper);
        m_humLowerSpinBox->setValue(lower);

        updateDisplay();
        emit humidityLimitsChanged(upper, lower);
    }
}

void ThresholdEditorWidget::setLightThreshold(double threshold)
{
    if (qAbs(m_lightThreshold - threshold) > 0.01) {
        m_lightThreshold = threshold;

        m_lightThresholdSpinBox->setValue(threshold);

        updateDisplay();
        emit lightThresholdChanged(threshold);
    }
}

void ThresholdEditorWidget::updateDisplay()
{
    // 只更新数值，不设置硬编码样式表，让全局QSS生效
    m_tempUpperSpinBox->setValue(m_tempUpper);
    m_tempLowerSpinBox->setValue(m_tempLower);
    m_humUpperSpinBox->setValue(m_humUpper);
    m_humLowerSpinBox->setValue(m_humLower);
    m_lightThresholdSpinBox->setValue(m_lightThreshold);

    // 上限低于下限时的异常提示：内联红色样式覆盖全局QSS
    bool tempError = m_tempUpper < m_tempLower;
    bool humError = m_humUpper < m_humLower;

    QString errorStyle = "QDoubleSpinBox { background-color: #3f1a1a; border: 1px solid #ef4444; color: #fca5a5; border-radius: 6px; padding: 4px 8px; }";
    QString normalStyle = "";

    m_tempUpperSpinBox->setStyleSheet(tempError ? errorStyle : normalStyle);
    m_tempLowerSpinBox->setStyleSheet(tempError ? errorStyle : normalStyle);
    m_humUpperSpinBox->setStyleSheet(humError ? errorStyle : normalStyle);
    m_humLowerSpinBox->setStyleSheet(humError ? errorStyle : normalStyle);
}

// ========== 按钮槽函数实现 ==========

void ThresholdEditorWidget::onTempUpperAddClicked()
{
    emit thresholdAdjustRequested("TempMax", true);
}

void ThresholdEditorWidget::onTempUpperDownClicked()
{
    emit thresholdAdjustRequested("TempMax", false);
}

void ThresholdEditorWidget::onTempLowerAddClicked()
{
    emit thresholdAdjustRequested("TempMin", true);
}

void ThresholdEditorWidget::onTempLowerDownClicked()
{
    emit thresholdAdjustRequested("TempMin", false);
}

void ThresholdEditorWidget::onHumUpperAddClicked()
{
    emit thresholdAdjustRequested("HumiMax", true);
}

void ThresholdEditorWidget::onHumUpperDownClicked()
{
    emit thresholdAdjustRequested("HumiMax", false);
}

void ThresholdEditorWidget::onHumLowerAddClicked()
{
    emit thresholdAdjustRequested("HumiMin", true);
}

void ThresholdEditorWidget::onHumLowerDownClicked()
{
    emit thresholdAdjustRequested("HumiMin", false);
}

void ThresholdEditorWidget::onLightThresholdAddClicked()
{
    emit thresholdAdjustRequested("Light", true);
}

void ThresholdEditorWidget::onLightThresholdDownClicked()
{
    emit thresholdAdjustRequested("Light", false);
}

void ThresholdEditorWidget::setThresholdMode(bool enabled)
{
    if (m_thresholdMode != enabled) {
        m_thresholdMode = enabled;
        m_thresholdCheckBox->setChecked(enabled);
        emit thresholdModeChanged(m_thresholdMode);
        qDebug() << "Threshold mode set to:" << m_thresholdMode;
    }
}

void ThresholdEditorWidget::onThresholdCheckBoxToggled(bool checked)
{
    if (m_thresholdMode != checked) {
        m_thresholdMode = checked;
        emit thresholdModeChanged(m_thresholdMode);
        qDebug() << "Threshold mode changed to:" << m_thresholdMode;
    }
}
