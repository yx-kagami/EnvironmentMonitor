#include "mqttclient.h"
#include <QDebug>
#include <QDateTime>
#include <QRandomGenerator>

MqttClient::MqttClient(QObject *parent)
    : QObject(parent)
    , m_client(new QMqttClient(this))
    , m_reconnectTimer(new QTimer(this))
    , m_autoReconnect(true)
    , m_reconnectInterval(5000)
    , m_reconnectDelay(1000)
    , m_port(1883)
{
    setupConnections();

    // 配置重连定时器
    m_reconnectTimer->setSingleShot(true);
    m_reconnectTimer->setInterval(m_reconnectInterval);

    // 设置默认客户端ID
    QString clientId = QString("QtMonitor_%1").arg(QDateTime::currentMSecsSinceEpoch());
    m_client->setClientId(clientId);
}

MqttClient::~MqttClient()
{
    disconnectFromBroker();
}

void MqttClient::setupConnections()
{
    // 连接QMqttClient信号
    connect(m_client, &QMqttClient::connected, this, &MqttClient::onConnected);
    connect(m_client, &QMqttClient::disconnected, this, &MqttClient::onDisconnected);
    connect(m_client, &QMqttClient::stateChanged, this, &MqttClient::onStateChanged);
    connect(m_client, &QMqttClient::errorChanged, this, &MqttClient::onErrorChanged);
    connect(m_client, &QMqttClient::messageReceived, this, &MqttClient::onMessageReceived);

    // 连接重连定时器
    connect(m_reconnectTimer, &QTimer::timeout, this, &MqttClient::attemptReconnect);
}

bool MqttClient::connectToBroker(const QString &host, quint16 port,
                                 const QString &username, const QString &password,
                                 const QString &clientId)
{
    if (m_client->state() == QMqttClient::Connected) {
        qDebug() << "Already connected to broker";
        return true;
    }

    m_host = host;
    m_port = port;
    m_username = username;
    m_password = password;

    m_client->setHostname(host);
    m_client->setPort(port);
    m_client->setUsername(username);
    m_client->setPassword(password);

    if (!clientId.isEmpty()) {
        m_client->setClientId(clientId);
    }

    m_client->setCleanSession(false);
    m_client->setKeepAlive(20);
    m_client->setProtocolVersion(QMqttClient::MQTT_3_1_1);

    qDebug() << "Connecting to MQTT broker:" << host << ":" << port;
    m_client->connectToHost();

    return true;
}

void MqttClient::disconnectFromBroker()
{
    if (m_client->state() == QMqttClient::Connected) {
        qDebug() << "Disconnecting from MQTT broker";
        m_client->disconnectFromHost();
    }

    m_reconnectTimer->stop();
}

bool MqttClient::subscribe(const QString &topic, quint8 qos)
{
    if (!isConnected()) {
        qWarning() << "Cannot subscribe: not connected to broker";
        return false;
    }

    qInfo() << "=== DEBUG: MqttClient::subscribe() ===";
    qInfo() << "Topic:" << topic;
    qInfo() << "Requested QoS:" << qos;

    auto subscription = m_client->subscribe(topic, qos);
    if (!subscription) {
        qWarning() << "Failed to subscribe to topic:" << topic;
        return false;
    }

    qInfo() << "Subscribed successfully to topic:" << topic;
    qInfo() << "Actual QoS:" << qos;
    return true;
}

void MqttClient::unsubscribe(const QString &topic)
{
    if (isConnected()) {
        m_client->unsubscribe(topic);
        qDebug() << "Unsubscribed from topic:" << topic;
    }
}

bool MqttClient::publish(const QString &topic, const QString &message,
                         quint8 qos, bool retain)
{
    if (!isConnected()) {
        qWarning() << "Cannot publish: not connected to broker";
        return false;
    }

    QByteArray data = message.toUtf8();
    // STM32 端用 strstr 解析命令帧，必须以 '\0' 结尾
    data.append('\0');

    int messageId = m_client->publish(topic, data, qos, retain);

    if (messageId < 0) {
        qWarning() << "Failed to publish message to topic:" << topic;
        return false;
    }

    qInfo() << "Published successfully, message ID:" << messageId;
    return true;
}

