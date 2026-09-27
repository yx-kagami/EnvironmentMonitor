#include "modbusclient.h"
#include "modbustcp.h"
#include <QTcpSocket>
#include <QTimer>

ModbusClient::ModbusClient(QObject* parent) : QObject(parent)
{
    m_sock = new QTcpSocket(this);
    connect(m_sock, &QTcpSocket::connected, this, &ModbusClient::onConnected);
    connect(m_sock, &QTcpSocket::disconnected, this, &ModbusClient::onDisconnected);
    connect(m_sock, &QTcpSocket::readyRead, this, &ModbusClient::onReadyRead);

    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &ModbusClient::onPollTick);

    m_respTimer = new QTimer(this);
    m_respTimer->setSingleShot(true);
    m_respTimer->setInterval(500);
    connect(m_respTimer, &QTimer::timeout, this, &ModbusClient::onRespTimeout);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setInterval(3000);
    connect(m_reconnectTimer, &QTimer::timeout, [this]{
        if(m_sock->state() == QAbstractSocket::UnconnectedState)
            m_sock->connectToHost(m_host, m_port);
    });
}

void ModbusClient::connectToSlave(const QString& host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_sock->connectToHost(host, port);
    m_reconnectTimer->start();
}

void ModbusClient::onConnected()
{
    emit linkStateChanged(true);
    m_pollTimer->start();
    onPollTick();
    m_reconnectTimer->stop();
}

void ModbusClient::onDisconnected()
{
    m_pollTimer->stop();
    m_respTimer->stop();
    m_busy = false;
    m_buf.clear();
    emit linkStateChanged(false);
    m_reconnectTimer->start();
}

void ModbusClient::onPollTick()
{
    if(m_busy) return;
    m_sock->write(modbustcp::buildReadHoldingRequest(++m_txn, 1, 0, 3));
    m_busy = true;
    m_respTimer->start();
}

void ModbusClient::onReadyRead()
{
    m_buf += m_sock->readAll();
    while(m_buf.size() >= 7) {
        const auto* p = reinterpret_cast<const quint8*>(m_buf.constData());
        const int frameLen = 6 + ((p[4] << 8) | p[5]);  //长度=单元ID+PDU
        if(m_buf.size() < frameLen)
            return;     //半包，等下次readyRead
        const QByteArray frame = m_buf.left(frameLen);  //截取一帧完整数据
        m_buf.remove(0, frameLen);

        m_respTimer->stop();
        m_busy = false;
        QList<quint16> regs;
        if(modbustcp::parseReadHoldingResponse(frame, m_txn, 1, regs)) {
            ++m_okCnt;
            emit registersUpdated(regs);
        } else {
            ++m_errorCnt;
        }
        emit countersChanged(m_okCnt, m_timeoutCnt, m_errorCnt);
    }
}

void ModbusClient::onRespTimeout()
{
    m_busy = false;
    m_buf.clear();
    ++m_timeoutCnt;
    emit countersChanged(m_okCnt, m_timeoutCnt, m_errorCnt);
}
