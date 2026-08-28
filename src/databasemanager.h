#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include <QDateTime>
#include "sensordata.h"

// 时间段内传感器数据的聚合统计（由 SQL 的 COUNT/AVG/MAX/MIN 直接算出）
struct SensorStatistics {
    int count = 0;
    double tempAvg = 0, tempMax = 0, tempMin = 0;
    double humAvg = 0, humMax = 0, humMin = 0;
    double lightAvg = 0, lightMax = 0, lightMin = 0;
};

// SQLite 数据库管理
class DatabaseManager : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseManager(QObject *parent = nullptr);
    ~DatabaseManager();

    // dbPath 为空时使用默认路径
    bool initializeDatabase(const QString &dbPath = QString());

    void closeDatabase();

    // 保存传感器数据
    bool saveSensorData(const SensorData &data);

    // 批量保存，返回成功条数
    int saveSensorDataBatch(const QList<SensorData> &dataList);

    // 查询历史数据（按时间升序）；limit=0 表示不分页（导出等全量场景用），分页时配合 offset
    QList<SensorData> getHistoricalData(const QDateTime &start, const QDateTime &end,
                                        int limit = 0, int offset = 0);

    // 统计时间段内的数据（SQL 侧 COUNT/AVG/MAX/MIN，不分页，不受表格分页影响）；
    // 查询失败时 count 为 -1
    SensorStatistics getStatistics(const QDateTime &start, const QDateTime &end);

    // 清理 daysToKeep 天前的旧数据，返回删除条数
    int clearOldData(int daysToKeep = 30);

    QString getDatabaseInfo() const;
    bool isOpen() const;
    QString lastError() const;

    // 备份数据库到指定路径
    bool backupDatabase(const QString &backupPath);

signals:

    void databaseError(const QString &error);
    void dataSaved(int count);
    void dataCleared(int count);

private:
    bool createTables();
    bool createIndices();
    bool checkDatabaseStructure();
    SensorData sensorDataFromQuery(const QSqlQuery &query);

    QSqlDatabase m_database;
    QString m_databasePath;
    mutable QString m_lastError;
};

#endif // DATABASEMANAGER_H