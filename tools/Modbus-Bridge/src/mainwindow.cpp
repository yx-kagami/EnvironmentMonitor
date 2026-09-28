#include "mainwindow.h"
#include "modbusclient.h"
#include "mqttbridge.h"
#include <QLabel>
#include <QPushButton>
#include <QFormLayout>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent)
{
    auto* central = new QWidget(this);
    auto* form = new QFormLayout(central);
    m_connectBtn = new QPushButton(QStringLiteral("连接从站"), this);
    m_tempLabel = new QLabel(QStringLiteral("--"), this);
    m_humiLabel = new QLabel(QStringLiteral("--"), this);
    m_lightLabel = new QLabel(QStringLiteral("--"), this);
    m_okLabel = new QLabel(QStringLiteral("0"), this);
    m_timeoutLabel = new QLabel(QStringLiteral("0"), this);
    m_errLabel = new QLabel(QStringLiteral("0"), this);
    m_tcpStateLabel = new QLabel(QStringLiteral("未连接"), this);
    m_mqttStateLabel = new QLabel(QStringLiteral("未连接"), this);

    form->addRow(QStringLiteral("温度："), m_tempLabel);
    form->addRow(QStringLiteral("湿度："), m_humiLabel);
    form->addRow(QStringLiteral("光照ADC："), m_lightLabel);
    form->addRow(QStringLiteral("成功帧："), m_okLabel);
    form->addRow(QStringLiteral("超时："), m_timeoutLabel);
    form->addRow(QStringLiteral("错误帧："), m_errLabel);
    form->addRow(QStringLiteral("TCP状态："), m_tcpStateLabel);
    form->addRow(QStringLiteral("MQTT状态："), m_mqttStateLabel);
    form->addRow(m_connectBtn);
    setCentralWidget(central);
    setWindowTitle(QStringLiteral("Modbus TCP → MQTT桥接demo"));
    resize(500, 350);

    m_client = new ModbusClient(this);
    m_mqtt = new MqttBridge(this);

    connect(m_connectBtn, &QPushButton::clicked, this, [this]{
        m_client->connectToSlave(QStringLiteral("127.0.0.1"), 5020);
        m_mqtt->start(QStringLiteral("127.0.0.1"), 1883);
        m_connectBtn->setEnabled(false);
    });
    connect(m_client, &ModbusClient::registersUpdated, this, [this](const QList<quint16>& regs){
        const double t = regs.value(0) / 10.0;
        const double h = regs.value(1) / 10.0;
        const int l = regs.value(2);
        m_tempLabel->setText(QString::number(t, 'f', 1) + QStringLiteral("℃"));
        m_humiLabel->setText(QString::number(h, 'f', 1) + QStringLiteral("%"));
        m_lightLabel->setText(QString::number(l));
        m_mqtt->publishData(regs);
    });
    connect(m_client, &ModbusClient::countersChanged, this, [this](int ok, int tout, int err){
        m_okLabel->setText(QString::number(ok));
        m_timeoutLabel->setText(QString::number(tout));
        m_errLabel->setText(QString::number(err));
    });
    connect(m_client, &ModbusClient::linkStateChanged, this, [this](bool on){
        m_tcpStateLabel->setText(on ? QStringLiteral("已连接") : QStringLiteral("断开"));
    });
    connect(m_mqtt, &MqttBridge::linkStateChanged, this, [this](bool on){
        m_mqttStateLabel->setText(on ? QStringLiteral("已连接") : QStringLiteral("断开"));
    });
}
