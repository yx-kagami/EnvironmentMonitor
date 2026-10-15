#include "sensordata.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QDateTime>
#include <cmath>

DataParser::DataParser(QObject *parent)
    : QObject(parent)
{
}

SensorData DataParser::parseJsonData(const QString &jsonString)
{
    SensorData data;
    // 先置为无效时间戳：任何校验/解析失败都会让 isValid() 返回 false，
    // 避免坏报文以全零值伪装成好数据入库
    data.timestamp = QDateTime();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << parseError.errorString();
        return data;
    }

    if (!doc.isObject()) {
        qWarning() << "JSON document is not an object";
        return data;
    }

    QJsonObject obj = doc.object();

    // 全键校验：缺任一 sensor 字段即整条拒绝（协议规定 14 字段齐全）
    if (!isValidSensorObject(obj)) {
        return data;
    }

    static const char *const keys[14] = {
        "sensor1", "sensor2", "sensor3", "sensor4", "sensor5",
        "sensor6", "sensor7", "sensor8", "sensor9", "sensor10",
        "sensor11", "sensor12", "sensor13", "sensor14"
    };
    double v[14];
    for (int i = 0; i < 14; ++i) {
        if (!parseSensorValue(obj, QLatin1String(keys[i]), &v[i])) {
            qWarning() << "Invalid sensor value for" << keys[i];
            return data;
        }
    }

    // 解析传感器值
    data.temperature = v[0];
    data.humidity = v[1];
    data.lightIntensity = v[2];

    // 解析阈值
    data.tempUpperLimit = v[3];
    data.tempLowerLimit = v[4];
    data.humUpperLimit = v[5];
    data.humLowerLimit = v[6];
    data.lightThreshold = v[7];

    // 解析开关状态
    data.coolingSwitch = (v[8] == 1.0);
    data.heatingSwitch = (v[9] == 1.0);
    data.humidifyingSwitch = (v[10] == 1.0);
    data.dehumidifyingSwitch = (v[11] == 1.0);
    data.lightingSwitch = (v[12] == 1.0);
    data.alarmSwitch = (v[13] == 1.0);

    // 全部字段解析完成，打上接收端时间戳（下位机无 RTC 授时）
    data.timestamp = QDateTime::currentDateTime();

    qDebug() << "Parsed sensor data:" << data.toString();
    return data;
}

bool DataParser::isValidSensorObject(const QJsonObject &obj)
{
    static const QStringList requiredKeys = {
        "sensor1", "sensor2", "sensor3",
        "sensor4", "sensor5", "sensor6", "sensor7", "sensor8",
        "sensor9", "sensor10", "sensor11", "sensor12", "sensor13", "sensor14"
    };

    for (const QString &key : requiredKeys) {
        if (!obj.contains(key)) {
            qWarning() << "Missing required key in JSON:" << key;
            return false;
        }
    }

    return true;
}

bool DataParser::parseSensorValue(const QJsonObject &obj, const QString &key, double *out)
{
    if (!out || !obj.contains(key)) {
        qWarning() << "JSON missing key:" << key;
        return false;
    }

    QJsonValue value = obj.value(key);
    double result = 0.0;
    if (value.isDouble()) {
        result = value.toDouble();
    } else if (value.isString()) {
        bool ok = false;
        result = value.toString().toDouble(&ok);
        if (!ok) {
            return false;
        }
    } else if (value.isBool()) {
        result = value.toBool() ? 1.0 : 0.0;
    } else {
        return false;
    }

    if (!std::isfinite(result)) {
        return false;
    }

    *out = result;
    return true;
}

QString DataParser::createControlCommand(const QString &command)
{
    // 纯命令字符串格式，与Android应用完全兼容
    // STM32固件使用strstr查找命令字符串
    return command;
}

QString DataParser::createSwitchCommand(int switchId, bool state)
{
    QString command;
    switch (switchId) {
    case 1:
        command = state ? "Switch1ON" : "Switch1OFF";
        break;
    case 2:
        command = state ? "Switch2ON" : "Switch2OFF";
        break;
    case 3:
        command = state ? "Switch3ON" : "Switch3OFF";
        break;
    case 4:
        command = state ? "Switch4ON" : "Switch4OFF";
        break;
    case 5:
        command = state ? "Switch5ON" : "Switch5OFF";
        break;
    case 6:
        command = state ? "Switch6ON" : "Switch6OFF";
        break;
    default:
        qWarning() << "Invalid switch ID:" << switchId;
        return QString();
    }

    return createControlCommand(command);
}

QString DataParser::createThresholdCommand(const QString &param, bool increase)
{
    QString command;
    if (param == "TempMax") {
        command = increase ? "TempMaxAdd" : "TempMaxDown";
    } else if (param == "TempMin") {
        command = increase ? "TempMinAdd" : "TempMinDown";
    } else if (param == "HumiMax") {
        command = increase ? "HumiMaxAdd" : "HumiMaxDown";
    } else if (param == "HumiMin") {
        command = increase ? "HumiMinAdd" : "HumiMinDown";
    } else if (param == "Light") {
        command = increase ? "LightAdd" : "LightDown";
    } else {
        qWarning() << "Invalid threshold parameter:" << param;
        return QString();
    }

    return createControlCommand(command);
}

QString DataParser::createModeCommand(bool manualMode)
{
    QString command = manualMode ? "Manual" : "Automatic";
    return createControlCommand(command);
}

QString DataParser::createThresholdModeCommand(bool enabled)
{
    QString command = enabled ? "ThresholdON" : "ThresholdOFF";
    return createControlCommand(command);
}

bool DataParser::validateJsonData(const QString &jsonString)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(jsonString.toUtf8(), &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    return isValidSensorObject(doc.object());
}