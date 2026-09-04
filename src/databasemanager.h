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

// 注册为元类型：统计结果要作为信号参数经 QueuedConnection 从工作线程送回 GUI 线程
Q_DECLARE_METATYPE(SensorStatistics)

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

    // 指定连接版，供工作线程调用：db 由 openThreadConnection() 在调用方线程内取得。
    // 静态函数不碰任何成员状态（尤其不写 m_lastError，避免跨线程数据竞争），
    // 失败仅以 qWarning 上报并返回空列表。
    static QList<SensorData> getHistoricalData(const QSqlDatabase &db,
                                               const QDateTime &start, const QDateTime &end,
                                               int limit = 0, int offset = 0);

    // 统计时间段内的数据（SQL 侧 COUNT/AVG/MAX/MIN，不分页，不受表格分页影响）；
    // 查询失败时 count 为 -1
    SensorStatistics getStatistics(const QDateTime &start, const QDateTime &end);

    // 指定连接版，供工作线程调用；静态函数不碰任何成员状态，失败返回 count=-1 的哨兵对象
    static SensorStatistics getStatistics(const QSqlDatabase &db,
                                          const QDateTime &start, const QDateTime &end);

    // 为当前线程打开一条独立连接（连接名按线程 ID 区分，同线程复用）。
    // QSqlDatabase 不能跨线程使用，由工作线程任务的 run() 调用；
    // busy_timeout=3000：GUI 攒批写入持锁时，读侧等待重试而不是直接报 SQLITE_BUSY
    static QSqlDatabase openThreadConnection();

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
    // 行→对象映射只收口在这一处；static 供静态查询重载（工作线程路径）复用
    static SensorData sensorDataFromQuery(const QSqlQuery &query);

    QSqlDatabase m_database;
    QString m_databasePath;
    mutable QString m_lastError;

    // 行数内存缓存：启动时 COUNT(*) 一次，之后随批量入库/清理记账增减；
    // 状态栏每秒刷新走这个值，不再每次全表扫 COUNT(*)
    int m_recordCount = 0;

    // 库路径静态副本：静态方法 openThreadConnection() 用；initializeDatabase() 写入后只读
    static QString s_databasePath;
};

#endif // DATABASEMANAGER_H