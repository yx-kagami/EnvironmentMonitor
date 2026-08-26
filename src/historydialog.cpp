#include "historydialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QHeaderView>
#include <QDebug>
#include <QThreadPool>

// CSV 导出任务：把纯文件 I/O 放到线程池执行，结果通过信号排队送回 GUI 线程
//（QObject + QRunnable 双继承模式）
class CsvExportTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    CsvExportTask(QString filePath, QList<SensorData> dataList)
        : m_filePath(std::move(filePath))
        , m_dataList(std::move(dataList))
    {
        setAutoDelete(false);   // 生命周期显式管理：finished 投递完后 deleteLater
    }

    void run() override
    {
        const bool ok = HistoryDialog::exportToCsv(m_filePath, m_dataList);
        emit finished(ok, m_filePath, static_cast<int>(m_dataList.size()));
    }

signals:
    void finished(bool success, const QString &filePath, int rowCount);

private:
    QString m_filePath;
    QList<SensorData> m_dataList;
};

HistoryDialog::HistoryDialog(DatabaseManager *dbManager, QWidget *parent)
    : QDialog(parent)
    , m_databaseManager(dbManager)
    , m_startTimeEdit(nullptr)
    , m_endTimeEdit(nullptr)
    , m_quickRangeCombo(nullptr)
    , m_queryButton(nullptr)
    , m_exportButton(nullptr)
    , m_tableView(nullptr)
    , m_tableModel(nullptr)
    , m_statisticsLabel(nullptr)
    , m_countLabel(nullptr)
{
    setWindowTitle(tr("历史数据查询与分析"));
    resize(900, 600);
    setupUI();

    // 默认查询最近24小时
    onQuickRangeChanged(1);
    onQueryClicked();
}

HistoryDialog::~HistoryDialog()
{
}

void HistoryDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // 查询条件区域
    QGroupBox *queryGroup = new QGroupBox(tr("查询条件"), this);
    QGridLayout *queryLayout = new QGridLayout(queryGroup);
    queryLayout->setSpacing(10);

    QLabel *quickLabel = new QLabel(tr("快速范围:"), this);
    m_quickRangeCombo = new QComboBox(this);
    m_quickRangeCombo->addItem(tr("全部数据"), 0);
    m_quickRangeCombo->addItem(tr("最近1小时"), 1);
    m_quickRangeCombo->addItem(tr("最近6小时"), 6);
    m_quickRangeCombo->addItem(tr("最近24小时"), 24);
    m_quickRangeCombo->addItem(tr("最近7天"), 24 * 7);
    m_quickRangeCombo->addItem(tr("最近30天"), 24 * 30);
    connect(m_quickRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &HistoryDialog::onQuickRangeChanged);

    QLabel *startLabel = new QLabel(tr("开始时间:"), this);
    m_startTimeEdit = new QDateTimeEdit(this);
    m_startTimeEdit->setCalendarPopup(true);
    m_startTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm");

    QLabel *endLabel = new QLabel(tr("结束时间:"), this);
    m_endTimeEdit = new QDateTimeEdit(this);
    m_endTimeEdit->setCalendarPopup(true);
    m_endTimeEdit->setDisplayFormat("yyyy-MM-dd HH:mm");

    m_queryButton = new QPushButton(tr("查询数据"), this);
    m_queryButton->setProperty("class", "thresholdButton");
    connect(m_queryButton, &QPushButton::clicked, this, &HistoryDialog::onQueryClicked);

    m_exportButton = new QPushButton(tr("导出CSV"), this);
    m_exportButton->setProperty("class", "thresholdButton");
    connect(m_exportButton, &QPushButton::clicked, this, &HistoryDialog::onExportClicked);

    queryLayout->addWidget(quickLabel, 0, 0);
    queryLayout->addWidget(m_quickRangeCombo, 0, 1);
    queryLayout->addWidget(startLabel, 0, 2);
    queryLayout->addWidget(m_startTimeEdit, 0, 3);
    queryLayout->addWidget(endLabel, 0, 4);
    queryLayout->addWidget(m_endTimeEdit, 0, 5);
    queryLayout->addWidget(m_queryButton, 0, 6);
    queryLayout->addWidget(m_exportButton, 0, 7);

    mainLayout->addWidget(queryGroup);

    // 数据统计区域
    QHBoxLayout *statLayout = new QHBoxLayout();
    m_countLabel = new QLabel(tr("数据条数: 0"), this);
    m_statisticsLabel = new QLabel(tr("统计信息: -"), this);
    m_statisticsLabel->setWordWrap(true);
    statLayout->addWidget(m_countLabel);
    statLayout->addWidget(m_statisticsLabel, 1);
    mainLayout->addLayout(statLayout);

    // 数据表格
    m_tableView = new QTableView(this);
    m_tableView->setAlternatingRowColors(true);
    m_tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableView->horizontalHeader()->setStretchLastSection(true);
    m_tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_tableView->verticalHeader()->setVisible(false);

    m_tableModel = new QStandardItemModel(this);
    m_tableModel->setHorizontalHeaderLabels({
        tr("时间"), tr("温度(°C)"), tr("湿度(%)"), tr("光照(Lux)"),
        tr("温度上限"), tr("温度下限"), tr("湿度上限"), tr("湿度下限"), tr("光照阈值"),
        tr("制冷"), tr("加热"), tr("加湿"), tr("除湿"), tr("补光"), tr("报警")
    });
    m_tableView->setModel(m_tableModel);

    mainLayout->addWidget(m_tableView, 1);
}

