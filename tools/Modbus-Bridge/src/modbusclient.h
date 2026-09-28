#ifndef MODBUSCLIENT_H
#define MODBUSCLIENT_H

#include <QByteArray>
#include <QObject>
#include <QList>

class QTcpSocket;
class QTimer;

class ModbusClient : public QObject
{
    Q_OBJECT
public:
    explicit ModbusClient(QObject* parent = nullptr);
    void connectToSlave(const QString& host, quint16 port);
    void writeRegister(quint16 reg, quint16 value);
    void handleCommand(const QString& cmd);     //MQTT下行命令入口

signals:
    void registersUpdated(const QList<quint16>& regs);
    void countersChanged(int ok, int timeout, int error);
    void linkStateChanged(bool connected);
    void writeDone(quint16 reg, quint16 val);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onPollTick();
    void onRespTimeout();

private:
    void flushPendingWrite();
    QTcpSocket* m_sock = nullptr;
    QTimer* m_pollTimer = nullptr;  //1s轮询
    QTimer* m_respTimer = nullptr;  //单次500ms应答超时
    QTimer* m_reconnectTimer = nullptr;
    QString m_host;
    quint16 m_port = 0;
    QByteArray m_buf;
    quint16 m_txn = 0;
    bool m_busy = false;            //上一帧没答完不重发
    int m_okCnt = 0, m_timeoutCnt = 0, m_errorCnt = 0;
    QList<quint16> m_lastRegs;      //最近一次轮询的14个寄存器
    bool m_writeInFlight = false;   //在途事务是FC06写
    quint16 m_inflightReg = 0, m_inflightVal = 0;   //用于回显对比
    bool m_hasPendingWrite = false; //单在途约束，忙时暂存一条写请求
    quint16 m_pendReg = 0, m_pendVal = 0;
};

#endif // MODBUSCLIENT_H
