#include "sensordisplaywidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPixmap>
#include <QImage>

// 三个传感器图标都是黑字透明底 PNG：深色主题下只反色 RGB、保留 alpha，
static QPixmap themedIcon(const QString &path, bool lightTheme)
{
    QImage img(path);
    if (!lightTheme) {
        img = img.convertToFormat(QImage::Format_ARGB32);
        img.invertPixels(QImage::InvertRgb);
    }
    return QPixmap::fromImage(img).scaled(20, 20, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

SensorDisplayWidget::SensorDisplayWidget(QWidget *parent)
    : QWidget(parent)
    , m_temperatureLabel(nullptr)
    , m_humidityLabel(nullptr)
    , m_lightIntensityLabel(nullptr)
    , m_tempIcon(nullptr)
    , m_humiIcon(nullptr)
    , m_lightIcon(nullptr)
    , m_temperature(0.0)
    , m_humidity(0.0)
    , m_lightIntensity(0.0)
    , m_lightTheme(true)
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
    m_tempIcon = new QLabel(this);
    m_tempIcon->setPixmap(themedIcon(QStringLiteral(":/temperature.png"), m_lightTheme));
    m_tempIcon->setAlignment(Qt::AlignCenter);
    m_tempIcon->setFixedSize(24, 24);

    m_temperatureLabel = new QLabel(tr("0.0°C"), this);
    m_temperatureLabel->setAlignment(Qt::AlignCenter);
    m_temperatureLabel->setProperty("valueType", "temperature");

    tempLayout->addStretch();
    tempLayout->addWidget(m_tempIcon);
    tempLayout->addWidget(m_temperatureLabel);
    tempLayout->addStretch();
    mainLayout->addLayout(tempLayout);

    // 湿度显示
    QHBoxLayout *humiLayout = new QHBoxLayout();
    humiLayout->setSpacing(4);
    m_humiIcon = new QLabel(this);
    m_humiIcon->setPixmap(themedIcon(QStringLiteral(":/humidity.png"), m_lightTheme));
    m_humiIcon->setAlignment(Qt::AlignCenter);
    m_humiIcon->setFixedSize(24, 24);

    m_humidityLabel = new QLabel(tr("0.0%"), this);
    m_humidityLabel->setAlignment(Qt::AlignCenter);
    m_humidityLabel->setProperty("valueType", "humidity");

    humiLayout->addStretch();
    humiLayout->addWidget(m_humiIcon);
    humiLayout->addWidget(m_humidityLabel);
    humiLayout->addStretch();
    mainLayout->addLayout(humiLayout);

    // 光照显示
    QHBoxLayout *lightLayout = new QHBoxLayout();
    lightLayout->setSpacing(4);
    m_lightIcon = new QLabel(this);
    m_lightIcon->setPixmap(themedIcon(QStringLiteral(":/light.png"), m_lightTheme));
    m_lightIcon->setAlignment(Qt::AlignCenter);
    m_lightIcon->setFixedSize(24, 24);

    m_lightIntensityLabel = new QLabel(tr("0 Lux"), this);
    m_lightIntensityLabel->setAlignment(Qt::AlignCenter);
    m_lightIntensityLabel->setProperty("valueType", "light");

    lightLayout->addStretch();
    lightLayout->addWidget(m_lightIcon);
    lightLayout->addWidget(m_lightIntensityLabel);
    lightLayout->addStretch();
    mainLayout->addLayout(lightLayout);
}

void SensorDisplayWidget::setLightTheme(bool light)
{
    if (m_lightTheme == light) {
        return;
    }
    m_lightTheme = light;
    updateIcons();
}

void SensorDisplayWidget::updateIcons()
{
    m_tempIcon->setPixmap(themedIcon(QStringLiteral(":/temperature.png"), m_lightTheme));
    m_humiIcon->setPixmap(themedIcon(QStringLiteral(":/humidity.png"), m_lightTheme));
    m_lightIcon->setPixmap(themedIcon(QStringLiteral(":/light.png"), m_lightTheme));
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
