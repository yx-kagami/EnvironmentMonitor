#include <QtTest>
#include "sensordata.h"

// DataParser 单元测试
// DataParser 是网络边界（不可信输入的唯一入口），纯静态函数、零依赖 UI，
// 是测试金字塔中最优先覆盖的一层。运行：build 目录下 ctest --output-on-failure
class TestDataParser : public QObject
{
    Q_OBJECT

private slots:
    // 回环：SensorData -> toJsonString -> parseJsonData，全字段无损还原
    void parseRoundTrip();
    // 坏输入一律判 invalid（数据驱动：一行数据 = 一个用例）
    void parseInvalidInput_data();
    void parseInvalidInput();
    // 数值兼容：字符串型数值 "25.5" 也能解析（下位机可能发字符串）
    void parseStringNumber();
    // 命令构造：与下位机协议字符串一一对应，非法输入返回空串
    void switchCommand();
    void switchCommandInvalidId();
    void thresholdCommand();
    void thresholdCommandInvalidParam();
    void modeCommands();
    void parseRejectsBadValues();
    void parseRejectsOutOfRange();
};

void TestDataParser::parseRoundTrip()
{
    SensorData mock;
    mock.temperature = 25.5;  mock.humidity = 60.5;   mock.lightIntensity = 500;
    mock.tempUpperLimit = 40.0; mock.tempLowerLimit = 10.5;
    mock.humUpperLimit = 80.5;  mock.humLowerLimit = 20.5;
    mock.lightThreshold = 300;
    mock.coolingSwitch = true;      mock.heatingSwitch = false;
    mock.humidifyingSwitch = true;  mock.dehumidifyingSwitch = false;
    mock.lightingSwitch = true;     mock.alarmSwitch = false;

    const SensorData parsed = DataParser::parseJsonData(mock.toJsonString());

    QVERIFY2(parsed.isValid(), "round-trip 结果必须通过有效性校验");
    QCOMPARE(parsed.temperature, 25.5);
    QCOMPARE(parsed.humidity, 60.5);
    QCOMPARE(parsed.lightIntensity, 500.0);
    QCOMPARE(parsed.tempUpperLimit, 40.0);
    QCOMPARE(parsed.tempLowerLimit, 10.5);
    QCOMPARE(parsed.humUpperLimit, 80.5);
    QCOMPARE(parsed.humLowerLimit, 20.5);
    QCOMPARE(parsed.lightThreshold, 300.0);
    QCOMPARE(parsed.coolingSwitch, true);
    QCOMPARE(parsed.heatingSwitch, false);
    QCOMPARE(parsed.humidifyingSwitch, true);
    QCOMPARE(parsed.dehumidifyingSwitch, false);
    QCOMPARE(parsed.lightingSwitch, true);
    QCOMPARE(parsed.alarmSwitch, false);
    QVERIFY(parsed.timestamp.isValid()); // 接收端打时间戳（下位机无 RTC）
}

void TestDataParser::parseInvalidInput_data()
{
    QTest::addColumn<QString>("json");
    QTest::newRow("empty")         << QString();
    QTest::newRow("malformed")     << QString("{sensor1: 25}");
    QTest::newRow("not-object")    << QString("[1,2,3]");
    QTest::newRow("empty-object")  << QString("{}");
    QTest::newRow("missing-field") << QString(R"({"sensor1":25,"sensor2":60})");
}

void TestDataParser::parseInvalidInput()
{
    QFETCH(QString, json);
    // 双保险：validateJsonData 与 parseJsonData 的判定必须一致
    QVERIFY(!DataParser::validateJsonData(json));
    QVERIFY(!DataParser::parseJsonData(json).isValid());
}

void TestDataParser::parseStringNumber()
{
    SensorData mock;
    mock.temperature = 25.5;
    QString json = mock.toJsonString();
    // 把 sensor1 的数值换成字符串形式，模拟固件发字符串数值
    json.replace(QStringLiteral("\"sensor1\": 25.5"),
                 QStringLiteral("\"sensor1\": \"25.5\""));

    const SensorData parsed = DataParser::parseJsonData(json);
    QVERIFY(parsed.isValid());
    QCOMPARE(parsed.temperature, 25.5);
}

void TestDataParser::parseRejectsBadValues()
{
    QString json = "{\"sensor1\":\"abc\",\"sensor2\":60.5,\"sensor3\":500,"
                   "\"sensor4\":30,\"sensor5\":15,\"sensor6\":70,\"sensor7\":40,"
                   "\"sensor8\":500,\"sensor9\":1,\"sensor10\":0,\"sensor11\":1,"
                   "\"sensor12\":0,\"sensor13\":1,\"sensor14\":0}";
    SensorData data = DataParser::parseJsonData(json);
    QVERIFY(!data.isValid());

    json.replace("\"abc\"", "null");
    QVERIFY(!DataParser::parseJsonData(json).isValid());

    json.replace("null", "\"25.5\"");
    SensorData ok = DataParser::parseJsonData(json);
    QVERIFY(ok.isValid());
    QCOMPARE(ok.temperature, 25.5);
}

void TestDataParser::parseRejectsOutOfRange()
{
    const QString json = "{\"sensor1\":999,\"sensor2\":60.5,\"sensor3\":500,"
                         "\"sensor4\":30,\"sensor5\":15,\"sensor6\":70,\"sensor7\":40,"
                         "\"sensor8\":500,\"sensor9\":1,\"sensor10\":0,\"sensor11\":1,"
                         "\"sensor12\":0,\"sensor13\":1,\"sensor14\":0}";
    QVERIFY(!DataParser::parseJsonData(json).isValid());
}

void TestDataParser::switchCommand()
{
    QCOMPARE(DataParser::createSwitchCommand(1, true),  QString("Switch1ON"));
    QCOMPARE(DataParser::createSwitchCommand(1, false), QString("Switch1OFF"));
    QCOMPARE(DataParser::createSwitchCommand(6, true),  QString("Switch6ON"));
    QCOMPARE(DataParser::createSwitchCommand(6, false), QString("Switch6OFF"));
}

void TestDataParser::switchCommandInvalidId()
{
    // 协议只定义 1~6 号开关，越界返回空串而不是乱发命令
    QVERIFY(DataParser::createSwitchCommand(0, true).isEmpty());
    QVERIFY(DataParser::createSwitchCommand(7, false).isEmpty());
}

void TestDataParser::thresholdCommand()
{
    QCOMPARE(DataParser::createThresholdCommand("TempMax", true),  QString("TempMaxAdd"));
    QCOMPARE(DataParser::createThresholdCommand("TempMax", false), QString("TempMaxDown"));
    QCOMPARE(DataParser::createThresholdCommand("HumiMin", true),  QString("HumiMinAdd"));
    QCOMPARE(DataParser::createThresholdCommand("Light", false),   QString("LightDown"));
}

void TestDataParser::thresholdCommandInvalidParam()
{
    QVERIFY(DataParser::createThresholdCommand("BadParam", true).isEmpty());
    QVERIFY(DataParser::createThresholdCommand(QString(), false).isEmpty());
}

void TestDataParser::modeCommands()
{
    QCOMPARE(DataParser::createModeCommand(true),           QString("Manual"));
    QCOMPARE(DataParser::createModeCommand(false),          QString("Automatic"));
    QCOMPARE(DataParser::createThresholdModeCommand(true),  QString("ThresholdON"));
    QCOMPARE(DataParser::createThresholdModeCommand(false), QString("ThresholdOFF"));
}

QTEST_APPLESS_MAIN(TestDataParser)
#include "test_dataparser.moc"
