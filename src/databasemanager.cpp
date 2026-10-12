#include "databasemanager.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QThread>

QString DatabaseManager::s_databasePath;

static const char insertDataSql[] =
    "INSERT INTO sensor_history ("
    "  timestamp, temperature, humidity, light_intensity,"
    "  temp_upper, temp_lower, hum_upper, hum_lower, light_threshold,"
    "  switch1_state, switch2_state, switch3_state,"
    "  switch4_state, switch5_state, switch6_state"
    ") VALUES ("
    "  :timestamp, :temperature, :humidity, :light_intensity,"
    "  :temp_upper, :temp_lower, :hum_upper, :hum_lower, :light_threshold,"
    "  :switch1_state, :switch2_state, :switch3_state,"
    "  :switch4_state, :switch5_state, :switch6_state"
    ")";

DatabaseManager::DatabaseManager(QObject *parent)
    : QObject(parent)
    , m_database(QSqlDatabase::addDatabase("QSQLITE", "sensor_data_connection"))
{
}

DatabaseManager::~DatabaseManager()
{
    closeDatabase();
    QSqlDatabase::removeDatabase("sensor_data_connection");
}

bool DatabaseManager::initializeDatabase(const QString &dbPath)
{
    if (m_database.isOpen()) {
        m_database.close();
    }

    // 确定数据库文件路径
    if (dbPath.isEmpty()) {
        // 使用应用程序目录下的数据库文件
        QString appDir = QCoreApplication::applicationDirPath();
        QDir dir(appDir);
        if (!dir.exists()) {
            dir.mkpath(".");
        }
        m_databasePath = dir.filePath("sensor_data.db");
    } else {
        m_databasePath = dbPath;
    }

    // 同步静态副本：供 openThreadConnection() 在工作线程里打开同一库文件
    s_databasePath = m_databasePath;

    m_database.setDatabaseName(m_databasePath);

    if (!m_database.open()) {
        m_lastError = m_database.lastError().text();
        qCritical() << "Failed to open database:" << m_lastError;
        emit databaseError(m_lastError);
        return false;
    }

    qDebug() << "Database opened successfully:" << m_databasePath;

    // 启用外键约束
    QSqlQuery query(m_database);
    if (!query.exec("PRAGMA foreign_keys = ON")) {
        m_lastError = query.lastError().text();
        qWarning() << "Failed to enable foreign keys:" << m_lastError;
    }

    // 创建表
    if (!createTables()) {
        closeDatabase();
        return false;
    }

    // 创建索引
    if (!createIndices()) {
        qWarning() << "Failed to create indices, but continuing...";
    }

    // 检查数据库结构
    if (!checkDatabaseStructure()) {
        qWarning() << "Database structure check failed, but continuing...";
    }

    // 行数缓存初始化：COUNT(*) 是全表扫描（SQLite 无行数元数据），只在启动时执行这一次；
    // 之后由 saveSensorDataBatch/clearOldData 记账维护，不再让状态栏每秒刷一次就全表扫一次
    {
        QSqlQuery countQuery(m_database);
        if (countQuery.exec("SELECT COUNT(*) FROM sensor_history") && countQuery.next()) {
            m_recordCount = countQuery.value(0).toInt();
        }
    }

    return true;
}

void DatabaseManager::closeDatabase()
{
    if (m_database.isOpen()) {
        m_database.close();
        qDebug() << "Database closed";
    }
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(m_database);

    // 创建传感器历史数据表
    QString createSensorTable =
        "CREATE TABLE IF NOT EXISTS sensor_history ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  timestamp DATETIME NOT NULL,"
        "  temperature REAL NOT NULL,"
        "  humidity REAL NOT NULL,"
        "  light_intensity REAL NOT NULL,"
        "  temp_upper REAL NOT NULL,"
        "  temp_lower REAL NOT NULL,"
        "  hum_upper REAL NOT NULL,"
        "  hum_lower REAL NOT NULL,"
        "  light_threshold REAL NOT NULL,"
        "  switch1_state INTEGER NOT NULL,"
        "  switch2_state INTEGER NOT NULL,"
        "  switch3_state INTEGER NOT NULL,"
        "  switch4_state INTEGER NOT NULL,"
        "  switch5_state INTEGER NOT NULL,"
        "  switch6_state INTEGER NOT NULL"
        ")";

    if (!query.exec(createSensorTable)) {
        m_lastError = query.lastError().text();
        qCritical() << "Failed to create sensor_history table:" << m_lastError;
        emit databaseError(m_lastError);
        return false;
    }

    // 创建系统配置表
    QString createConfigTable =
        "CREATE TABLE IF NOT EXISTS system_config ("
        "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  config_key TEXT UNIQUE NOT NULL,"
        "  config_value TEXT,"
        "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
        ")";

    if (!query.exec(createConfigTable)) {
        m_lastError = query.lastError().text();
        qWarning() << "Failed to create system_config table:" << m_lastError;
        // 继续执行，配置表不是必需的
    }

    qDebug() << "Database tables created successfully";
    return true;
}

