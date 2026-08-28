#include "mainwindow.h"
#include <QApplication>
#include <QStyleFactory>
#include <QFile>
#include <QDebug>
#include <QDateTime>
#include <cstdio>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
// 链接依赖见 CMakeLists：if(WIN32) target_link_libraries(... dbghelp)
// （MinGW 用 -ldbghelp；MSVC 才识别 #pragma comment(lib,...)）
#endif

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
    // 降噪：qInfo（周期性的收发/保存记录）只进控制台不进文件；
    // 日志文件只保留 DEBUG 事件与 WARN 以上，保证崩溃现场干净可读
    if (g_logFile.isOpen() && type != QtInfoMsg) {
        g_logFile.write(line.toUtf8());
        g_logFile.flush();          // 每条都落盘：崩溃也不丢最后几条
    }
    fprintf(stderr, "%s", qPrintable(line));   // 控制台输出保留
}

#ifdef Q_OS_WIN
// 崩溃守卫：未处理异常时自动写 minidump
// AddVectoredExceptionHandler 在 first-chance 阶段拦截：x64 Windows 下穿越内核边界的回调
// （如 Qt 定时器消息）里发生异常会直接送 WER，只用 SetUnhandledExceptionFilter 收不到，两道防线并存。
// handler 里只用 Win32 + 栈缓冲，不做堆分配、不调 Qt：崩溃时进程状态（堆/CRT/Qt）已不可信。
static void writeCrashDump(EXCEPTION_POINTERS *ep)
{
    wchar_t exeDir[MAX_PATH];
    GetModuleFileNameW(nullptr, exeDir, MAX_PATH);   // exe 全路径
    wchar_t *slash = wcsrchr(exeDir, L'\\');
    if (slash) *slash = L'\0';                       // 截掉文件名，留目录

    wchar_t dumpPath[MAX_PATH];
    wsprintfW(dumpPath, L"%s\\crash.dmp", exeDir);   // 固定文件名（演示够用；生产可加时间戳）

    HANDLE hFile = CreateFileW(dumpPath, GENERIC_WRITE, 0, nullptr,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{ GetCurrentThreadId(), ep, FALSE };
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hFile,
                          MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(hFile);
    }
}

// 向量化异常处理（first-chance，每个异常都先到这里）：按异常码只抓致命异常
static LONG CALLBACK vectoredCrashHandler(EXCEPTION_POINTERS *ep)
{
    switch (ep->ExceptionRecord->ExceptionCode) {
    case EXCEPTION_ACCESS_VIOLATION:        // 0xC0000005 访问冲突（空指针/野指针/越界）
    case EXCEPTION_STACK_OVERFLOW:          // 0xC00000FD 栈溢出（无限递归）
    case EXCEPTION_ILLEGAL_INSTRUCTION:     // 0xC000001D 非法指令（跳到野地址）
    case EXCEPTION_INT_DIVIDE_BY_ZERO:      // 0xC0000094 整数除零
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;   // C++ throw(0xE06D7363)/断点等可恢复异常不抓
    }
    writeCrashDump(ep);
    return EXCEPTION_CONTINUE_SEARCH;       // 抓完放行，进程按原流程退出
}

// 兜底：非回调路径的未处理异常（两道防线并存）
static LONG WINAPI unhandledCrashHandler(EXCEPTION_POINTERS *ep)
{
    writeCrashDump(ep);
    return EXCEPTION_EXECUTE_HANDLER;   // 写完 dump 后让进程退出
}
#endif // Q_OS_WIN

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef Q_OS_WIN
    // 安装崩溃守卫（尽量早装）：向量化 first-chance 为主，Unhandled 为兜底
    AddVectoredExceptionHandler(1, vectoredCrashHandler);
    SetUnhandledExceptionFilter(unhandledCrashHandler);
#endif

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

    // 创建主窗口
    MainWindow mainWindow;
    mainWindow.show();

    qDebug() << "Application started successfully";

    return app.exec();
}