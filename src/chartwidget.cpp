#include "chartwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QDebug>
#include <QStyle>

ChartWidget::ChartWidget(QWidget *parent)
    : QWidget(parent)
    , m_chart(nullptr)
    , m_chartView(nullptr)
    , m_temperatureSeries(nullptr)
    , m_humiditySeries(nullptr)
    , m_lightSeries(nullptr)
    , m_axisX(nullptr)
    , m_axisY(nullptr)
    , m_chartType(AllCharts)
    , m_timeRangeHours(1)
    , m_updateTimer(new QTimer(this))
    , m_typeComboBox(nullptr)
    , m_timeComboBox(nullptr)
    , m_minValue(0.0)
    , m_maxValue(100.0)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setupUI();
    setupChart();

    m_updateTimer->setInterval(2000);
    connect(m_updateTimer, &QTimer::timeout, this, &ChartWidget::updateChartView);
    m_updateTimer->start();
}

ChartWidget::~ChartWidget()
{
    // QChartView 已取得 m_chart 所有权，无需手动 delete
}

void ChartWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);

    // 控制栏
    QFrame *controlFrame = new QFrame(this);
    controlFrame->setProperty("class", "chartControlFrame");

    QHBoxLayout *controlLayout = new QHBoxLayout(controlFrame);
    controlLayout->setSpacing(8);
    controlLayout->setContentsMargins(6, 4, 6, 4);

    QHBoxLayout *leftControlLayout = new QHBoxLayout();
    leftControlLayout->setSpacing(8);

    QLabel *typeLabel = new QLabel(tr("图表类型:"), this);
    typeLabel->setProperty("class", "chartLabel");
    leftControlLayout->addWidget(typeLabel);

    m_typeComboBox = new QComboBox(this);
    QComboBox *typeComboBox = m_typeComboBox;
    typeComboBox->addItem(tr("温度"), TemperatureChart);
    typeComboBox->addItem(tr("湿度"), HumidityChart);
    typeComboBox->addItem(tr("光照"), LightChart);
    typeComboBox->addItem(tr("全部"), AllCharts);
    typeComboBox->setCurrentIndex(3);
    typeComboBox->setProperty("class", "chartControl");
    typeComboBox->setFixedWidth(100);
    connect(typeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, typeComboBox](int index) {
                ChartType type = static_cast<ChartType>(typeComboBox->itemData(index).toInt());
                setChartType(type);
            });
    leftControlLayout->addWidget(typeComboBox);

    leftControlLayout->addSpacing(12);

    QLabel *timeLabel = new QLabel(tr("时间范围:"), this);
    timeLabel->setProperty("class", "chartLabel");
    leftControlLayout->addWidget(timeLabel);

    m_timeComboBox = new QComboBox(this);
    QComboBox *timeComboBox = m_timeComboBox;
    timeComboBox->addItem(tr("1小时"), 1);
    timeComboBox->addItem(tr("6小时"), 6);
    timeComboBox->addItem(tr("24小时"), 24);
    timeComboBox->addItem(tr("自定义"), 0);
    timeComboBox->setCurrentIndex(0);
    timeComboBox->setProperty("class", "chartControl");
    timeComboBox->setFixedWidth(100);
    connect(timeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this, timeComboBox](int index) {
                int hours = timeComboBox->itemData(index).toInt();
                if (hours > 0) {
                    setTimeRange(hours);
                }
            });
    leftControlLayout->addWidget(timeComboBox);

    controlLayout->addLayout(leftControlLayout);
    controlLayout->addStretch();

    mainLayout->addWidget(controlFrame);

    // 图表视图容器
    QFrame *chartFrame = new QFrame(this);
    chartFrame->setProperty("class", "chartFrame");

    QVBoxLayout *chartFrameLayout = new QVBoxLayout(chartFrame);
    chartFrameLayout->setSpacing(0);
    chartFrameLayout->setContentsMargins(8, 8, 8, 8);

    m_chartView = new QChartView(this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setProperty("class", "chartView");

    chartFrameLayout->addWidget(m_chartView);
    mainLayout->addWidget(chartFrame, 1);
}

