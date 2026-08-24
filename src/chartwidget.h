#ifndef CHARTWIDGET_H
#define CHARTWIDGET_H

#include <QWidget>
#include <QtCharts>
#include <QTimer>
#include "sensordata.h"


class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget *parent = nullptr);
    ~ChartWidget();

    enum ChartType {
        TemperatureChart,
        HumidityChart,
        LightChart,
        AllCharts
    };

    // 设置图表类型
    void setChartType(ChartType type);
    ChartType chartType() const { return m_chartType; }

    // 设置时间范围（小时）
    void setTimeRange(int hours);
    int timeRange() const { return m_timeRangeHours; }

    // 添加数据点
    void addDataPoint(const SensorData &data);

    // 更新图表数据
    void updateChart(const QList<SensorData> &data);

    // 清空图表
    void clearChart();

    // 主题切换
    void setLightTheme(bool light);

signals:
    void chartTypeChanged(ChartType type);
    void timeRangeChanged(int hours);

private slots:
    void updateChartView();

private:
    void setupUI();
    void setupChart();
    void updateAxes();
    void cleanupOldData();

    // 图表组件
    QChart *m_chart;
    QChartView *m_chartView;
    QLineSeries *m_temperatureSeries;
    QLineSeries *m_humiditySeries;
    QLineSeries *m_lightSeries;
    QDateTimeAxis *m_axisX;
    QValueAxis *m_axisY;

    // 数据
    QList<SensorData> m_chartData;
    ChartType m_chartType;
    int m_timeRangeHours;

    // 定时器（用于定期更新）
    QTimer *m_updateTimer;

    // 控件
    QComboBox *m_typeComboBox;
    QComboBox *m_timeComboBox;

    // 显示范围
    QDateTime m_minTime;
    QDateTime m_maxTime;
    double m_minValue;
    double m_maxValue;
};

#endif // CHARTWIDGET_H