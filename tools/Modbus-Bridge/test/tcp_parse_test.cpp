#include "modbustcp.h"
#include <QtTest>

class TcpParseTest : public QObject
{
    Q_OBJECT
private slots:
    void buildRequest();
    void parseValidResp();
    void parseRejects();
    void writeSingle();
};

void TcpParseTest::buildRequest()
{
    QCOMPARE(modbustcp::buildReadHoldingRequest(1, 1, 0, 3),
             QByteArray::fromHex("000100000006010300000003"));
}

void TcpParseTest::parseValidResp()
{
    //MBAP（事务1，协议0，长度9，单元1）+PDU（功能码03，数据长度06，数据）
    const QByteArray validResp = QByteArray::fromHex("00010000000901030600FD02620800");
    QList<quint16> regs;
    QVERIFY(modbustcp::parseReadHoldingResponse(validResp, 1, 1, regs));
    QCOMPARE(regs.size(), qsizetype(3));
    QCOMPARE(regs[0], quint16(253));
    QCOMPARE(regs[1], quint16(610));
    QCOMPARE(regs[2], quint16(2048));
}

void TcpParseTest::parseRejects()
{
    const QByteArray validResp = QByteArray::fromHex("00010000000901030600FD02620800");
    QList<quint16> regs;
    QVERIFY(!modbustcp::parseReadHoldingResponse(validResp, 2, 1, regs));
    QVERIFY(!modbustcp::parseReadHoldingResponse(validResp, 1, 2, regs));
    QByteArray badLen = validResp;
    badLen[5] = '\x07';
    //长度字段不匹配
    QVERIFY(!modbustcp::parseReadHoldingResponse(badLen, 1, 1, regs));
    //截断
    QVERIFY(!modbustcp::parseReadHoldingResponse(validResp.left(10), 1, 1, regs));
    //异常应答83，异常码02
    QVERIFY(!modbustcp::parseReadHoldingResponse(QByteArray::fromHex("000100000003018302"), 1, 1, regs));
}

void TcpParseTest::writeSingle()
{
    //写入成功时FC06的应答帧=请求帧原样返回
    const QByteArray req = modbustcp::buildWriteSingleRequest(1, 1, 8, 1);
    QCOMPARE(req, QByteArray::fromHex("000100000006010600080001"));
    QVERIFY(modbustcp::parseWriteSingleResponse(req, 1, 1, 8, 1));
    QVERIFY(!modbustcp::parseWriteSingleResponse(req, 2, 1, 8, 1));
    QVERIFY(!modbustcp::parseWriteSingleResponse(req, 1, 1, 8, 0));
    //异常应答83
    QVERIFY(!modbustcp::parseWriteSingleResponse(QByteArray::fromHex("000100000003018602"), 1, 1, 8, 1));
}

QTEST_APPLESS_MAIN(TcpParseTest)
#include "tcp_parse_test.moc"
