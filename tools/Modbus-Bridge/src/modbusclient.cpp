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
    m_writeInFlight = false;
    m_hasPendingWrite = false;
    m_buf.clear();
    emit linkStateChanged(false);
    m_reconnectTimer->start();
}

void ModbusClient::onPollTick()
{
    if(m_busy) return;
    m_sock->write(modbustcp::buildReadHoldingRequest(++m_txn, 1, 0, 14));
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
            break;     //半包，等下次readyRead
        const QByteArray frame = m_buf.left(frameLen);  //截取一帧完整数据
        m_buf.remove(0, frameLen);

        m_respTimer->stop();
        m_busy = false;
        if(m_writeInFlight) {
            m_writeInFlight = false;
            if(modbustcp::parseWriteSingleResponse(frame, m_txn, 1, m_inflightReg, m_inflightVal)){
                ++m_okCnt;
                emit writeDone(m_inflightReg, m_inflightVal);
            }
            else
                ++m_errorCnt;
        } else {
            QList<quint16> regs;
            if(modbustcp::parseReadHoldingResponse(frame, m_txn, 1, regs)) {
                ++m_okCnt;
                m_lastRegs = regs;
                emit registersUpdated(regs);
            } else {
                ++m_errorCnt;
            }
        }
        emit countersChanged(m_okCnt, m_timeoutCnt, m_errorCnt);
    }
    flushPendingWrite();
}

void ModbusClient::onRespTimeout()
{
    m_busy = false;
    m_writeInFlight = false;
    m_buf.clear();
    ++m_timeoutCnt;
    emit countersChanged(m_okCnt, m_timeoutCnt, m_errorCnt);
    flushPendingWrite();
}

void ModbusClient::writeRegister(quint16 reg, quint16 value)
{
    m_pendReg = reg;
    m_pendVal = value;
    m_hasPendingWrite = true;
    flushPendingWrite();
}

void ModbusClient::flushPendingWrite()
{
    if(!m_hasPendingWrite) return;
    if(m_busy || m_sock->state() != QAbstractSocket::ConnectedState){
        if(m_sock->state() != QAbstractSocket::ConnectedState)
            m_hasPendingWrite = false;
        return;
    }
    m_hasPendingWrite = false;
    m_inflightReg = m_pendReg;
    m_inflightVal = m_pendVal;
    m_sock->write(modbustcp::buildWriteSingleRequest(++m_txn, 1, m_inflightReg, m_inflightVal));
    m_writeInFlight = true;
    m_busy = true;
    m_respTimer->start();
}

void ModbusClient::handleCommand(const QString& cmd)
{
    //开关指令对应寄存器8~13
    for(int i = 1; i <= 6; ++i) {
        if(cmd == QStringLiteral("Switch%1ON").arg(i)) {
            writeRegister(7 + i, 1);
            return;
        }
        if(cmd == QStringLiteral("Switch%1OFF").arg(i)) {
            writeRegister(7 + i, 0);
            return;
        }
    }

    struct ThresholdCmd {
        const char* add;
        const char* down;
        int reg;
        int step;
    };
    static const ThresholdCmd thresholdCmds[] = {
        {"TempMaxAdd", "TempMaxDown", 3, 10},
        {"TempMinAdd", "TempMinDown", 4, 10},
        {"HumiMaxAdd", "HumiMaxDown", 5, 10},
        {"HumiMinAdd", "HumiMinDown", 6, 10},
        {"LightAdd", "LightDown", 7, 50},
    };
    for(const auto& t : thresholdCmds) {
        if(cmd == QLatin1String(t.add) || cmd == QLatin1String(t.down)) {
            if(m_lastRegs.size() < 14) return;
            const int cur = m_lastRegs.at(t.reg);
            const int next = cur + (cmd == QLatin1String(t.add) ? t.step : -t.step);
            writeRegister(t.reg, static_cast<quint16>(qBound(0, next, 0xFFFF)));
            return;
        }
    }
    qInfo() << "未识别或模式类命令，忽略：" << cmd;
}