void ChartWidget::setupChart()
{
    m_chart = new QChart();
    m_chart->setBackgroundBrush(QBrush(QColor(0x1e, 0x21, 0x2b)));
    m_chart->setTitleBrush(QBrush(QColor(0xe0, 0xe2, 0xe8)));
    m_chart->setTitle(tr("环境数据趋势图"));
    m_chart->setTitleFont(QFont("Segoe UI", 12, QFont::Bold));
    m_chart->setMargins(QMargins(12, 12, 12, 12));

    m_chart->legend()->setVisible(true);
    m_chart->legend()->setLabelColor(QColor(0xc0, 0xc4, 0xd0));
    m_chart->legend()->setBackgroundVisible(false);
    m_chart->legend()->setFont(QFont("Segoe UI", 9));
    m_chart->legend()->setAlignment(Qt::AlignTop);

    m_chart->setAnimationOptions(QChart::NoAnimation);

    m_temperatureSeries = new QLineSeries();
    m_temperatureSeries->setName(tr("温度 (°C)"));
    m_temperatureSeries->setColor(QColor(0xf8, 0x71, 0x71));
    m_temperatureSeries->setPen(QPen(QColor(0xf8, 0x71, 0x71), 2.0));

    m_humiditySeries = new QLineSeries();
    m_humiditySeries->setName(tr("湿度 (%)"));
    m_humiditySeries->setColor(QColor(0x38, 0xbd, 0xf8));
    m_humiditySeries->setPen(QPen(QColor(0x38, 0xbd, 0xf8), 2.0));

    m_lightSeries = new QLineSeries();
    m_lightSeries->setName(tr("光照 (Lux)"));
    m_lightSeries->setColor(QColor(0xfb, 0xbf, 0x24));
    m_lightSeries->setPen(QPen(QColor(0xfb, 0xbf, 0x24), 2.0));

    m_chart->addSeries(m_temperatureSeries);
    m_chart->addSeries(m_humiditySeries);
    m_chart->addSeries(m_lightSeries);

    m_axisX = new QDateTimeAxis();
    m_axisX->setFormat("HH:mm");
    m_axisX->setTitleText(tr("时间"));
    m_axisX->setTitleBrush(QBrush(QColor(0xc0, 0xc4, 0xd0)));
    m_axisX->setTitleFont(QFont("Segoe UI", 9, QFont::Medium));
    m_axisX->setLabelsBrush(QBrush(QColor(0xa0, 0xa6, 0xb8)));
    m_axisX->setLabelsFont(QFont("Segoe UI", 8));
    m_axisX->setGridLineColor(QColor(0x3a, 0x3f, 0x52));
    m_axisX->setGridLinePen(QPen(QColor(0x3a, 0x3f, 0x52), 0.6, Qt::DashLine));
    m_axisX->setLinePenColor(QColor(0x5a, 0x60, 0x72));
    m_axisX->setLinePen(QPen(QColor(0x5a, 0x60, 0x72), 1.0));

    m_axisY = new QValueAxis();
    m_axisY->setTitleText(tr("数值"));
    m_axisY->setTitleBrush(QBrush(QColor(0xc0, 0xc4, 0xd0)));
    m_axisY->setTitleFont(QFont("Segoe UI", 9, QFont::Medium));
    m_axisY->setLabelsBrush(QBrush(QColor(0xa0, 0xa6, 0xb8)));
    m_axisY->setLabelsFont(QFont("Segoe UI", 8));
    m_axisY->setGridLineColor(QColor(0x3a, 0x3f, 0x52));
    m_axisY->setGridLinePen(QPen(QColor(0x3a, 0x3f, 0x52), 0.6, Qt::DashLine));
    m_axisY->setLinePenColor(QColor(0x5a, 0x60, 0x72));
    m_axisY->setLinePen(QPen(QColor(0x5a, 0x60, 0x72), 1.0));
    m_axisY->setRange(0.0, 100.0);

    m_chart->addAxis(m_axisX, Qt::AlignBottom);
    m_chart->addAxis(m_axisY, Qt::AlignLeft);

    m_temperatureSeries->attachAxis(m_axisX);
    m_temperatureSeries->attachAxis(m_axisY);
    m_humiditySeries->attachAxis(m_axisX);
    m_humiditySeries->attachAxis(m_axisY);
    m_lightSeries->attachAxis(m_axisX);
    m_lightSeries->attachAxis(m_axisY);

    m_chartView->setChart(m_chart);
}

