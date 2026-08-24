#include "sensordata.h"
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QDateTime>

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

    // 解析传感器值
    data.temperature = parseSensorValue(obj, "sensor1");
    data.humidity = parseSensorValue(obj, "sensor2");
    data.lightIntensity = parseSensorValue(obj, "sensor3");

    // 解析阈值
    data.tempUpperLimit = parseSensorValue(obj, "sensor4");
    data.tempLowerLimit = parseSensorValue(obj, "sensor5");
    data.humUpperLimit = parseSensorValue(obj, "sensor6");
    data.humLowerLimit = parseSensorValue(obj, "sensor7");
    data.lightThreshold = parseSensorValue(obj, "sensor8");

    // 解析开关状态
    data.coolingSwitch = (parseSensorValue(obj, "sensor9") == 1.0);
    data.heatingSwitch = (parseSensorValue(obj, "sensor10") == 1.0);
    data.humidifyingSwitch = (parseSensorValue(obj, "sensor11") == 1.0);
    data.dehumidifyingSwitch = (parseSensorValue(obj, "sensor12") == 1.0);
    data.lightingSwitch = (parseSensorValue(obj, "sensor13") == 1.0);
    data.alarmSwitch = (parseSensorValue(obj, "sensor14") == 1.0);

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

double DataParser::parseSensorValue(const QJsonObject &obj, const QString &key)
{
    if (!obj.contains(key)) {
        qWarning() << "JSON missing key:" << key;
        return 0.0;
    }

    QJsonValue value = obj.value(key);
    if (value.isDouble()) {
        return value.toDouble();
    } else if (value.isString()) {
        bool ok;
        double result = value.toString().toDouble(&ok);
        return ok ? result : 0.0;
    } else if (value.isBool()) {
        return value.toBool() ? 1.0 : 0.0;
    }

    return 0.0;
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