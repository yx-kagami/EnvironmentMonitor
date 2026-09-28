#include "mqttbridge.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMqttClient>
#include <QTimer>
#include <QMqttMessage>
#include <QMqttSubscription>

MqttBridge::MqttBridge(QObject* parent) : QObject(parent)
{
    m_client = new QMqttClient(this);
    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    connect(m_reconnectTimer, &QTimer::timeout, this, [this]{
        if(m_client->state() == QMqttClient::Disconnected)
            m_client->connectToHost();
    });
    connect(m_client, &QMqttClient::stateChanged, this, [this](QMqttClient::ClientState s){
        if(s == QMqttClient::Connected) {
            m_reconnectTimer->stop();
            //下行命令通道，订阅上位机命令主题
            auto* sub = m_client->subscribe(QStringLiteral("pub100010"));
            if(sub) {
                connect(sub, &QMqttSubscription::messageReceived, this, [this](const QMqttMessage& msg){
                    QString cmd = QString::fromUtf8(msg.payload());
                    while(cmd.endsWith(QLatin1Char('\0'))) cmd.chop(1);
                    emit commandReceived(cmd);
                });
            }
        } else {
            m_reconnectTimer->start();
        }
        emit linkStateChanged(s == QMqttClient::Connected);
    });
}

void MqttBridge::start(const QString& host, quint16 port)
{
    m_client->setHostname(host);
    m_client->setPort(port);
    m_client->connectToHost();
}

void MqttBridge::publishData(const QList<quint16>& regs)
{
    if(m_client->state() != QMqttClient::Connected || regs.size() < 14)
        return;     //简化版本，broker不在就丢弃，不做缓存
    QJsonObject obj;
    obj["sensor1"] = regs[0] / 10.0;
    obj["sensor2"] = regs[1] / 10.0;
    obj["sensor3"] = regs[2];
    obj["sensor4"] = regs[3] / 10.0;
    obj["sensor5"] = regs[4] / 10.0;
    obj["sensor6"] = regs[5] / 10.0;
    obj["sensor7"] = regs[6] / 10.0;
    obj["sensor8"] = regs[7];
    for(int i = 8; i < 14; ++i)
        obj[QStringLiteral("sensor%1").arg(i+1)] = regs[i];
    m_client->publish(QStringLiteral("sub100010"), QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

