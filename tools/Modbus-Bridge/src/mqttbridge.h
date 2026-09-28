#ifndef MQTTBRIDGE_H
#define MQTTBRIDGE_H

#include <QObject>
#include <QList>

class QMqttClient;
class QTimer;

class MqttBridge : public QObject
{
    Q_OBJECT
public:
    explicit MqttBridge(QObject* parent = nullptr);
    void start(const QString& host, quint16 port);
    void publishData(const QList<quint16>& regs);

signals:
    void linkStateChanged(bool connected);

private:
    QMqttClient* m_client = nullptr;
    QTimer* m_reconnectTimer = nullptr;
};

#endif // MQTTBRIDGE_H