void HistoryDialog::onQuickRangeChanged(int index)
{
    Q_UNUSED(index); // 区间值从 currentData() 取，index 形参仅为匹配信号签名
    int hours = m_quickRangeCombo->currentData().toInt();
    QDateTime end = QDateTime::currentDateTime();
    QDateTime start;

    if (hours == 0) {
        start = QDateTime::fromString("2000-01-01 00:00", "yyyy-MM-dd HH:mm");
    } else {
        start = end.addSecs(-hours * 3600);
    }

    m_startTimeEdit->setDateTime(start);
    m_endTimeEdit->setDateTime(end);
}

void HistoryDialog::onQueryClicked()
{
    QDateTime start = m_startTimeEdit->dateTime();
    QDateTime end = m_endTimeEdit->dateTime();

    if (start > end) {
        QMessageBox::warning(this, tr("查询错误"), tr("开始时间不能晚于结束时间"));
        return;
    }

    loadData(start, end);
}

void HistoryDialog::loadData(const QDateTime &start, const QDateTime &end)
{
    m_tableModel->removeRows(0, m_tableModel->rowCount());

    if (!m_databaseManager || !m_databaseManager->isOpen()) {
        QMessageBox::warning(this, tr("数据库错误"), tr("数据库未打开或不可用"));
        return;
    }

    QList<SensorData> dataList = m_databaseManager->getHistoricalData(start, end);

    m_countLabel->setText(tr("数据条数: %1").arg(dataList.size()));

    if (dataList.isEmpty()) {
        m_statisticsLabel->setText(tr("统计信息: 该时间段内无数据"));
        return;
    }

    for (const SensorData &data : dataList) {
        QList<QStandardItem *> rowItems;
        rowItems.append(new QStandardItem(data.timestamp.toString("yyyy-MM-dd HH:mm:ss")));
        rowItems.append(new QStandardItem(QString::number(data.temperature, 'f', 1)));
        rowItems.append(new QStandardItem(QString::number(data.humidity, 'f', 1)));
        rowItems.append(new QStandardItem(QString::number(data.lightIntensity, 'f', 0)));
        rowItems.append(new QStandardItem(QString::number(data.tempUpperLimit, 'f', 1)));
        rowItems.append(new QStandardItem(QString::number(data.tempLowerLimit, 'f', 1)));
        rowItems.append(new QStandardItem(QString::number(data.humUpperLimit, 'f', 1)));
        rowItems.append(new QStandardItem(QString::number(data.humLowerLimit, 'f', 1)));
        rowItems.append(new QStandardItem(QString::number(data.lightThreshold, 'f', 0)));
        rowItems.append(new QStandardItem(data.coolingSwitch ? tr("开") : tr("关")));
        rowItems.append(new QStandardItem(data.heatingSwitch ? tr("开") : tr("关")));
        rowItems.append(new QStandardItem(data.humidifyingSwitch ? tr("开") : tr("关")));
        rowItems.append(new QStandardItem(data.dehumidifyingSwitch ? tr("开") : tr("关")));
        rowItems.append(new QStandardItem(data.lightingSwitch ? tr("开") : tr("关")));
        rowItems.append(new QStandardItem(data.alarmSwitch ? tr("开") : tr("关")));

        for (QStandardItem *item : rowItems) {
            item->setEditable(false);
            item->setTextAlignment(Qt::AlignCenter);
        }

        m_tableModel->appendRow(rowItems);
    }

    updateStatistics(dataList);

    // 自动调整列宽
    m_tableView->resizeColumnsToContents();
}

