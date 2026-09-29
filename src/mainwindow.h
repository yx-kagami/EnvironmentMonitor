#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QSettings>
#include "mqttclient.h"
#include "sensordata.h"
#include "databasemanager.h"

// 前向声明
class ChartWidget;
class SensorDisplayWidget;
class SwitchControlWidget;
class ThresholdEditorWidget;

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

// 智能环境监控系统主窗口
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 登录对话框取消时为 true：事件循环未启动时 exit() 无效，由 main() 据此直接退出
    bool isLoginCancelled() const { return m_loginCancelled; }

signals:
    // 统一数据模型的信号槽分发：无条件更新的模块（实时显示、曲线）直接挂这个信号，
    // 带条件逻辑的模块（自动模式开关回显、阈值显示）由协调器直调
    void sensorDataUpdated(const SensorData &data);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    // MQTT相关槽函数
    void onMqttConnected();
    void onMqttDisconnected();
    void onMqttError(const QString &error);
    void onMqttMessageReceived(const QString &topic, const QString &message);
    void onMqttConnectionStateChanged(QMqttClient::ClientState state);

    // 控制按钮槽函数
    void onConnectClicked();
    void onDisconnectClicked();
    void onSettingsClicked();
    void onExportClicked();
    void onHistoryDataClicked();
    void onClearDataClicked();

    // 数据更新槽函数
    void updateSensorDisplay(const SensorData &data);
    void updateStatusBar();
    void autoSaveData();

    // 控制命令槽函数
    void onSwitchToggled(int switchId, bool state);
    void onThresholdAdjustRequested(const QString &param, bool increase);
    void onThresholdModeChanged(bool enabled);
    void onModeChanged(bool manualMode);

    // 菜单栏槽函数
    void onFileExit();
    void onViewFullscreen();
    void onSettingsPreferences();
    void onHelpAbout();
    void onToggleTheme();

private:
    void setupUI();
    void setupConnections();
    void setupMenuBar();
    void setupToolBar();
    void setupStatusBar();
    void loadSettings();
    void saveSettings();
    void applyStyleSheet();
    void setTheme(bool light);
    void initializeComponents();

    // MQTT配置
    void setupMqttClient();
    void connectToMqttBroker();
    void disconnectFromMqttBroker();

    // 数据库管理
    void initializeDatabase();
    void cleanupDatabase();

    // 数据处理
    void checkAlarms(const SensorData &data);
    void showNotification(const QString &title, const QString &message);

    // UI更新
    void updateConnectionStatus(bool connected);

private:
    Ui::MainWindow *ui;

    // 核心组件
    MqttClient *m_mqttClient;
    DatabaseManager *m_databaseManager;
    QTimer *m_statusUpdateTimer;
    QTimer *m_autoSaveTimer;
    QSettings *m_settings;

    // 数据
    SensorData m_currentData;
    QList<SensorData> m_pendingSave; // 待入库缓冲：autoSave 批量写入后清空（曲线数据由 ChartWidget 自管）
    int m_dataReceivedCount;
    int m_dataSavedCount;

    // UI组件
    ChartWidget *m_chartWidget;
    SensorDisplayWidget *m_sensorDisplayWidget;
    SwitchControlWidget *m_switchControlWidget;
    ThresholdEditorWidget *m_thresholdEditorWidget;

    // 状态
    bool m_isConnected;
    bool m_autoSaveEnabled;
    bool m_lightTheme;
    bool m_loginCancelled = false;
    bool m_rememberMqttSettings = true;  // 对应登录框“记住主题设置”：false 时退出不落盘 MQTT 参数
    QDateTime m_lastDataTime;

    // 配置
    QString m_mqttHost;
    quint16 m_mqttPort;
    QString m_mqttUsername;
    QString m_mqttPassword;
    QString m_subscribeTopic;
    QString m_publishTopic;
};

#endif // MAINWINDOW_H