void ChartWidget::setLightTheme(bool light)
{
    if (light) {
        m_chart->setBackgroundBrush(QBrush(QColor(0xff, 0xff, 0xff)));
        m_chart->setTitleBrush(QBrush(QColor(0x1e, 0x29, 0x3b)));
        m_chart->legend()->setLabelColor(QColor(0x47, 0x48, 0x4f));

        m_axisX->setTitleBrush(QBrush(QColor(0x64, 0x72, 0x8b)));
        m_axisX->setLabelsBrush(QBrush(QColor(0x64, 0x72, 0x8b)));
        m_axisX->setGridLineColor(QColor(0xe2, 0xe8, 0xf0));
        m_axisX->setGridLinePen(QPen(QColor(0xe2, 0xe8, 0xf0), 0.6, Qt::DashLine));
        m_axisX->setLinePenColor(QColor(0xcb, 0xd5, 0xe1));
        m_axisX->setLinePen(QPen(QColor(0xcb, 0xd5, 0xe1), 1.0));

        m_axisY->setTitleBrush(QBrush(QColor(0x64, 0x72, 0x8b)));
        m_axisY->setLabelsBrush(QBrush(QColor(0x64, 0x72, 0x8b)));
        m_axisY->setGridLineColor(QColor(0xe2, 0xe8, 0xf0));
        m_axisY->setGridLinePen(QPen(QColor(0xe2, 0xe8, 0xf0), 0.6, Qt::DashLine));
        m_axisY->setLinePenColor(QColor(0xcb, 0xd5, 0xe1));
        m_axisY->setLinePen(QPen(QColor(0xcb, 0xd5, 0xe1), 1.0));
    } else {
        m_chart->setBackgroundBrush(QBrush(QColor(0x1e, 0x21, 0x2b)));
        m_chart->setTitleBrush(QBrush(QColor(0xe0, 0xe2, 0xe8)));
        m_chart->legend()->setLabelColor(QColor(0xc0, 0xc4, 0xd0));

        m_axisX->setTitleBrush(QBrush(QColor(0xc0, 0xc4, 0xd0)));
        m_axisX->setLabelsBrush(QBrush(QColor(0xa0, 0xa6, 0xb8)));
        m_axisX->setGridLineColor(QColor(0x3a, 0x3f, 0x52));
        m_axisX->setGridLinePen(QPen(QColor(0x3a, 0x3f, 0x52), 0.6, Qt::DashLine));
        m_axisX->setLinePenColor(QColor(0x5a, 0x60, 0x72));
        m_axisX->setLinePen(QPen(QColor(0x5a, 0x60, 0x72), 1.0));

        m_axisY->setTitleBrush(QBrush(QColor(0xc0, 0xc4, 0xd0)));
        m_axisY->setLabelsBrush(QBrush(QColor(0xa0, 0xa6, 0xb8)));
        m_axisY->setGridLineColor(QColor(0x3a, 0x3f, 0x52));
        m_axisY->setGridLinePen(QPen(QColor(0x3a, 0x3f, 0x52), 0.6, Qt::DashLine));
        m_axisY->setLinePenColor(QColor(0x5a, 0x60, 0x72));
        m_axisY->setLinePen(QPen(QColor(0x5a, 0x60, 0x72), 1.0));
    }

    // 强制刷新下拉框视图样式
    auto refreshComboStyle = [](QComboBox *combo) {
        if (!combo) return;
        QAbstractItemView *view = combo->view();
        if (view) {
            view->style()->unpolish(view);
            view->style()->polish(view);
            view->update();
        }
        combo->style()->unpolish(combo);
        combo->style()->polish(combo);
        combo->update();
    };
    refreshComboStyle(m_typeComboBox);
    refreshComboStyle(m_timeComboBox);
}

