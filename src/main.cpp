#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <cstdio>

// 日志落盘：qInstallMessageHandler 接管所有 qDebug/qWarning 输出
// 发布版本没有控制台，崩溃排查的第一手资料就是这份日志
static QFile g_logFile;

static void messageHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    // QtMsgType: 0=Debug 1=Warning 2=Critical 3=Fatal 4=Info
    static const char *level[] = {"DEBUG", "WARN ", "ERROR", "FATAL", "INFO "};
    const QString line = QStringLiteral("[%1][%2] %3\n")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")),
             level[type], msg);
    if (g_logFile.isOpen()) {
        g_logFile.write(line.toUtf8());
        g_logFile.flush();          // 每条都落盘：崩溃也不丢最后几条
    }
    fprintf(stderr, "%s", qPrintable(line));   // 控制台输出保留
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("智能环境监控系统");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("SmartEnvironment");
    app.setOrganizationDomain("smartenv.example.com");

    // 安装日志处理器：qDebug/qWarning 自动带时间戳写入 exe 同目录 app.log
    g_logFile.setFileName(QCoreApplication::applicationDirPath() + QStringLiteral("/app.log"));
    g_logFile.open(QIODevice::Append | QIODevice::Text);
    qInstallMessageHandler(messageHandler);

    // 设置全局样式
    app.setStyle(QStyleFactory::create("Fusion"));

    // 加载应用程序样式表
    QFile styleFile(":/styles/style.qss");
    if (styleFile.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(styleFile.readAll());
        app.setStyleSheet(styleSheet);
        styleFile.close();
        qDebug() << "Application style sheet loaded";
    } else {
        qWarning() << "Failed to load style sheet:" << styleFile.errorString();
    }

    // 注册元类型
    qRegisterMetaType<SensorData>("SensorData");

    // 创建主窗口
    MainWindow mainWindow;
    mainWindow.show();

    qDebug() << "Application started successfully";

    return app.exec();
}