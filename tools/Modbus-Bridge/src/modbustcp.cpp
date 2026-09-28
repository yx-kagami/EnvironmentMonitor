#include "modbustcp.h"

namespace modbustcp {

QByteArray buildReadHoldingRequest(quint16 txnId, quint8 unitId, quint16 startReg, quint16 count)
{
    QByteArray f;
    auto putU16 = [&f](quint16 v) {
        f.append(static_cast<char>(v >> 8));
        f.append(static_cast<char>(v & 0xFF));
    };
    putU16(txnId);      //事务ID
    putU16(0);          //协议ID
    putU16(6);          //长度=单元ID（1）+PDU（5）
    f.append(static_cast<char>(unitId));
    //PDU
    f.append(static_cast<char>(0x03));
    putU16(startReg);
    putU16(count);
    return f;
}

bool parseReadHoldingResponse(const QByteArray& frame, quint16 expectTxn, quint8 expectUnit, QList<quint16>& regs)
{
    if(frame.size() < 9) return false;
    const auto* p = reinterpret_cast<const quint8*>(frame.constData());
    const quint16 txn = static_cast<quint16>((p[0] << 8) | p[1]);
    const quint16 proto = static_cast<quint16>((p[2] << 8) | p[3]);
    const quint16 len = static_cast<quint16>((p[4] << 8) | p[5]);
    const quint8 unit = p[6];
    if(txn != expectTxn || proto != 0 || unit != expectUnit)
        return false;
    if(frame.size() != len + 6)     //长度字段不含自身和事务/协议ID
        return false;
    if(p[7] != 0x03)
        return false;
    const int byteCount = p[8];
    if(frame.size() != byteCount + 9)   //响应报文MBAP（7）+功能码（1）+数据长度（1）+数据
        return false;
    regs.clear();
    for(int i = 0; i < byteCount - 1; i += 2) {
        regs.append(static_cast<quint16>((p[9+i] << 8) | p[9+i+1]));
    }
    return true;
}

QByteArray buildWriteSingleRequest(quint16 txnId, quint8 unitId, quint16 reg, quint16 value)
{
    QByteArray f;
    auto putU16 = [&f](quint16 v) {
        f.append(static_cast<char>(v >> 8));
        f.append(static_cast<char>(v & 0xFF));
    };
    putU16(txnId);      //事务ID
    putU16(0);          //协议ID
    putU16(6);          //长度=单元ID（1）+PDU（5）
    f.append(static_cast<char>(unitId));
    //PDU
    f.append(static_cast<char>(0x06));
    putU16(reg);
    putU16(value);
    return f;
}

bool parseWriteSingleResponse(const QByteArray& frame, quint16 expectTxn, quint8 expectUnit, quint16 expectReg, quint16 expectValue)
{
    if(frame.size() != 12) return false;
    const auto* p = reinterpret_cast<const quint8*>(frame.constData());
    const quint16 txn = static_cast<quint16>((p[0] << 8) | p[1]);
    const quint16 proto = static_cast<quint16>((p[2] << 8) | p[3]);
    const quint8 unit = p[6];
    if(txn != expectTxn || proto != 0 || unit != expectUnit)
        return false;
    if(p[7] != 0x06)
        return false;
    const quint16 reg = static_cast<quint16>((p[8] << 8) | p[9]);
    const quint16 val = static_cast<quint16>((p[10] << 8) | p[11]);
    return (reg == expectReg) && (val == expectValue);
}

}