bool DatabaseManager::createIndices()
{
    QSqlQuery query(m_database);

    // 为时间戳创建索引，提高查询性能
    QString createTimestampIndex =
        "CREATE INDEX IF NOT EXISTS idx_sensor_history_timestamp "
        "ON sensor_history(timestamp)";

    if (!query.exec(createTimestampIndex)) {
        m_lastError = query.lastError().text();
        qWarning() << "Failed to create timestamp index:" << m_lastError;
        return false;
    }

    // 为温度创建索引（用于范围查询）
    QString createTemperatureIndex =
        "CREATE INDEX IF NOT EXISTS idx_sensor_history_temperature "
        "ON sensor_history(temperature)";

    if (!query.exec(createTemperatureIndex)) {
        m_lastError = query.lastError().text();
        qWarning() << "Failed to create temperature index:" << m_lastError;
        // 继续执行
    }

    qDebug() << "Database indices created successfully";
    return true;
}

bool DatabaseManager::checkDatabaseStructure()
{
    QSqlQuery query(m_database);
    QStringList requiredTables = {"sensor_history", "system_config"};

    for (const QString &tableName : requiredTables) {
        query.prepare("SELECT name FROM sqlite_master WHERE type='table' AND name=:name");
        query.bindValue(":name", tableName);

        if (!query.exec()) {
            m_lastError = query.lastError().text();
            qWarning() << "Failed to check table existence:" << m_lastError;
            return false;
        }

        if (!query.next()) {
            qWarning() << "Required table not found:" << tableName;
            return false;
        }
    }

    qDebug() << "Database structure check passed";
    return true;
}

bool DatabaseManager::insertRow(QSqlQuery &query, const SensorData &data)
{
    query.bindValue(":timestamp", data.timestamp);
    query.bindValue(":temperature", data.temperature);
    query.bindValue(":humidity", data.humidity);
    query.bindValue(":light_intensity", data.lightIntensity);
    query.bindValue(":temp_upper", data.tempUpperLimit);
    query.bindValue(":temp_lower", data.tempLowerLimit);
    query.bindValue(":hum_upper", data.humUpperLimit);
    query.bindValue(":hum_lower", data.humLowerLimit);
    query.bindValue(":light_threshold", data.lightThreshold);
    query.bindValue(":switch1_state", data.coolingSwitch ? 1 : 0);
    query.bindValue(":switch2_state", data.heatingSwitch ? 1 : 0);
    query.bindValue(":switch3_state", data.humidifyingSwitch ? 1 : 0);
    query.bindValue(":switch4_state", data.dehumidifyingSwitch ? 1 : 0);
    query.bindValue(":switch5_state", data.lightingSwitch ? 1 : 0);
    query.bindValue(":switch6_state", data.alarmSwitch ? 1 : 0);
    return query.exec();
}

int DatabaseManager::saveSensorDataBatch(const QList<SensorData> &dataList)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open";
        return 0;
    }

    if (dataList.isEmpty()) {
        return 0;
    }

    // 开始事务
    if (!m_database.transaction()) {
        m_lastError = "Failed to start transaction";
        qWarning() << m_lastError << m_database.lastError().text();
        return 0;
    }

    QSqlQuery query(m_database);
    query.prepare(QLatin1String(insertDataSql));

    int savedCount = 0;
    int skippedCount = 0;
    for (const SensorData &data : dataList) {
        // 单条无效或写失败就跳过，防止一条坏数据导致整体不停重试
        if (!data.isValid() || !insertRow(query, data)) {
            skippedCount++;
            continue;
        } else {
            savedCount++;
        }
    }

    // 提交事务
    if (!m_database.commit()) {
        m_lastError = "Failed to commit transaction";
        qWarning() << m_lastError << m_database.lastError().text();
        m_database.rollback();
        return 0;
    }

    // 记账放 commit 之后：事务回滚的路径不会污染计数；记账统一收口在这个批量入口
    m_recordCount += savedCount;
    if (savedCount > 0) {
        emit dataSaved(savedCount);
    }
    if (skippedCount > 0) {
        qWarning() << "Batch save skipped" << skippedCount << "invalid/failed records";
        emit databaseError(tr("有 %1 条数据写入失败，已跳过").arg(skippedCount));
    }
    return savedCount;
}

