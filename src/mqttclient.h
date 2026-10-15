#ifndef MQTTCLIENT_H
#define MQTTCLIENT_H

#include <QObject>
#include <QString>
#include <QMqttClient>
#include <QMqttSubscription>
#include <QTimer>

// MQTT 客户端封装：连接/订阅/发布，自动重连（指数退避），心跳保活
class MqttClient : public QObject
{
    Q_OBJECT

public:
    explicit MqttClient(QObject *parent = nullptr);
    ~MqttClient();

    // clientId 为空时自动生成
    bool connectToBroker(const QString &host, quint16 port,
                        const QString &username, const QString &password,
                        const QString &clientId = QString());

    void disconnectFromBroker();
    bool subscribe(const QString &topic, quint8 qos = 0);
    void unsubscribe(const QString &topic);
    bool publish(const QString &topic, const QString &message,
                quint8 qos = 1, bool retain = false);


    bool isConnected() const;
    QString host() const;
    quint16 port() const;
    void setAutoReconnect(bool enable, int interval = 5000);
    void setKeepAlive(int seconds);

signals:

    void connected();
    void disconnected();
    void errorOccurred(const QString &error);
    void messageReceived(const QString &topic, const QString &message);
    void connectionStateChanged(QMqttClient::ClientState state);

private slots:
    void onConnected();
    void onDisconnected();
    void onStateChanged(QMqttClient::ClientState state);
    void onErrorChanged(QMqttClient::ClientError error);
    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);
    void attemptReconnect();

private:
    void setupConnections();

    QMqttClient *m_client;
    QTimer *m_reconnectTimer;
    bool m_autoReconnect;
    bool m_userRequestedDisconnect = false;
    int m_reconnectInterval;    // 配置的基础重连间隔（setAutoReconnect 可调）
    int m_reconnectDelay;       // 当前实际重连延迟：指数退避增长，连接成功后复位
    QString m_host;
    quint16 m_port;
    QString m_username;
    QString m_password;
};

#endif // MQTTCLIENT_H