#ifndef SENSORDISPLAYWIDGET_H
#define SENSORDISPLAYWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "sensordata.h"

class SensorDisplayWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SensorDisplayWidget(QWidget *parent = nullptr);

    void setTemperature(double value);
    void setHumidity(double value);
    void setLightIntensity(double value);

    double temperature() const { return m_temperature; }
    double humidity() const { return m_humidity; }
    double lightIntensity() const { return m_lightIntensity; }

public slots:
    // 聚合槽：直接对接 MainWindow::sensorDataUpdated 信号，一次更新三个测量值
    void updateData(const SensorData &data);

signals:
    void temperatureChanged(double value);
    void humidityChanged(double value);
    void lightIntensityChanged(double value);

private:
    void setupUI();
    void updateDisplay();

    QLabel *m_temperatureLabel;
    QLabel *m_humidityLabel;
    QLabel *m_lightIntensityLabel;

    double m_temperature;
    double m_humidity;
    double m_lightIntensity;
};

#endif // SENSORDISPLAYWIDGET_H