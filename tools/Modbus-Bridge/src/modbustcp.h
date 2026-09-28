#ifndef MODBUSTCP_H
#define MODBUSTCP_H

#include <QByteArray>
#include <QList>

namespace modbustcp {
    // 组FC03读保持寄存器请求帧：MBAP头（事务ID2+协议ID2+长度2+单元ID1）+PDU（功能码+起始地址+数量）
    QByteArray buildReadHoldingRequest(quint16 txnId, quint8 unitId, quint16 startReg, quint16 count);
    // 解析FC03应答帧，事务/协议/单元/长度/功能码校验，成功后填入regs
    bool parseReadHoldingResponse(const QByteArray& frame, quint16 expectTxn, quint8 expectUnit, QList<quint16>& regs);
    // 组FC06写单个保持寄存器请求帧
    QByteArray buildWriteSingleRequest(quint16 txnId, quint8 unitId, quint16 reg, quint16 value);
    // 解析FC06应答帧，正常应答是请求帧的原样返回
    bool parseWriteSingleResponse(const QByteArray& frame, quint16 expectTxn, quint8 expectUnit, quint16 expectReg, quint16 expectValue);
}

#endif // MODBUSTCP_H
