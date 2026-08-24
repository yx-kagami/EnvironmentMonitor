#include "sensordisplaywidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>

SensorDisplayWidget::SensorDisplayWidget(QWidget *parent)
    : QWidget(parent)
    , m_temperatureLabel(nullptr)
    , m_humidityLabel(nullptr)
    , m_lightIntensityLabel(nullptr)
    , m_temperature(0.0)
    , m_humidity(0.0)
    , m_lightIntensity(0.0)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
}

void SensorDisplayWidget::setupUI()
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 温度显示
    QHBoxLayout *tempLayout = new QHBoxLayout();
    tempLayout->setSpacing(4);
    QLabel *tempIcon = new QLabel(this);
    tempIcon->setPixmap(QPixmap(":/temperature.png").scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    tempIcon->setAlignment(Qt::AlignCenter);
    tempIcon->setFixedSize(24, 24);

    m_temperatureLabel = new QLabel(tr("0.0°C"), this);
    m_temperatureLabel->setAlignment(Qt::AlignCenter);
    m_temperatureLabel->setProperty("valueType", "temperature");

    tempLayout->addStretch();
    tempLayout->addWidget(tempIcon);
    tempLayout->addWidget(m_temperatureLabel);
    tempLayout->addStretch();
    mainLayout->addLayout(tempLayout);

    // 湿度显示
    QHBoxLayout *humiLayout = new QHBoxLayout();
    humiLayout->setSpacing(4);
    QLabel *humiIcon = new QLabel(this);
    humiIcon->setPixmap(QPixmap(":/humidity.png").scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    humiIcon->setAlignment(Qt::AlignCenter);
    humiIcon->setFixedSize(24, 24);

    m_humidityLabel = new QLabel(tr("0.0%"), this);
    m_humidityLabel->setAlignment(Qt::AlignCenter);
    m_humidityLabel->setProperty("valueType", "humidity");

    humiLayout->addStretch();
    humiLayout->addWidget(humiIcon);
    humiLayout->addWidget(m_humidityLabel);
    humiLayout->addStretch();
    mainLayout->addLayout(humiLayout);

    // 光照显示
    QHBoxLayout *lightLayout = new QHBoxLayout();
    lightLayout->setSpacing(4);
    QLabel *lightIcon = new QLabel(this);
    lightIcon->setPixmap(QPixmap(":/light.png").scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    lightIcon->setAlignment(Qt::AlignCenter);
    lightIcon->setFixedSize(24, 24);

    m_lightIntensityLabel = new QLabel(tr("0 Lux"), this);
    m_lightIntensityLabel->setAlignment(Qt::AlignCenter);
    m_lightIntensityLabel->setProperty("valueType", "light");

    lightLayout->addStretch();
    lightLayout->addWidget(lightIcon);
    lightLayout->addWidget(m_lightIntensityLabel);
    lightLayout->addStretch();
    mainLayout->addLayout(lightLayout);
}

void SensorDisplayWidget::setTemperature(double value)
{
    if (qAbs(m_temperature - value) > 0.01) {
        m_temperature = value;
        updateDisplay();
        emit temperatureChanged(value);
    }
}

void SensorDisplayWidget::setHumidity(double value)
{
    if (qAbs(m_humidity - value) > 0.01) {
        m_humidity = value;
        updateDisplay();
        emit humidityChanged(value);
    }
}

void SensorDisplayWidget::setLightIntensity(double value)
{
    if (qAbs(m_lightIntensity - value) > 0.1) {
        m_lightIntensity = value;
        updateDisplay();
        emit lightIntensityChanged(value);
    }
}

void SensorDisplayWidget::updateData(const SensorData &data)
{
    setTemperature(data.temperature);
    setHumidity(data.humidity);
    setLightIntensity(data.lightIntensity);
}

void SensorDisplayWidget::updateDisplay()
{
    m_temperatureLabel->setText(QString("%1°C").arg(m_temperature, 0, 'f', 1));
    m_humidityLabel->setText(QString("%1%").arg(m_humidity, 0, 'f', 1));
    m_lightIntensityLabel->setText(QString("%1 Lux").arg(m_lightIntensity, 0, 'f', 0));

    // 根据温度值设置颜色属性，让QSS接管样式
    if (m_temperature > 30.0) {
        m_temperatureLabel->setProperty("valueType", "temperature");
        m_temperatureLabel->setStyleSheet("color: #f87171;");
    } else if (m_temperature < 10.0) {
        m_temperatureLabel->setStyleSheet("color: #38bdf8;");
    } else {
        m_temperatureLabel->setProperty("valueType", "temperature");
        m_temperatureLabel->setStyleSheet("");
    }

    if (m_humidity > 80.0) {
        m_humidityLabel->setStyleSheet("color: #38bdf8;");
    } else if (m_humidity < 30.0) {
        m_humidityLabel->setStyleSheet("color: #fbbf24;");
    } else {
        m_humidityLabel->setProperty("valueType", "humidity");
        m_humidityLabel->setStyleSheet("");
    }

    if (m_lightIntensity > 1000.0) {
        m_lightIntensityLabel->setStyleSheet("color: #fbbf24;");
    } else if (m_lightIntensity < 200.0) {
        m_lightIntensityLabel->setStyleSheet("color: #a0a6b8;");
    } else {
        m_lightIntensityLabel->setProperty("valueType", "light");
        m_lightIntensityLabel->setStyleSheet("");
    }
}
