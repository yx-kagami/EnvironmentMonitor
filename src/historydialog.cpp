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
#include <QElapsedTimer>
#include <QThreadPool>

// CSV 导出任务：全量查询 + 纯文件 I/O 都放到线程池执行，结果通过信号排队送回 GUI 线程
//（QObject + QRunnable 双继承模式）
class CsvExportTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    CsvExportTask(QString filePath, QDateTime start, QDateTime end)
        : m_filePath(std::move(filePath))
        , m_start(std::move(start))
        , m_end(std::move(end))
    {
        setAutoDelete(false);   // 生命周期显式管理：finished 投递完后 deleteLater
    }

    void run() override
    {
        // QSqlDatabase 不能跨线程用，连接在使用它的线程里现建
        QSqlDatabase db = DatabaseManager::openThreadConnection();
        if (!db.isOpen()) {
            emit finished(false, m_filePath, 0);
            return;
        }
        QElapsedTimer queryTimer;
        queryTimer.start();
        QList<SensorData> dataList = DatabaseManager::getHistoricalData(db, m_start, m_end);
        qInfo() << "export query took" << queryTimer.elapsed() << "ms,"
                << dataList.size() << "rows";
        const bool ok = HistoryDialog::exportToCsv(m_filePath, dataList);
        emit finished(ok, m_filePath, static_cast<int>(dataList.size()));
    }

signals:
    void finished(bool success, const QString &filePath, int rowCount);

private:
    QString m_filePath;
    QDateTime m_start;
    QDateTime m_end;
};

// 历史查询任务：统计+分页查询放线程池（同 CsvExportTask 模式），finished 回 GUI 填表
class HistoryQueryTask : public QObject, public QRunnable
{
    Q_OBJECT

public:
    HistoryQueryTask(QDateTime start, QDateTime end, int limit, int offset)
        : m_start(std::move(start))
        , m_end(std::move(end))
        , m_limit(limit)
        , m_offset(offset)
    {
        setAutoDelete(false);   // 生命周期显式管理：finished 投递完后 deleteLater
    }

    void run() override
    {
        QElapsedTimer queryTimer;
        queryTimer.start();
        QSqlDatabase db = DatabaseManager::openThreadConnection();
        if (!db.isOpen()) {
            emit finished(false, {}, SensorStatistics{});
            return;
        }
        // 统计先行（覆盖全时段），再 LIMIT/OFFSET 取当前页
        SensorStatistics stats = DatabaseManager::getStatistics(db, m_start, m_end);
        QList<SensorData> pageData =
            DatabaseManager::getHistoricalData(db, m_start, m_end, m_limit, m_offset);
        qInfo() << "page query took" << queryTimer.elapsed() << "ms,"
                << pageData.size() << "rows";
        emit finished(stats.count >= 0, pageData, stats);
    }

signals:
    void finished(bool success, const QList<SensorData> &pageData,
                  const SensorStatistics &stats);

private:
    QDateTime m_start;
    QDateTime m_end;
    int m_limit;
    int m_offset;
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
    , m_prevPageButton(nullptr)
    , m_nextPageButton(nullptr)
    , m_pageLabel(nullptr)
    , m_pageSpinBox(nullptr)
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

    // 统计与分页同一行：左边条数/统计，右边分页控件，避免独占一行挤占表格高度
    QHBoxLayout *statLayout = new QHBoxLayout();
    m_countLabel = new QLabel(tr("数据条数: 0"), this);
    m_statisticsLabel = new QLabel(tr("统计信息: -"), this);
    m_statisticsLabel->setWordWrap(true);

    m_prevPageButton = new QPushButton(tr("上一页"), this);
    m_prevPageButton->setProperty("class", "thresholdButton");
    m_prevPageButton->setFixedHeight(26);
    connect(m_prevPageButton, &QPushButton::clicked, this, &HistoryDialog::onPrevPage);
    m_pageSpinBox = new QSpinBox(this);   // 页码跳转：宽度自适应内容，3 位数也能看全
    m_pageSpinBox->setRange(0, 0);
    // 最小宽度按 4 位数字 + 上下按钮预留，页数到 9999 也不截断
    m_pageSpinBox->setMinimumWidth(
        m_pageSpinBox->fontMetrics().horizontalAdvance(QStringLiteral("9999")) + 32);
    connect(m_pageSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &HistoryDialog::onPageJumped);
    m_pageLabel = new QLabel(tr("/ 0 页"), this);
    m_pageLabel->setAlignment(Qt::AlignCenter);
    m_nextPageButton = new QPushButton(tr("下一页"), this);
    m_nextPageButton->setProperty("class", "thresholdButton");
    m_nextPageButton->setFixedHeight(26);
    connect(m_nextPageButton, &QPushButton::clicked, this, &HistoryDialog::onNextPage);

    statLayout->addWidget(m_countLabel);
    statLayout->addWidget(m_statisticsLabel, 1);
    statLayout->addWidget(m_prevPageButton);
    statLayout->addWidget(m_pageSpinBox);
    statLayout->addWidget(m_pageLabel);
    statLayout->addWidget(m_nextPageButton);
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

    m_currentPage = 0;   // 新查询总是从第一页开始
    loadData(start, end);
}

void HistoryDialog::onPrevPage()
{
    if (m_currentPage > 0) {
        --m_currentPage;
        loadData(m_startTimeEdit->dateTime(), m_endTimeEdit->dateTime());
    }
}

void HistoryDialog::onNextPage()
{
    if ((m_currentPage + 1) * kPageSize < m_totalCount) {
        ++m_currentPage;
        loadData(m_startTimeEdit->dateTime(), m_endTimeEdit->dateTime());
    }
}

