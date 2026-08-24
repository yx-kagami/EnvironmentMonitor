#ifndef HISTORYDIALOG_H
#define HISTORYDIALOG_H

#include <QDialog>
#include <QDateTimeEdit>
#include <QPushButton>
#include <QTableView>
#include <QStandardItemModel>
#include <QLabel>
#include <QComboBox>
#include "databasemanager.h"

class HistoryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit HistoryDialog(DatabaseManager *dbManager, QWidget *parent = nullptr);
    ~HistoryDialog();

    // 静态工具函数：纯文件 I/O，供后台导出任务（CsvExportTask）直接调用
    static bool exportToCsv(const QString &filePath, const QList<SensorData> &data);

private slots:
    void onQueryClicked();
    void onExportClicked();
    void onQuickRangeChanged(int index);
    void updateStatistics(const QList<SensorData> &data);

private:
    void setupUI();
    void loadData(const QDateTime &start, const QDateTime &end);

    DatabaseManager *m_databaseManager;

    QDateTimeEdit *m_startTimeEdit;
    QDateTimeEdit *m_endTimeEdit;
    QComboBox *m_quickRangeCombo;
    QPushButton *m_queryButton;
    QPushButton *m_exportButton;
    QTableView *m_tableView;
    QStandardItemModel *m_tableModel;
    QLabel *m_statisticsLabel;
    QLabel *m_countLabel;
};

#endif // HISTORYDIALOG_H
