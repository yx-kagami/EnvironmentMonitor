#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

class QLabel;
class QPushButton;
class ModbusClient;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QPushButton* m_connectBtn = nullptr;
    QLabel* m_tempLabel = nullptr;
    QLabel* m_humiLabel = nullptr;
    QLabel* m_lightLabel = nullptr;
    QLabel* m_okLabel = nullptr;
    QLabel* m_timeoutLabel = nullptr;
    QLabel* m_errLabel = nullptr;
    QLabel* m_tcpStateLabel = nullptr;
    ModbusClient* m_client = nullptr;
};

#endif // MAINWINDOW_H