QSqlDatabase DatabaseManager::openThreadConnection()
{
    // 连接名按线程 ID 区分（QSqlDatabase 不能跨线程用），同线程复用已有连接
    const QString connectionName =
        QStringLiteral("sensor_data_connection_worker_%1")
            .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));

    QSqlDatabase db;
    if (QSqlDatabase::contains(connectionName)) {
        db = QSqlDatabase::database(connectionName);
        if (db.isOpen()) {
            return db;
        }
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE", connectionName);
        db.setDatabaseName(s_databasePath);
    }

    if (!db.open()) {
        qWarning() << "Failed to open worker database connection:"
                   << db.lastError().text();
        return db;
    }

    // busy_timeout：GUI 攒批写入持锁时，读侧等待重试而不是直接吃 SQLITE_BUSY
    QSqlQuery pragmaQuery(db);
    if (!pragmaQuery.exec(QStringLiteral("PRAGMA busy_timeout = 3000"))) {
        qWarning() << "Failed to set busy_timeout on worker connection:"
                   << pragmaQuery.lastError().text();
    }
    return db;
}

QList<SensorData> DatabaseManager::getHistoricalData(const QDateTime &start, const QDateTime &end,
                                                     int limit, int offset)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open";
        return {};
    }
    return getHistoricalData(m_database, start, end, limit, offset);
}

QList<SensorData> DatabaseManager::getHistoricalData(const QSqlDatabase &db,
                                                     const QDateTime &start, const QDateTime &end,
                                                     int limit, int offset)
{
    QList<SensorData> result;

    if (!db.isOpen()) {
        // 不写 m_lastError：静态版会被工作线程调用，跨线程写成员是竞争
        qWarning() << "getHistoricalData: database connection is not open";
        return result;
    }

    QString sql =
        "SELECT * FROM sensor_history "
        "WHERE timestamp BETWEEN :start AND :end "
        "ORDER BY timestamp ASC";
    if (limit > 0) {
        // 分页：timestamp 索引保证 ORDER BY + LIMIT/OFFSET 走索引，小数据量下代价可忽略
        sql += " LIMIT :limit OFFSET :offset";
    }

    QSqlQuery query(db);
    query.prepare(sql);

    query.bindValue(":start", start);
    query.bindValue(":end", end);
    if (limit > 0) {
        query.bindValue(":limit", limit);
        query.bindValue(":offset", offset);
    }

    if (!query.exec()) {
        qWarning() << "Failed to query historical data:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        SensorData data = sensorDataFromQuery(query);
        if (data.isValid()) {
            result.append(data);
        }
    }
    qInfo() << "Retrieved" << result.size() << "historical data records";
    return result;
}

SensorStatistics DatabaseManager::getStatistics(const QDateTime &start, const QDateTime &end)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open";
        SensorStatistics stats;
        stats.count = -1;
        return stats;
    }
    return getStatistics(m_database, start, end);
}

SensorStatistics DatabaseManager::getStatistics(const QSqlDatabase &db,
                                                const QDateTime &start, const QDateTime &end)
{
    SensorStatistics stats;

    if (!db.isOpen()) {
        // 不写 m_lastError：静态版会被工作线程调用，跨线程写成员是竞争
        qWarning() << "getStatistics: database connection is not open";
        stats.count = -1;
        return stats;
    }

    // 聚合全部下推到 SQLite 执行：只回传 1 行 10 列，不随时间段数据量膨胀
    QSqlQuery query(db);
    query.prepare(
        "SELECT COUNT(*), "
        "AVG(temperature), MAX(temperature), MIN(temperature), "
        "AVG(humidity), MAX(humidity), MIN(humidity), "
        "AVG(light_intensity), MAX(light_intensity), MIN(light_intensity) "
        "FROM sensor_history "
        "WHERE timestamp BETWEEN :start AND :end"
    );
    query.bindValue(":start", start);
    query.bindValue(":end", end);

    if (!query.exec() || !query.next()) {
        qWarning() << "Failed to query statistics:" << query.lastError().text();
        stats.count = -1;
        return stats;
    }

    stats.count = query.value(0).toInt();
    if (stats.count > 0) {
        stats.tempAvg = query.value(1).toDouble();
        stats.tempMax = query.value(2).toDouble();
        stats.tempMin = query.value(3).toDouble();
        stats.humAvg = query.value(4).toDouble();
        stats.humMax = query.value(5).toDouble();
        stats.humMin = query.value(6).toDouble();
        stats.lightAvg = query.value(7).toDouble();
        stats.lightMax = query.value(8).toDouble();
        stats.lightMin = query.value(9).toDouble();
    }
    return stats;
}