void HistoryDialog::updateStatistics(const QList<SensorData> &data)
{
    if (data.isEmpty()) {
        m_statisticsLabel->setText(tr("统计信息: -"));
        return;
    }

    double tempSum = 0, humSum = 0, lightSum = 0;
    double tempMax = data.first().temperature, tempMin = data.first().temperature;
    double humMax = data.first().humidity, humMin = data.first().humidity;
    double lightMax = data.first().lightIntensity, lightMin = data.first().lightIntensity;

    for (const SensorData &d : data) {
        tempSum += d.temperature;
        humSum += d.humidity;
        lightSum += d.lightIntensity;

        if (d.temperature > tempMax) tempMax = d.temperature;
        if (d.temperature < tempMin) tempMin = d.temperature;
        if (d.humidity > humMax) humMax = d.humidity;
        if (d.humidity < humMin) humMin = d.humidity;
        if (d.lightIntensity > lightMax) lightMax = d.lightIntensity;
        if (d.lightIntensity < lightMin) lightMin = d.lightIntensity;
    }

    int count = data.size();
    QString stats = tr("温度: 平均 %1°C (最高 %2°C / 最低 %3°C) | 湿度: 平均 %4% (最高 %5% / 最低 %6%) | 光照: 平均 %7 Lux (最高 %8 Lux / 最低 %9 Lux)")
        .arg(tempSum / count, 0, 'f', 1)
        .arg(tempMax, 0, 'f', 1)
        .arg(tempMin, 0, 'f', 1)
        .arg(humSum / count, 0, 'f', 1)
        .arg(humMax, 0, 'f', 1)
        .arg(humMin, 0, 'f', 1)
        .arg(lightSum / count, 0, 'f', 0)
        .arg(lightMax, 0, 'f', 0)
        .arg(lightMin, 0, 'f', 0);

    m_statisticsLabel->setText(stats);
}

void HistoryDialog::onExportClicked()
{
    if (m_tableModel->rowCount() == 0) {
        QMessageBox::information(this, tr("导出"), tr("当前没有可导出的数据，请先查询"));
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("导出历史数据"), "sensor_history.csv",
        tr("CSV文件 (*.csv);;所有文件 (*)"));

    if (fileName.isEmpty()) {
        return;
    }

    QDateTime start = m_startTimeEdit->dateTime();
    QDateTime end = m_endTimeEdit->dateTime();
    QList<SensorData> dataList = m_databaseManager->getHistoricalData(start, end);

    // 写出文件放到线程池，避免数据量大时阻塞界面
    m_exportButton->setEnabled(false);

    auto *task = new CsvExportTask(fileName, dataList);
    connect(task, &CsvExportTask::finished, this,
            [this](bool success, const QString &filePath, int rowCount) {
        m_exportButton->setEnabled(true);
        if (success) {
            QMessageBox::information(this, tr("导出成功"),
                tr("已成功导出 %1 条数据到:\n%2").arg(rowCount).arg(filePath));
        } else {
            QMessageBox::warning(this, tr("导出失败"), tr("导出CSV文件时发生错误"));
        }
    }, Qt::QueuedConnection);
    // 对话框提前关闭时任务对象也能自行回收
    connect(task, &CsvExportTask::finished, task, &QObject::deleteLater, Qt::QueuedConnection);

    QThreadPool::globalInstance()->start(task);
}

bool HistoryDialog::exportToCsv(const QString &filePath, const QList<SensorData> &data)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open CSV file for writing:" << filePath;
        return false;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    // BOM for Excel
    stream.setGenerateByteOrderMark(true);

    // 表头
    stream << "时间,温度(°C),湿度(%),光照(Lux),"
           << "温度上限,温度下限,湿度上限,湿度下限,光照阈值,"
           << "制冷,加热,加湿,除湿,补光,报警\n";

    for (const SensorData &d : data) {
        stream << d.timestamp.toString("yyyy-MM-dd HH:mm:ss") << ","
               << d.temperature << ","
               << d.humidity << ","
               << d.lightIntensity << ","
               << d.tempUpperLimit << ","
               << d.tempLowerLimit << ","
               << d.humUpperLimit << ","
               << d.humLowerLimit << ","
               << d.lightThreshold << ","
               << (d.coolingSwitch ? "开" : "关") << ","
               << (d.heatingSwitch ? "开" : "关") << ","
               << (d.humidifyingSwitch ? "开" : "关") << ","
               << (d.dehumidifyingSwitch ? "开" : "关") << ","
               << (d.lightingSwitch ? "开" : "关") << ","
               << (d.alarmSwitch ? "开" : "关") << "\n";
    }

    file.close();
    return true;
}

#include "historydialog.moc"
