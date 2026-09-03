#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "chartwidget.h"
#include "sensordisplaywidget.h"
#include "switchcontrolwidget.h"
#include "thresholdeditorwidget.h"
#include "logindialog.h"
#include "historydialog.h"

#include <QCloseEvent>
#include <QMessageBox>
#include <QFileDialog>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QAction>
#include <QDateTime>
#include <QStyle>
#include <QApplication>
#include <QDebug>
#include <QRandomGenerator>
#include <QScrollArea>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_mqttClient(nullptr)
    , m_databaseManager(nullptr)
    , m_statusUpdateTimer(new QTimer(this))
    , m_autoSaveTimer(new QTimer(this))
    // 配置存 exe 同目录 config.ini（与 LoginDialog 指向同一文件），换机器随程序拷走即可
    , m_settings(new QSettings(QCoreApplication::applicationDirPath() + "/config.ini",
                               QSettings::IniFormat, this))
    , m_dataReceivedCount(0)
    , m_dataSavedCount(0)
    , m_chartWidget(nullptr)
    , m_sensorDisplayWidget(nullptr)
    , m_switchControlWidget(nullptr)
    , m_thresholdEditorWidget(nullptr)
    , m_isConnected(false)
    , m_autoSaveEnabled(true)
    , m_lightTheme(false)
    , m_mqttPort(1883)
{
    loadSettings();

    bool needLogin = m_subscribeTopic.isEmpty() || m_publishTopic.isEmpty()
                     || m_mqttUsername.isEmpty();
    if (needLogin) {
        LoginDialog dialog(this);
        dialog.setSaveSettings(m_rememberMqttSettings);  // 回显上次“记住设置”的选择，而不是每次强制勾选
        dialog.loadSavedSettings();

        if (dialog.exec() == QDialog::Accepted) {
            m_mqttHost = dialog.host();
            m_mqttPort = dialog.port();
            m_subscribeTopic = dialog.subscribeTopic();
            m_publishTopic = dialog.publishTopic();
            m_mqttUsername = dialog.username();
            m_mqttPassword = dialog.password();
            m_rememberMqttSettings = dialog.saveSettings();

            if (m_rememberMqttSettings) {
                m_settings->setValue("mqtt/subscribe_topic", m_subscribeTopic);
                m_settings->setValue("mqtt/publish_topic", m_publishTopic);
                m_settings->setValue("mqtt/username", m_mqttUsername);
                m_settings->setValue("mqtt/password", m_mqttPassword);
                m_settings->sync();
            }
        } else {
            QApplication::exit(0);
            return;
        }
    }

    ui->setupUi(this);
    setupUI();
    initializeComponents();
    initializeDatabase();
    setupMqttClient();

    m_statusUpdateTimer->setInterval(1000);
    connect(m_statusUpdateTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    m_statusUpdateTimer->start();

    m_autoSaveTimer->setInterval(60000);
    connect(m_autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSaveData);
    if (m_autoSaveEnabled) {
        m_autoSaveTimer->start();
    }

    applyStyleSheet();

    setWindowTitle(tr("智能环境监控系统"));
}

MainWindow::~MainWindow()
{
    saveSettings();
    cleanupDatabase();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isConnected) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, tr("确认退出"),
                                      tr("当前已连接到MQTT服务器，确定要退出吗？"),
                                      QMessageBox::Yes | QMessageBox::No);

        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }

    if (m_databaseManager && m_databaseManager->isOpen()) {
        m_databaseManager->clearOldData(30);
    }

    event->accept();
}