int DatabaseManager::clearOldData(int daysToKeep)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open";
        return 0;
    }

    QDateTime cutoff = QDateTime::currentDateTime().addDays(-daysToKeep);

    QSqlQuery query(m_database);
    query.prepare(
        "DELETE FROM sensor_history WHERE timestamp < :cutoff"
    );

    query.bindValue(":cutoff", cutoff);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        qWarning() << "Failed to clear old data:" << m_lastError;
        return 0;
    }

    int deletedCount = query.numRowsAffected();
    qDebug() << "Cleared" << deletedCount << "old data records (older than" << daysToKeep << "days)";

    // 只有真正删了数据才 VACUUM：VACUUM 要全库重建，成本与库大小成正比，
    // 无条件执行会让每次启动都白白付出一次全库拷贝的代价
    if (deletedCount > 0) {
        query.exec("VACUUM");
    }

    // 行数缓存同步减账（钳到 0：外部动过库导致记账偏高时也不至于显示负数）
    m_recordCount = qMax(0, m_recordCount - deletedCount);

    emit dataCleared(deletedCount);
    return deletedCount;
}

QString DatabaseManager::getDatabaseInfo() const
{
    if (!m_database.isOpen()) {
        return "Database is not open";
    }

    QSqlQuery query(m_database);
    QStringList info;

    // 行数走内存缓存（启动时 COUNT(*) 一次，之后随增删记账）：COUNT(*) 是全表扫描，状态栏每秒调本函数，
    // 若每次都现查，百万行库上每秒把 GUI 线程堵住几十~几百毫秒，表现为拖窗口每秒卡一下
    info.append(QString("Total records: %1").arg(m_recordCount));

    // 获取最早和最晚记录时间。MIN/MAX 拆开各查一次：SQLite 的 minmax 索引优化只对单个聚合生效，
    // 合在一句里会退化成全表扫描，百万行库上每秒一次的状态栏刷新又会卡住 GUI 线程
    QString minStr, maxStr;
    if (query.exec("SELECT MIN(timestamp) FROM sensor_history") && query.next()) {
        minStr = query.value(0).toDateTime().toString("yyyy-MM-dd HH:mm");
    }
    if (query.exec("SELECT MAX(timestamp) FROM sensor_history") && query.next()) {
        maxStr = query.value(0).toDateTime().toString("yyyy-MM-dd HH:mm");
    }
    if (!minStr.isEmpty() && !maxStr.isEmpty()) {
        info.append(QString("Time range: %1 to %2").arg(minStr).arg(maxStr));
    }

    // 获取数据库文件大小
    QFileInfo fileInfo(m_databasePath);
    if (fileInfo.exists()) {
        double sizeMB = fileInfo.size() / (1024.0 * 1024.0);
        // arg() 不认 printf 风格的 %.2f；浮点格式化要用 arg(值, 域宽, 进制, 精度)
        info.append(QString("File size: %1 MB").arg(sizeMB, 0, 'f', 2));
    }

    return info.join(" | ");
}

bool DatabaseManager::isOpen() const
{
    return m_database.isOpen();
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

bool DatabaseManager::backupDatabase(const QString &backupPath)
{
    if (!m_database.isOpen()) {
        m_lastError = "Database is not open";
        return false;
    }

    // 关闭当前数据库连接
    QString originalPath = m_databasePath;
    closeDatabase();

    // 复制数据库文件
    if (QFile::exists(backupPath)) {
        if (!QFile::remove(backupPath)) {
            m_lastError = "Failed to remove existing backup file";
            return false;
        }
    }

    if (!QFile::copy(originalPath, backupPath)) {
        m_lastError = "Failed to copy database file";
        // 尝试重新打开数据库
        initializeDatabase(originalPath);
        return false;
    }

    qDebug() << "Database backed up to:" << backupPath;

    // 重新打开数据库
    return initializeDatabase(originalPath);
}

SensorData DatabaseManager::sensorDataFromQuery(const QSqlQuery &query)
{
    SensorData data;

    data.timestamp = query.value("timestamp").toDateTime();
    data.temperature = query.value("temperature").toDouble();
    data.humidity = query.value("humidity").toDouble();
    data.lightIntensity = query.value("light_intensity").toDouble();
    data.tempUpperLimit = query.value("temp_upper").toDouble();
    data.tempLowerLimit = query.value("temp_lower").toDouble();
    data.humUpperLimit = query.value("hum_upper").toDouble();
    data.humLowerLimit = query.value("hum_lower").toDouble();
    data.lightThreshold = query.value("light_threshold").toDouble();
    data.coolingSwitch = query.value("switch1_state").toInt() == 1;
    data.heatingSwitch = query.value("switch2_state").toInt() == 1;
    data.humidifyingSwitch = query.value("switch3_state").toInt() == 1;
    data.dehumidifyingSwitch = query.value("switch4_state").toInt() == 1;
    data.lightingSwitch = query.value("switch5_state").toInt() == 1;
    data.alarmSwitch = query.value("switch6_state").toInt() == 1;

    return data;
}