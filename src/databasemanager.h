#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QList>
#include <QDateTime>
#include "sensordata.h"

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

    // 查询历史数据（按时间升序）
    QList<SensorData> getHistoricalData(const QDateTime &start, const QDateTime &end);

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