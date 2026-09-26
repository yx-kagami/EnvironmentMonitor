#include <QApplication>
#include <QWidget>

int main(int argc, char* argv[])
{
    QApplication a(argc, argv);
    QWidget w;
    w.setWindowTitle(QStringLiteral("modbus2mqtt"));
    w.resize(500, 400);
    w.show();
    return a.exec();
}

