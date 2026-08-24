#ifndef THRESHOLDEDITORWIDGET_H
#define THRESHOLDEDITORWIDGET_H

#include <QWidget>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QGridLayout>
#include <QPushButton>
#include <QCheckBox>

class ThresholdEditorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ThresholdEditorWidget(QWidget *parent = nullptr);

    // 温度阈值
    void setTemperatureLimits(double upper, double lower);
    double temperatureUpper() const { return m_tempUpper; }
    double temperatureLower() const { return m_tempLower; }

    // 湿度阈值
    void setHumidityLimits(double upper, double lower);
    double humidityUpper() const { return m_humUpper; }
    double humidityLower() const { return m_humLower; }

    // 光照阈值
    void setLightThreshold(double threshold);
    double lightThreshold() const { return m_lightThreshold; }

    // 阈值设置模式
    bool isThresholdMode() const { return m_thresholdMode; }
    void setThresholdMode(bool enabled);

signals:
    void temperatureLimitsChanged(double upper, double lower);
    void humidityLimitsChanged(double upper, double lower);
    void lightThresholdChanged(double threshold);

    // 增加/减少按钮信号
    void thresholdAdjustRequested(const QString &param, bool increase);

    // 阈值设置模式信号
    void thresholdModeChanged(bool enabled);

private slots:
    // 增加/减少按钮槽函数
    void onTempUpperAddClicked();
    void onTempUpperDownClicked();
    void onTempLowerAddClicked();
    void onTempLowerDownClicked();
    void onHumUpperAddClicked();
    void onHumUpperDownClicked();
    void onHumLowerAddClicked();
    void onHumLowerDownClicked();
    void onLightThresholdAddClicked();
    void onLightThresholdDownClicked();

    // 阈值设置模式槽函数
    void onThresholdCheckBoxToggled(bool checked);

private:
    void setupUI();
    void updateDisplay();

    // 温度阈值
    QDoubleSpinBox *m_tempUpperSpinBox;
    QDoubleSpinBox *m_tempLowerSpinBox;
    QPushButton *m_tempUpperAddButton;
    QPushButton *m_tempUpperDownButton;
    QPushButton *m_tempLowerAddButton;
    QPushButton *m_tempLowerDownButton;
    double m_tempUpper;
    double m_tempLower;

    // 湿度阈值
    QDoubleSpinBox *m_humUpperSpinBox;
    QDoubleSpinBox *m_humLowerSpinBox;
    QPushButton *m_humUpperAddButton;
    QPushButton *m_humUpperDownButton;
    QPushButton *m_humLowerAddButton;
    QPushButton *m_humLowerDownButton;
    double m_humUpper;
    double m_humLower;

    // 光照阈值
    QDoubleSpinBox *m_lightThresholdSpinBox;
    QPushButton *m_lightThresholdAddButton;
    QPushButton *m_lightThresholdDownButton;
    double m_lightThreshold;

    // 阈值设置模式
    QCheckBox *m_thresholdCheckBox;
    bool m_thresholdMode;
};

#endif // THRESHOLDEDITORWIDGET_H