void MainWindow::setupUI()
{
    m_sensorDisplayWidget = new SensorDisplayWidget(this);
    m_switchControlWidget = new SwitchControlWidget(this);
    m_switchControlWidget->setMinimumHeight(240);
    m_thresholdEditorWidget = new ThresholdEditorWidget(this);
    m_thresholdEditorWidget->setMinimumHeight(90);
    m_chartWidget = new ChartWidget(this);

    if (ui->sensorDisplayPlaceholder) {
        QLayout *layout = ui->sensorDisplayPlaceholder->layout();
        if (!layout) {
            layout = new QVBoxLayout(ui->sensorDisplayPlaceholder);
            ui->sensorDisplayPlaceholder->setLayout(layout);
        }
        layout->addWidget(m_sensorDisplayWidget);
    }

    if (ui->switchControlPlaceholder) {
        QLayout *layout = ui->switchControlPlaceholder->layout();
        if (!layout) {
            layout = new QVBoxLayout(ui->switchControlPlaceholder);
            ui->switchControlPlaceholder->setLayout(layout);
        }
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(m_switchControlWidget);
    }

    if (ui->thresholdEditorPlaceholder) {
        QLayout *layout = ui->thresholdEditorPlaceholder->layout();
        if (!layout) {
            layout = new QVBoxLayout(ui->thresholdEditorPlaceholder);
            ui->thresholdEditorPlaceholder->setLayout(layout);
        }
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        layout->addWidget(m_thresholdEditorWidget);
    }

    if (ui->chartWidgetPlaceholder) {
        QLayout *layout = ui->chartWidgetPlaceholder->layout();
        if (!layout) {
            layout = new QVBoxLayout(ui->chartWidgetPlaceholder);
            ui->chartWidgetPlaceholder->setLayout(layout);
        }
        layout->addWidget(m_chartWidget);
    }

    m_chartWidget->setLightTheme(m_lightTheme);

    // 设置主滚动区域样式
    if (ui->mainScrollArea) {
        ui->mainScrollArea->setStyleSheet("QScrollArea { background-color: transparent; border: none; }");
        if (ui->mainScrollArea->widget()) {
            ui->mainScrollArea->widget()->setStyleSheet("background-color: transparent;");
            // 确保内容区最小高度足够大，避免子控件被压缩
            ui->mainScrollArea->widget()->setMinimumHeight(600);
        }
    }

    setupMenuBar();
    setupToolBar();
    setupStatusBar();
}