void ChartWidget::setChartType(ChartType type)
{
    if (m_chartType != type) {
        m_chartType = type;

        switch (type) {
        case TemperatureChart:
            m_temperatureSeries->setVisible(true);
            m_humiditySeries->setVisible(false);
            m_lightSeries->setVisible(false);
            m_axisY->setTitleText(tr("温度 (°C)"));
            break;
        case HumidityChart:
            m_temperatureSeries->setVisible(false);
            m_humiditySeries->setVisible(true);
            m_lightSeries->setVisible(false);
            m_axisY->setTitleText(tr("湿度 (%)"));
            break;
        case LightChart:
            m_temperatureSeries->setVisible(false);
            m_humiditySeries->setVisible(false);
            m_lightSeries->setVisible(true);
            m_axisY->setTitleText(tr("光照 (Lux)"));
            break;
        case AllCharts:
            m_temperatureSeries->setVisible(true);
            m_humiditySeries->setVisible(true);
            m_lightSeries->setVisible(true);
            m_axisY->setTitleText(tr("数值"));
            break;
        }

        emit chartTypeChanged(type);
    }
}

void ChartWidget::setTimeRange(int hours)
{
    if (hours > 0 && m_timeRangeHours != hours) {
        m_timeRangeHours = hours;
        updateAxes();
        emit timeRangeChanged(hours);
    }
}

void ChartWidget::addDataPoint(const SensorData &data)
{
    if (!data.isValid()) {
        return;
    }

    m_chartData.append(data);
    cleanupOldData();

    QDateTime currentTime = data.timestamp;
    if (!m_minTime.isValid() || currentTime < m_minTime) {
        m_minTime = currentTime;
    }
    if (!m_maxTime.isValid() || currentTime > m_maxTime) {
        m_maxTime = currentTime;
    }

    double temp = data.temperature;
    double hum = data.humidity;
    double light = data.lightIntensity;

    if (temp < m_minValue) m_minValue = temp;
    if (temp > m_maxValue) m_maxValue = temp;
    if (hum < m_minValue) m_minValue = hum;
    if (hum > m_maxValue) m_maxValue = hum;
    if (light < m_minValue) m_minValue = light;
    if (light > m_maxValue) m_maxValue = light;

    qint64 msecs = data.timestamp.toMSecsSinceEpoch();
    m_temperatureSeries->append(msecs, temp);
    m_humiditySeries->append(msecs, hum);
    m_lightSeries->append(msecs, light);
}

void ChartWidget::updateChart(const QList<SensorData> &data)
{
    clearChart();

    for (const SensorData &item : data) {
        addDataPoint(item);
    }

    updateChartView();
}

void ChartWidget::clearChart()
{
    m_chartData.clear();
    m_temperatureSeries->clear();
    m_humiditySeries->clear();
    m_lightSeries->clear();
    m_minTime = QDateTime();
    m_maxTime = QDateTime();
    m_minValue = 0.0;
    m_maxValue = 100.0;
}

void ChartWidget::updateChartView()
{
    updateAxes();

    double range = m_maxValue - m_minValue;
    double padding = range * 0.1;
    if (padding < 1.0) padding = 1.0;

    m_axisY->setRange(m_minValue - padding, m_maxValue + padding);
}

void ChartWidget::updateAxes()
{
    if (!m_minTime.isValid() || !m_maxTime.isValid()) {
        QDateTime now = QDateTime::currentDateTime();
        QDateTime past = now.addSecs(-3600 * m_timeRangeHours);
        m_axisX->setRange(past, now);
        return;
    }

    QDateTime endTime = m_maxTime;
    QDateTime startTime = endTime.addSecs(-3600 * m_timeRangeHours);

    if (startTime < m_minTime) {
        startTime = m_minTime;
    }

    m_axisX->setRange(startTime, endTime);
}

void ChartWidget::cleanupOldData()
{
    if (m_timeRangeHours <= 0) {
        return;
    }

    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-3600 * m_timeRangeHours * 2);

    while (!m_chartData.isEmpty() && m_chartData.first().timestamp < cutoff) {
        m_chartData.removeFirst();
    }

    if (m_chartData.size() > 1000) {
        QList<SensorData> recentData = m_chartData.mid(m_chartData.size() - 500);
        clearChart();
        for (const SensorData &data : recentData) {
            addDataPoint(data);
        }
    }
}
