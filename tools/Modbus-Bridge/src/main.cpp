#include <QApplication>
#include <QWidget>
#include "modbusclient.h"

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);

    ModbusClient client;
    QObject::connect(&client, &ModbusClient::registersUpdated,
                     [](const QList<quint16>& regs) {
                         qDebug() << "regs:" << regs;
    });
    QObject::connect(&client, &ModbusClient::countersChanged,
                     [](int ok, int tout, int err) {
        qDebug() << "ok/timeout/err:" << ok << tout << err;
                     });
    QObject::connect(&client, &ModbusClient::linkStateChanged,
                     [](bool on) {
                         qDebug() << "link:" << on;
                     });
    client.connectToSlave(QStringLiteral("127.0.0.1"), 5020);

    return a.exec();
}

