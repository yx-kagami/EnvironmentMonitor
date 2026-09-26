#ifndef MODBUSTCP_H
#define MODBUSTCP_H

#include <QByteArray>
#include <QList>

namespace modbustcp {
    // 组FC03读保持寄存器请求帧：MBAP头（事务ID2+协议ID2+长度2+单元ID1）+PDU（功能码+起始地址+数量）
    QByteArray buildReadHoldingRequest(quint16 txnId, quint8 uintId, quint16 startReg, quint16 count);
    bool parseReadHoldingResponse(const QByteArray& frame, quint16 expectTxn, quint8 expectUnit, QList<quint16>& regs);
}

#endif // MODBUSTCP_H