bool MqttClient::isConnected() const
{
    return m_client->state() == QMqttClient::Connected;
}

QString MqttClient::host() const
{
    return m_host;
}

quint16 MqttClient::port() const
{
    return m_port;
}

void MqttClient::setAutoReconnect(bool enable, int interval)
{
    m_autoReconnect = enable;
    m_reconnectInterval = interval;
    m_reconnectTimer->setInterval(interval);

    if (!enable) {
        m_reconnectTimer->stop();
    }
}

void MqttClient::setKeepAlive(int seconds)
{
    m_client->setKeepAlive(seconds);
}

void MqttClient::onConnected()
{
    qDebug() << "Connected to MQTT broker";
    m_reconnectTimer->stop();
    m_reconnectDelay = 1000;    // 连接成功，退避延迟复位回初值
    emit connected();
}

void MqttClient::onDisconnected()
{
    qDebug() << "Disconnected from MQTT broker";
    emit disconnected();

    if (m_autoReconnect && m_client->state() != QMqttClient::Connecting) {
        qDebug() << "Will attempt to reconnect in" << m_reconnectDelay << "ms";
        m_reconnectTimer->start(m_reconnectDelay);
    }
}

void MqttClient::onStateChanged(QMqttClient::ClientState state)
{
    qDebug() << "MQTT connection state changed:" << state;
    emit connectionStateChanged(state);
}

void MqttClient::onErrorChanged(QMqttClient::ClientError error)
{
    QString errorStr;
    switch (error) {
    case QMqttClient::NoError:
        errorStr = "No error";
        break;
    case QMqttClient::InvalidProtocolVersion:
        errorStr = "Invalid protocol version";
        break;
    case QMqttClient::IdRejected:
        errorStr = "Client ID rejected";
        break;
    case QMqttClient::ServerUnavailable:
        errorStr = "Server unavailable";
        break;
    case QMqttClient::BadUsernameOrPassword:
        errorStr = "Bad username or password";
        break;
    case QMqttClient::NotAuthorized:
        errorStr = "Not authorized";
        break;
    case QMqttClient::TransportInvalid:
        errorStr = "Transport invalid";
        break;
    case QMqttClient::ProtocolViolation:
        errorStr = "Protocol violation";
        break;
    case QMqttClient::UnknownError:
        errorStr = "Unknown error";
        break;
    case QMqttClient::Mqtt5SpecificError:
        errorStr = "MQTT 5.0 specific error";
        break;
    }

    if (error != QMqttClient::NoError) {
        qWarning() << "MQTT error:" << errorStr;
        emit errorOccurred(errorStr);
    }
}

void MqttClient::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    QString msgString = QString::fromUtf8(message);
    qInfo() << "Received message from topic:" << topic.name() << "Size:" << message.size();

    emit messageReceived(topic.name(), msgString);
}

void MqttClient::attemptReconnect()
{
    if (m_client->state() != QMqttClient::Connected &&
        m_client->state() != QMqttClient::Connecting) {

        qDebug() << "Attempting to reconnect to MQTT broker...";
        m_client->connectToHost();

        // 指数退避：本次尝试后延迟翻倍，封顶60s；
        // 若连接失败且不再触发 disconnected 信号，这里重新武装定时器兜底
        m_reconnectDelay = qMin(m_reconnectDelay * 2, 60000);
        if (m_autoReconnect) {
            // +0~25% 随机抖动：broker 重启后大量客户端退避节奏一致会撞车（惊群），抖动把重连时刻打散
            m_reconnectTimer->start(m_reconnectDelay
                + QRandomGenerator::global()->bounded(m_reconnectDelay / 4 + 1));
        }
    }
}