void HistoryDialog::loadData(const QDateTime &start, const QDateTime &end)
{
    if (!m_databaseManager || !m_databaseManager->isOpen()) {
        QMessageBox::warning(this, tr("数据库错误"), tr("数据库未打开或不可用"));
        return;
    }

    // 查询在途防重入；旧数据留在表格里，结果回来一次性替换，避免闪烁
    m_queryInFlight = true;
    updateInputEnabled();

    auto *task = new HistoryQueryTask(start, end, kPageSize, m_currentPage * kPageSize);
    connect(task, &HistoryQueryTask::finished, this,
            &HistoryDialog::onQueryFinished, Qt::QueuedConnection);
    // 对话框提前关闭时任务对象也能自行回收
    connect(task, &HistoryQueryTask::finished, task, &QObject::deleteLater, Qt::QueuedConnection);
    QThreadPool::globalInstance()->start(task);
}

void HistoryDialog::onQueryFinished(bool success, const QList<SensorData> &dataList,
                                    const SensorStatistics &stats)
{
    m_queryInFlight = false;
    updateInputEnabled();

    if (!success) {
        QMessageBox::warning(this, tr("数据库错误"), tr("历史数据查询失败，详见日志"));
        return;
    }

    m_tableModel->removeRows(0, m_tableModel->rowCount());

    // 统计覆盖整个时间段，不随翻页变化，避免“当前页的统计”误导
    m_totalCount = qMax(0, stats.count);

    // 异步后总数要查完才知道，页码只能事后钳制；
    // 被压小时按钳制页补查一次（钳制页必然合法，不会循环）
    const int totalPages = (m_totalCount + kPageSize - 1) / kPageSize;
    const int clampedPage = qMin(m_currentPage, qMax(0, totalPages - 1));
    if (clampedPage != m_currentPage) {
        m_currentPage = clampedPage;
        loadData(m_startTimeEdit->dateTime(), m_endTimeEdit->dateTime());
        return;
    }

    m_countLabel->setText(tr("数据条数: %1").arg(m_totalCount));

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

    updateStatistics(stats);
    updatePageControls();

    // 自动调整列宽
    m_tableView->resizeColumnsToContents();
}

void HistoryDialog::updateInputEnabled()
{
    // 查询或导出任一在途即视为忙：查询/翻页/页码跳转/导出入口全部禁用
    const bool busy = m_queryInFlight || m_exportInFlight;
    m_queryButton->setEnabled(!busy);
    m_exportButton->setEnabled(!busy);
    m_pageSpinBox->setEnabled(!busy);
    if (busy) {
        // 翻页按钮使能态平时由 updatePageControls 按页码维护，忙时一律压成禁用
        m_prevPageButton->setEnabled(false);
        m_nextPageButton->setEnabled(false);
    } else {
        updatePageControls();
    }
}

void HistoryDialog::onPageJumped(int page)
{
    // updatePageControls 里 setValue 会递归触发本槽：那边已用 QSignalBlocker 挡住，
    // 这里只需处理用户真实输入
    const int totalPages = (m_totalCount + kPageSize - 1) / kPageSize;
    if (totalPages <= 0) return;
    const int target = qBound(1, page, totalPages) - 1;
    if (target == m_currentPage) return;
    m_currentPage = target;
    loadData(m_startTimeEdit->dateTime(), m_endTimeEdit->dateTime());
}

void HistoryDialog::updatePageControls()
{
    const int totalPages = (m_totalCount + kPageSize - 1) / kPageSize;
    m_pageLabel->setText(tr("/ %1 页").arg(totalPages));
    {
        const QSignalBlocker blocker(m_pageSpinBox);
        m_pageSpinBox->setRange(totalPages == 0 ? 0 : 1, totalPages);
        m_pageSpinBox->setValue(totalPages == 0 ? 0 : m_currentPage + 1);
    }
    m_prevPageButton->setEnabled(m_currentPage > 0);
    m_nextPageButton->setEnabled(m_currentPage + 1 < totalPages);
}

void HistoryDialog::updateStatistics(const SensorStatistics &stats)
{
    if (stats.count <= 0) {
        m_statisticsLabel->setText(tr("统计信息: 该时间段内无数据"));
        return;
    }

    QString text = tr("温度: 平均 %1°C (最高 %2°C / 最低 %3°C) | 湿度: 平均 %4% (最高 %5% / 最低 %6%) | 光照: 平均 %7 Lux (最高 %8 Lux / 最低 %9 Lux)")
        .arg(stats.tempAvg, 0, 'f', 1)
        .arg(stats.tempMax, 0, 'f', 1)
        .arg(stats.tempMin, 0, 'f', 1)
        .arg(stats.humAvg, 0, 'f', 1)
        .arg(stats.humMax, 0, 'f', 1)
        .arg(stats.humMin, 0, 'f', 1)
        .arg(stats.lightAvg, 0, 'f', 0)
        .arg(stats.lightMax, 0, 'f', 0)
        .arg(stats.lightMin, 0, 'f', 0);

    m_statisticsLabel->setText(text);
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
    QElapsedTimer perfTimer;
    perfTimer.start();

    // 全量查询+写文件都进线程池，总耗时（查询+写盘）在 finished 回调里打点
    m_exportInFlight = true;
    updateInputEnabled();

    auto *task = new CsvExportTask(fileName, start, end);
    connect(task, &CsvExportTask::finished, this,
            [this, perfTimer](bool success, const QString &filePath, int rowCount) {
        qInfo() << "export total took" << perfTimer.elapsed() << "ms";
        m_exportInFlight = false;
        updateInputEnabled();
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