void MainWindow::setupConnections()
{
    if (m_mqttClient) {
        connect(m_mqttClient, &MqttClient::connected, this, &MainWindow::onMqttConnected);
        connect(m_mqttClient, &MqttClient::disconnected, this, &MainWindow::onMqttDisconnected);
        connect(m_mqttClient, &MqttClient::errorOccurred, this, &MainWindow::onMqttError);
        connect(m_mqttClient, &MqttClient::messageReceived, this, &MainWindow::onMqttMessageReceived);
        connect(m_mqttClient, &MqttClient::connectionStateChanged, this, &MainWindow::onMqttConnectionStateChanged);
    }

    connect(m_switchControlWidget, &SwitchControlWidget::switchToggled,
            this, &MainWindow::onSwitchToggled);
    connect(m_switchControlWidget, &SwitchControlWidget::modeChanged,
            this, &MainWindow::onModeChanged);

    connect(m_thresholdEditorWidget, &ThresholdEditorWidget::thresholdAdjustRequested,
            this, &MainWindow::onThresholdAdjustRequested);
    connect(m_thresholdEditorWidget, &ThresholdEditorWidget::thresholdModeChanged,
            this, &MainWindow::onThresholdModeChanged);

    // 无条件更新的模块走信号槽分发：统一数据模型直接驱动实时显示与曲线
    connect(this, &MainWindow::sensorDataUpdated,
            m_sensorDisplayWidget, &SensorDisplayWidget::updateData);
    connect(this, &MainWindow::sensorDataUpdated,
            m_chartWidget, &ChartWidget::addDataPoint);
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();

    QMenu *fileMenu = menuBar->addMenu(tr("文件(&F)"));
    QAction *exportAction = fileMenu->addAction(tr("导出数据(&E)"));
    exportAction->setShortcut(QKeySequence::Save);
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportClicked);

    fileMenu->addSeparator();

    QAction *exitAction = fileMenu->addAction(tr("退出(&X)"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::onFileExit);

    QMenu *viewMenu = menuBar->addMenu(tr("视图(&V)"));
    QAction *fullscreenAction = viewMenu->addAction(tr("全屏(&F)"));
    fullscreenAction->setShortcut(QKeySequence::FullScreen);
    connect(fullscreenAction, &QAction::triggered, this, &MainWindow::onViewFullscreen);

    viewMenu->addSeparator();

    QAction *historyAction = viewMenu->addAction(tr("历史数据(&H)"));
    historyAction->setShortcut(QKeySequence("Ctrl+H"));
    connect(historyAction, &QAction::triggered, this, &MainWindow::onHistoryDataClicked);

    viewMenu->addSeparator();

    QAction *showSensorAction = viewMenu->addAction(tr("显示传感器面板"));
    showSensorAction->setCheckable(true);
    showSensorAction->setChecked(true);
    connect(showSensorAction, &QAction::toggled, m_sensorDisplayWidget, &QWidget::setVisible);

    QAction *showSwitchAction = viewMenu->addAction(tr("显示开关面板"));
    showSwitchAction->setCheckable(true);
    showSwitchAction->setChecked(true);
    connect(showSwitchAction, &QAction::toggled, m_switchControlWidget, &QWidget::setVisible);

    QAction *showThresholdAction = viewMenu->addAction(tr("显示阈值面板"));
    showThresholdAction->setCheckable(true);
    showThresholdAction->setChecked(true);
    connect(showThresholdAction, &QAction::toggled, m_thresholdEditorWidget, &QWidget::setVisible);

    QAction *showChartAction = viewMenu->addAction(tr("显示图表面板"));
    showChartAction->setCheckable(true);
    showChartAction->setChecked(true);
    connect(showChartAction, &QAction::toggled, m_chartWidget, &QWidget::setVisible);

    viewMenu->addSeparator();

    QAction *themeAction = viewMenu->addAction(tr("浅色模式"));
    themeAction->setCheckable(true);
    themeAction->setChecked(m_lightTheme);
    connect(themeAction, &QAction::toggled, this, &MainWindow::setTheme);

    QMenu *settingsMenu = menuBar->addMenu(tr("设置(&S)"));
    QAction *preferencesAction = settingsMenu->addAction(tr("偏好设置(&P)"));
    preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(preferencesAction, &QAction::triggered, this, &MainWindow::onSettingsPreferences);

    QAction *mqttSettingsAction = settingsMenu->addAction(tr("MQTT设置(&M)"));
    connect(mqttSettingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);

    settingsMenu->addSeparator();

    QAction *clearDataAction = settingsMenu->addAction(tr("清理历史数据(&C)"));
    connect(clearDataAction, &QAction::triggered, this, &MainWindow::onClearDataClicked);

    QMenu *helpMenu = menuBar->addMenu(tr("帮助(&H)"));
    QAction *aboutAction = helpMenu->addAction(tr("关于(&A)"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::onHelpAbout);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolBar = addToolBar(tr("主工具栏"));
    toolBar->setMovable(false);

    QAction *connectAction = toolBar->addAction(tr("连接"));
    connectAction->setToolTip(tr("连接到MQTT服务器"));
    connect(connectAction, &QAction::triggered, this, &MainWindow::onConnectClicked);

    QAction *disconnectAction = toolBar->addAction(tr("断开"));
    disconnectAction->setToolTip(tr("断开MQTT连接"));
    connect(disconnectAction, &QAction::triggered, this, &MainWindow::onDisconnectClicked);

    toolBar->addSeparator();

    QAction *settingsAction = toolBar->addAction(tr("设置"));
    settingsAction->setToolTip(tr("打开设置对话框"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::onSettingsClicked);

    QAction *exportAction = toolBar->addAction(tr("导出"));
    exportAction->setToolTip(tr("导出历史数据"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::onExportClicked);

    toolBar->addSeparator();

    QAction *themeAction = toolBar->addAction(tr("主题"));
    themeAction->setToolTip(tr("切换浅色/深色主题"));
    connect(themeAction, &QAction::triggered, this, &MainWindow::onToggleTheme);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("就绪"));
}

void MainWindow::initializeComponents()
{
    m_sensorDisplayWidget->setTemperature(0.0);
    m_sensorDisplayWidget->setHumidity(0.0);
    m_sensorDisplayWidget->setLightIntensity(0.0);

    for (int id = 1; id <= 6; ++id) {
        m_switchControlWidget->setSwitchState(id, false);
    }

    m_thresholdEditorWidget->setTemperatureLimits(0.0, 0.0);
    m_thresholdEditorWidget->setHumidityLimits(0.0, 0.0);
    m_thresholdEditorWidget->setLightThreshold(0.0);

    m_chartWidget->setTimeRange(1);
}

void MainWindow::initializeDatabase()
{
    m_databaseManager = new DatabaseManager(this);
    if (!m_databaseManager->initializeDatabase()) {
        QMessageBox::warning(this, tr("数据库错误"),
                             tr("无法初始化数据库：%1").arg(m_databaseManager->lastError()));
    } else {
        qDebug() << "Database initialized:" << m_databaseManager->getDatabaseInfo();
    }
}

void MainWindow::cleanupDatabase()
{
    if (m_databaseManager) {
        m_databaseManager->closeDatabase();
        delete m_databaseManager;
        m_databaseManager = nullptr;
    }
}

void MainWindow::setupMqttClient()
{
    m_mqttClient = new MqttClient(this);
    m_mqttClient->setAutoReconnect(true, 5000);
    m_mqttClient->setKeepAlive(20);

    setupConnections();
}

void MainWindow::connectToMqttBroker()
{
    if (!m_mqttClient) {
        return;
    }

    if (m_mqttClient->isConnected()) {
        QMessageBox::information(this, tr("提示"), tr("已经连接到MQTT服务器"));
        return;
    }

    static int clientCounter = 0;
    QString clientId = QString("qt-%1-%2-%3")
                          .arg(QDateTime::currentDateTime().toMSecsSinceEpoch())
                          .arg(++clientCounter)
                          .arg(QRandomGenerator::global()->generate() % 10000);

    qDebug() << "Using client ID:" << clientId;

    m_mqttClient->connectToBroker(m_mqttHost, m_mqttPort,
                                  m_mqttUsername, m_mqttPassword,
                                  clientId);
}

void MainWindow::disconnectFromMqttBroker()
{
    if (m_mqttClient && m_mqttClient->isConnected()) {
        m_mqttClient->disconnectFromBroker();
    }
}

void MainWindow::onMqttConnected()
{
    m_isConnected = true;
    updateConnectionStatus(true);
    statusBar()->showMessage(tr("已连接到MQTT服务器"), 3000);

    if (!m_subscribeTopic.isEmpty()) {
        m_mqttClient->subscribe(m_subscribeTopic, 0);
    }
}

void MainWindow::onMqttDisconnected()
{
    m_isConnected = false;
    updateConnectionStatus(false);
    statusBar()->showMessage(tr("已断开MQTT连接"), 3000);
}

void MainWindow::onMqttError(const QString &error)
{
    // 凭证类错误重连无意义：停掉自动重连并引导检查设置（否则退避循环会反复弹窗）
    if (error.contains(QStringLiteral("username"), Qt::CaseInsensitive) ||
        error.contains(QStringLiteral("authorized"), Qt::CaseInsensitive)) {
        m_mqttClient->setAutoReconnect(false);
        QMessageBox::warning(this, tr("认证失败"),
                             tr("MQTT用户名或密码错误，请在“设置”中检查连接参数。"));
        statusBar()->showMessage(tr("MQTT认证失败，已停止自动重连"), 5000);
        return;
    }
    // 非凭证错误（断网等）：模态弹窗会挡住用户、让人以为卡死；改为日志+状态栏提示，自动重连在后台继续
    qWarning() << "MQTT错误（自动重连中）:" << error;
    statusBar()->showMessage(tr("MQTT连接异常：%1（自动重连中）").arg(error), 5000);
}

void MainWindow::onMqttMessageReceived(const QString &topic, const QString &message)
{
    Q_UNUSED(topic); // 消息内容自包含，topic 形参仅为匹配信号签名
    m_dataReceivedCount++;
    m_lastDataTime = QDateTime::currentDateTime();

    SensorData data = DataParser::parseJsonData(message);
    if (!data.isValid()) {
        qWarning() << "Invalid sensor data received";
        return;
    }

    m_currentData = data;
    m_pendingSave.append(data);

    // 内存水位线：防止数据库不可用时缓冲无限增长（正常每分钟 autoSave 后清空）
    if (m_pendingSave.size() > 1000) {
        m_pendingSave.removeFirst();
    }

    // 无条件更新的模块（实时显示、曲线）走信号槽分发；曲线增量追加由 ChartWidget 自管
    emit sensorDataUpdated(data);
    // 带条件逻辑的模块（自动模式开关回显、阈值显示）由协调器直调
    updateSensorDisplay(data);
    checkAlarms(data);

    ui->dataCountLabel->setText(tr("数据接收: %1").arg(m_dataReceivedCount));
    ui->lastUpdateLabel->setText(tr("最后更新: %1").arg(m_lastDataTime.toString("HH:mm:ss")));
}

void MainWindow::onMqttConnectionStateChanged(QMqttClient::ClientState state)
{
    qDebug() << "MQTT connection state changed:" << state;
}

void MainWindow::onConnectClicked()
{
    connectToMqttBroker();
}

void MainWindow::onDisconnectClicked()
{
    disconnectFromMqttBroker();
}

void MainWindow::onSettingsClicked()
{
    LoginDialog dialog(this);
    dialog.setMandatory(false);   // 设置菜单打开：关闭即取消，不触发退出确认
    dialog.loadSavedSettings();
    dialog.setSaveSettings(m_rememberMqttSettings);  // 回显上次选择，而非强制勾选

    if (dialog.exec() == QDialog::Accepted) {
        QString newSubscribe = dialog.subscribeTopic();
        QString newPublish = dialog.publishTopic();
        m_rememberMqttSettings = dialog.saveSettings();

        // 凭据当次生效；是否落盘取决于“记住设置”勾选（LoginDialog 已按勾选写/清 INI）
        m_mqttUsername = dialog.username();
        m_mqttPassword = dialog.password();

        const QString newHost = dialog.host();
        const quint16 newPort = dialog.port();

        if (newHost != m_mqttHost || newPort != m_mqttPort) {
            // 换了服务器：整个连接作废，主题随成员一起更新，重连后由 onMqttConnected 重新订阅
            m_mqttHost = newHost;
            m_mqttPort = newPort;
            m_subscribeTopic = newSubscribe;
            m_publishTopic = newPublish;

            if (m_mqttClient && m_mqttClient->isConnected()) {
                disconnectFromMqttBroker();
                // disconnect 是异步的，让事件循环跑完这一轮再发起新连接
                QTimer::singleShot(0, this, [this]() { connectToMqttBroker(); });
            }

            QMessageBox::information(this, tr("设置已更新"),
                                     tr("服务器地址已更新为 %1:%2。")
                                     .arg(m_mqttHost).arg(m_mqttPort));
        } else if (newSubscribe != m_subscribeTopic || newPublish != m_publishTopic) {
            const QString oldSubscribe = m_subscribeTopic;
            m_subscribeTopic = newSubscribe;
            m_publishTopic = newPublish;

            // 主题落盘与否同样由 LoginDialog 按勾选处理，这里不再重复写 INI

            if (m_mqttClient && m_mqttClient->isConnected()) {
                // 先退订旧主题：否则旧主题的数据会继续进入消息槽，两个主题的数据混库
                if (!oldSubscribe.isEmpty() && oldSubscribe != newSubscribe) {
                    m_mqttClient->unsubscribe(oldSubscribe);
                }
                if (!newSubscribe.isEmpty()) {
                    m_mqttClient->subscribe(newSubscribe, 0);
                }
            }

            QMessageBox::information(this, tr("设置已更新"),
                                     tr("MQTT主题已更新。\n订阅: %1\n发布: %2")
                                     .arg(m_subscribeTopic).arg(m_publishTopic));
        }
    }
}

void MainWindow::onExportClicked()
{
    if (!m_databaseManager || !m_databaseManager->isOpen()) {
        QMessageBox::warning(this, tr("数据库错误"), tr("数据库未打开，无法导出数据"));
        return;
    }

    HistoryDialog dialog(m_databaseManager, this);
    dialog.exec();
}

void MainWindow::onHistoryDataClicked()
{
    if (!m_databaseManager || !m_databaseManager->isOpen()) {
        QMessageBox::warning(this, tr("数据库错误"), tr("数据库未打开，无法查询历史数据"));
        return;
    }

    HistoryDialog dialog(m_databaseManager, this);
    dialog.exec();
}

void MainWindow::onClearDataClicked()
{
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("清理数据"),
        tr("确定要清理所有历史数据吗？此操作不可恢复。"),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        if (m_databaseManager && m_databaseManager->isOpen()) {
            int cleared = m_databaseManager->clearOldData(0);
            QMessageBox::information(this, tr("清理完成"),
                tr("已清理 %1 条历史数据记录").arg(cleared));
        }
    }
}

void MainWindow::updateSensorDisplay(const SensorData &data)
{
    // 三个测量值的显示已改走 sensorDataUpdated 信号直挂 SensorDisplayWidget::updateData，
    // 这里只处理带条件逻辑的模块

    // 自动模式下界面开关跟随下位机回报状态；手动模式下以用户操作为准
    if (!m_switchControlWidget->isManualMode()) {
        m_switchControlWidget->setSwitchState(1, data.coolingSwitch);
        m_switchControlWidget->setSwitchState(2, data.heatingSwitch);
        m_switchControlWidget->setSwitchState(3, data.humidifyingSwitch);
        m_switchControlWidget->setSwitchState(4, data.dehumidifyingSwitch);
        m_switchControlWidget->setSwitchState(5, data.lightingSwitch);
        m_switchControlWidget->setSwitchState(6, data.alarmSwitch);
    }

    m_thresholdEditorWidget->setTemperatureLimits(data.tempUpperLimit, data.tempLowerLimit);
    m_thresholdEditorWidget->setHumidityLimits(data.humUpperLimit, data.humLowerLimit);
    m_thresholdEditorWidget->setLightThreshold(data.lightThreshold);
}

void MainWindow::updateStatusBar()
{
    QString statusText;

    if (m_isConnected) {
        statusText += tr("● 在线");
    } else {
        statusText += tr("○ 离线");
    }

    statusText += QString(" | 接收: %1").arg(m_dataReceivedCount);

    if (m_lastDataTime.isValid()) {
        QString lastTime = m_lastDataTime.toString("HH:mm:ss");
        statusText += QString(" | 最后更新: %1").arg(lastTime);
    }

    if (m_databaseManager && m_databaseManager->isOpen()) {
        statusText += " | " + m_databaseManager->getDatabaseInfo();
    }

    statusBar()->showMessage(statusText);
}

void MainWindow::autoSaveData()
{
    if (m_databaseManager && m_databaseManager->isOpen() && !m_pendingSave.isEmpty()) {
        int saved = m_databaseManager->saveSensorDataBatch(m_pendingSave);
        if (saved > 0) {
            m_dataSavedCount += saved;
            m_pendingSave.clear();
            qInfo() << "Auto-saved" << saved << "data records";
        }
    }
}

void MainWindow::onSwitchToggled(int switchId, bool state)
{
    QString command = DataParser::createSwitchCommand(switchId, state);
    if (!command.isEmpty() && m_mqttClient && m_mqttClient->isConnected()) {
        if (!m_publishTopic.isEmpty()) {
            m_mqttClient->publish(m_publishTopic, command, 0, false);
            qDebug() << "Sent switch command:" << command;
        }
    }
}

void MainWindow::onThresholdAdjustRequested(const QString &param, bool increase)
{
    qDebug() << "Threshold adjust requested:" << param << (increase ? "增加" : "减少");

    QString command = DataParser::createThresholdCommand(param, increase);
    if (!command.isEmpty() && m_mqttClient && m_mqttClient->isConnected()) {
        if (!m_publishTopic.isEmpty()) {
            m_mqttClient->publish(m_publishTopic, command, 0, false);
            qDebug() << "Sent threshold command:" << command;
        }
    }
}

void MainWindow::onThresholdModeChanged(bool enabled)
{
    qDebug() << "Threshold mode changed:" << (enabled ? "ThresholdON" : "ThresholdOFF");

    QString command = DataParser::createThresholdModeCommand(enabled);
    if (!command.isEmpty() && m_mqttClient && m_mqttClient->isConnected()) {
        if (!m_publishTopic.isEmpty()) {
            m_mqttClient->publish(m_publishTopic, command, 0, false);
            qDebug() << "Sent threshold mode command:" << command;
        }
    }
}

void MainWindow::onModeChanged(bool manualMode)
{
    QString command = DataParser::createModeCommand(manualMode);
    qInfo() << "Generated command:" << command << "Length:" << command.length();

    if (!command.isEmpty() && m_mqttClient && m_mqttClient->isConnected()) {
        if (!m_publishTopic.isEmpty()) {
            qInfo() << "Publishing to topic:" << m_publishTopic << "QoS: 0";
            m_mqttClient->publish(m_publishTopic, command, 0, false);
            qDebug() << "Sent mode command:" << command;
        }
    }
}

void MainWindow::onFileExit()
{
    close();
}

void MainWindow::onViewFullscreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::onSettingsPreferences()
{
    QMessageBox::information(this, tr("偏好设置"), tr("偏好设置功能待实现"));
}

void MainWindow::onHelpAbout()
{
    QMessageBox::about(this, tr("关于智能环境监控系统"),
        tr("<h2>智能环境监控系统</h2>"
           "<p>版本: 1.0.0</p>"
           "<p>基于Qt 6.8.3开发的高经济价值作物智能环境监控软件</p>"
           "<p>功能特点：</p>"
           "<ul>"
           "<li>实时监控温度、湿度、光照强度</li>"
           "<li>6路开关设备远程控制</li>"
           "<li>阈值设置和自动报警</li>"
           "<li>历史数据图表显示</li>"
           "<li>SQLite数据库存储</li>"
           "</ul>"
           "<p>兼容STM32+ESP8266+MQTT硬件平台</p>"));
}

void MainWindow::loadSettings()
{
    m_mqttHost = m_settings->value("mqtt/host", "139.196.24.105").toString();
    m_mqttPort = m_settings->value("mqtt/port", 1883).toUInt();
    m_mqttUsername = m_settings->value("mqtt/username", "").toString();
    m_mqttPassword = m_settings->value("mqtt/password", "").toString();
    m_subscribeTopic = m_settings->value("mqtt/subscribe_topic", "").toString();
    m_publishTopic = m_settings->value("mqtt/publish_topic", "").toString();
    m_autoSaveEnabled = m_settings->value("data/auto_save", true).toBool();
    m_lightTheme = m_settings->value("ui/light_theme", false).toBool();
    m_rememberMqttSettings = m_settings->value("mqtt/remember", true).toBool();
}

void MainWindow::saveSettings()
{
    m_settings->setValue("mqtt/host", m_mqttHost);
    m_settings->setValue("mqtt/port", m_mqttPort);
    // “记住主题设置”未勾选时：只记勾选状态本身，MQTT 参数不落盘并清除旧值
    m_settings->setValue("mqtt/remember", m_rememberMqttSettings);
    if (m_rememberMqttSettings) {
        m_settings->setValue("mqtt/username", m_mqttUsername);
        m_settings->setValue("mqtt/password", m_mqttPassword);
        m_settings->setValue("mqtt/subscribe_topic", m_subscribeTopic);
        m_settings->setValue("mqtt/publish_topic", m_publishTopic);
    } else {
        m_settings->remove("mqtt/username");
        m_settings->remove("mqtt/password");
        m_settings->remove("mqtt/subscribe_topic");
        m_settings->remove("mqtt/publish_topic");
    }
    m_settings->setValue("data/auto_save", m_autoSaveEnabled);
    m_settings->setValue("ui/light_theme", m_lightTheme);
}

void MainWindow::applyStyleSheet()
{
    // 主题文件在 qrc 里的固定路径（resources/styles.qrc 收录了两个 QSS）
    const QString themeFile = m_lightTheme ? ":/styles/light.qss" : ":/styles/style.qss";

    QFile styleFile(themeFile);
    if (!styleFile.open(QFile::ReadOnly)) {
        qWarning() << "Failed to load style sheet:" << themeFile << styleFile.errorString();
        return;
    }

    qApp->setStyleSheet(QLatin1String(styleFile.readAll()));
}

void MainWindow::setTheme(bool light)
{
    if (m_lightTheme == light) {
        return;
    }
    m_lightTheme = light;
    applyStyleSheet();
    m_chartWidget->setLightTheme(light);
    saveSettings();
    qDebug() << "Theme changed to:" << (m_lightTheme ? "Light" : "Dark");
}

void MainWindow::onToggleTheme()
{
    setTheme(!m_lightTheme);
}

void MainWindow::checkAlarms(const SensorData &data)
{
    int tempStatus = data.temperatureStatus();
    if (tempStatus == 1) {
        showNotification(tr("温度过高"),
            tr("当前温度 %1°C 超过上限 %2°C").arg(data.temperature).arg(data.tempUpperLimit));
    } else if (tempStatus == -1) {
        showNotification(tr("温度过低"),
            tr("当前温度 %1°C 低于下限 %2°C").arg(data.temperature).arg(data.tempLowerLimit));
    }

    int humStatus = data.humidityStatus();
    if (humStatus == 1) {
        showNotification(tr("湿度过高"),
            tr("当前湿度 %1% 超过上限 %2%").arg(data.humidity).arg(data.humUpperLimit));
    } else if (humStatus == -1) {
        showNotification(tr("湿度过低"),
            tr("当前湿度 %1% 低于下限 %2%").arg(data.humidity).arg(data.humLowerLimit));
    }

    if (data.lightInsufficient()) {
        showNotification(tr("光照不足"),
            tr("当前光照 %1 Lux 低于阈值 %2 Lux").arg(data.lightIntensity).arg(data.lightThreshold));
    }

    if (data.alarmSwitch) {
        showNotification(tr("报警触发"), tr("报警开关已触发"));
    }
}

void MainWindow::showNotification(const QString &title, const QString &message)
{
    qDebug() << "ALERT:" << title << "-" << message;
    statusBar()->showMessage(tr("报警：%1 - %2").arg(title).arg(message), 5000);
}

void MainWindow::updateConnectionStatus(bool connected)
{
    m_isConnected = connected;

    if (connected) {
        ui->connectionStatusLabel->setText(tr("连接状态: ● 在线"));
        ui->connectionStatusLabel->setProperty("statusType", "online");
    } else {
        ui->connectionStatusLabel->setText(tr("连接状态: ○ 离线"));
        ui->connectionStatusLabel->setProperty("statusType", "offline");
    }
    // statusType 是动态属性，需 unpolish/polish 让QSS选择器重新匹配
    ui->connectionStatusLabel->style()->unpolish(ui->connectionStatusLabel);
    ui->connectionStatusLabel->style()->polish(ui->connectionStatusLabel);
}

