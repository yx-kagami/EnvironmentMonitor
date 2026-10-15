#ifndef SENSORDATA_H
#define SENSORDATA_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include <QMetaType>

// 传感器数据结构
struct SensorData
{
    // 传感器测量值
    double temperature;
    double humidity;
    double lightIntensity;

    // 阈值设置
    double tempUpperLimit;
    double tempLowerLimit;
    double humUpperLimit;
    double humLowerLimit;
    double lightThreshold;

    // 开关状态 (1=开/ON, 0=关/OFF)
    bool coolingSwitch;
    bool heatingSwitch;
    bool humidifyingSwitch;
    bool dehumidifyingSwitch;
    bool lightingSwitch;
    bool alarmSwitch;


    QDateTime timestamp;


    SensorData()
        : temperature(0.0)
        , humidity(0.0)
        , lightIntensity(0.0)
        , tempUpperLimit(0.0)
        , tempLowerLimit(0.0)
        , humUpperLimit(0.0)
        , humLowerLimit(0.0)
        , lightThreshold(0.0)
        , coolingSwitch(false)
        , heatingSwitch(false)
        , humidifyingSwitch(false)
        , dehumidifyingSwitch(false)
        , lightingSwitch(false)
        , alarmSwitch(false)
        , timestamp(QDateTime::currentDateTime())
    {}

    bool isValid() const {
        return timestamp.isValid() &&
               temperature >= -50 && temperature <= 100 &&
               humidity >= 0 && humidity <= 100 &&
               lightIntensity >= 0;
    }

    int temperatureStatus() const {
        if (temperature > tempUpperLimit) return 1;
        if (temperature < tempLowerLimit) return -1;
        return 0;
    }

    int humidityStatus() const {
        if (humidity > humUpperLimit) return 1;
        if (humidity < humLowerLimit) return -1;
        return 0;
    }

    bool lightInsufficient() const {
        return lightIntensity < lightThreshold;
    }

    QString toString() const {
        return QString("Temperature: %1°C, Humidity: %2%%, Light: %3 Lux")
            .arg(temperature, 0, 'f', 1)
            .arg(humidity, 0, 'f', 1)
            .arg(lightIntensity, 0, 'f', 0);
    }

    // 序列化为与下位机上行报文同构的 JSON 字符串，用于无硬件联调时构造模拟报文
    // 做解析回环自测，或粘贴到 MQTT 客户端手动发布模拟数据
    QString toJsonString() const {
        return QString(
            "{\n"
            "  \"sensor1\": %1,\n"
            "  \"sensor2\": %2,\n"
            "  \"sensor3\": %3,\n"
            "  \"sensor4\": %4,\n"
            "  \"sensor5\": %5,\n"
            "  \"sensor6\": %6,\n"
            "  \"sensor7\": %7,\n"
            "  \"sensor8\": %8,\n"
            "  \"sensor9\": %9,\n"
            "  \"sensor10\": %10,\n"
            "  \"sensor11\": %11,\n"
            "  \"sensor12\": %12,\n"
            "  \"sensor13\": %13,\n"
            "  \"sensor14\": %14\n"
            "}"
        ).arg(temperature, 0, 'f', 1)
         .arg(humidity, 0, 'f', 1)
         .arg(lightIntensity, 0, 'f', 0)
         .arg(tempUpperLimit, 0, 'f', 1)
         .arg(tempLowerLimit, 0, 'f', 1)
         .arg(humUpperLimit, 0, 'f', 1)
         .arg(humLowerLimit, 0, 'f', 1)
         .arg(lightThreshold, 0, 'f', 0)
         .arg(coolingSwitch ? 1 : 0)
         .arg(heatingSwitch ? 1 : 0)
         .arg(humidifyingSwitch ? 1 : 0)
         .arg(dehumidifyingSwitch ? 1 : 0)
         .arg(lightingSwitch ? 1 : 0)
         .arg(alarmSwitch ? 1 : 0);
    }
};

// 注册为元类型，便于信号槽传递
Q_DECLARE_METATYPE(SensorData)

// JSON 数据解析
class DataParser : public QObject
{
    Q_OBJECT

public:
    explicit DataParser(QObject *parent = nullptr);

    // 解析 JSON 字符串为 SensorData
    static SensorData parseJsonData(const QString &jsonString);


    static QString createControlCommand(const QString &command);

    static QString createSwitchCommand(int switchId, bool state);


    static QString createThresholdCommand(const QString &param, bool increase);

    static QString createModeCommand(bool manualMode);
    static QString createThresholdModeCommand(bool enabled);


    static bool validateJsonData(const QString &jsonString);

private:
    // 校验 JSON 对象是否包含全部 14 个必需字段
    static bool isValidSensorObject(const QJsonObject &obj);

    // 从 JSON 对象取值，key 为 sensor1~sensor14
    static bool parseSensorValue(const QJsonObject &obj, const QString &key, double *out);
};

#endif // SENSORDATA_H