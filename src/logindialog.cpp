#include "logindialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QMessageBox>
#include <QCloseEvent>
#include <QApplication>
#include <QIntValidator>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    // 配置存 exe 同目录 config.ini
    , m_settings(new QSettings(QCoreApplication::applicationDirPath() + "/config.ini",
                               QSettings::IniFormat, this))
    , m_saveSettingsFlag(true)
{
    setWindowTitle(tr("MQTT连接设置"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    setWindowModality(Qt::ApplicationModal);
    setFixedSize(400, 250);

    setupUI();
    setupConnections();
    loadSavedSettings();
}

LoginDialog::~LoginDialog()
{
}

void LoginDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(15);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    // 标题
    m_titleLabel = new QLabel(tr("请输入MQTT主题（从STM32 OLED读取）"), this);
    m_titleLabel->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    m_titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_titleLabel);

    // 表单布局
    QFormLayout *formLayout = new QFormLayout();
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight);

    // 服务器地址
    QLabel *hostLabel = new QLabel(tr("服务器地址:"), this);
    hostLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    m_hostEdit = new QLineEdit(this);
    m_hostEdit->setPlaceholderText(tr("MQTT服务器地址"));
    m_hostEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2d2d2d;"
        "  color: #ffffff;"
        "  border: 1px solid #4a4a4a;"
        "  border-radius: 3px;"
        "  padding: 5px;"
        "}"
    );
    formLayout->addRow(hostLabel, m_hostEdit);

    // 端口：validator 把输入直接圈死在 1..65535，提交时无需再查范围
    QLabel *portLabel = new QLabel(tr("端口:"), this);
    portLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    m_portEdit = new QLineEdit(this);
    m_portEdit->setPlaceholderText(tr("默认 1883"));
    m_portEdit->setValidator(new QIntValidator(1, 65535, this));
    m_portEdit->setStyleSheet(m_hostEdit->styleSheet());
    formLayout->addRow(portLabel, m_portEdit);

    // 订阅主题
    m_subscribeLabel = new QLabel(tr("订阅主题:"), this);
    m_subscribeLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    m_subscribeEdit = new QLineEdit(this);
    m_subscribeEdit->setPlaceholderText(tr("例如：sub100010（接收数据）"));
    m_subscribeEdit->setStyleSheet(
        "QLineEdit {"
        "  background-color: #2d2d2d;"
        "  color: #ffffff;"
        "  border: 1px solid #4a4a4a;"
        "  border-radius: 3px;"
        "  padding: 5px;"
        "}"
    );
    formLayout->addRow(m_subscribeLabel, m_subscribeEdit);

    // 发布主题
    m_publishLabel = new QLabel(tr("发布主题:"), this);
    m_publishLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    m_publishEdit = new QLineEdit(this);
    m_publishEdit->setPlaceholderText(tr("例如：pub100010（发送命令）"));
    m_publishEdit->setStyleSheet(m_subscribeEdit->styleSheet());
    formLayout->addRow(m_publishLabel, m_publishEdit);

    // 用户名
    QLabel *usernameLabel = new QLabel(tr("用户名:"), this);
    usernameLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    m_usernameEdit = new QLineEdit(this);
    m_usernameEdit->setPlaceholderText(tr("MQTT用户名"));
    m_usernameEdit->setStyleSheet(m_subscribeEdit->styleSheet());
    formLayout->addRow(usernameLabel, m_usernameEdit);

    // 密码（凭据仅存本地INI，代码中不留默认值，防止随源码外泄）
    QLabel *passwordLabel = new QLabel(tr("密码:"), this);
    passwordLabel->setStyleSheet("color: #cccccc; font-size: 12px;");
    m_passwordEdit = new QLineEdit(this);
    m_passwordEdit->setPlaceholderText(tr("MQTT密码"));
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setStyleSheet(m_subscribeEdit->styleSheet());
    formLayout->addRow(passwordLabel, m_passwordEdit);

    mainLayout->addLayout(formLayout);

    // 保存设置复选框
    m_saveCheckBox = new QCheckBox(tr("记住主题设置"), this);
    m_saveCheckBox->setChecked(true);
    m_saveCheckBox->setStyleSheet(
        "QCheckBox {"
        "  color: #cccccc;"
        "  font-size: 12px;"
        "}"
        "QCheckBox::indicator {"
        "  width: 16px;"
        "  height: 16px;"
        "}"
    );
    mainLayout->addWidget(m_saveCheckBox);

    // 弹性空间沉底：窗口变高时控件保持聚顶，不会散开
    mainLayout->addStretch();

    // 按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);

    m_cancelButton = new QPushButton(tr("取消"), this);
    m_cancelButton->setFixedSize(80, 30);
    m_cancelButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #4a4a4a;"
        "  color: #ffffff;"
        "  border: 1px solid #5a5a5a;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #5a5a5a;"
        "}"
    );

    m_okButton = new QPushButton(tr("确定"), this);
    m_okButton->setFixedSize(80, 30);
    m_okButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #2d7d9a;"
        "  color: #ffffff;"
        "  border: 1px solid #3d8daa;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover {"
        "  background-color: #3d8daa;"
        "}"
        "QPushButton:disabled {"
        "  background-color: #3a3a3a;"
        "  color: #7a7a7a;"
        "}"
    );
    m_okButton->setDefault(true);

    buttonLayout->addStretch();
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addWidget(m_okButton);

    mainLayout->addLayout(buttonLayout);

    // 设置对话框背景
    setStyleSheet(
        "QDialog {"
        "  background-color: #252525;"
        "  border: 2px solid #3a3a3a;"
        "  border-radius: 8px;"
        "}"
    );

    // 窗口大小强制适配内容：内容增减时窗口自动重算高度，输入框不会被压缩挤在一起
    mainLayout->setSizeConstraint(QLayout::SetFixedSize);

    // 初始验证
    validateInputs();
}

void LoginDialog::setupConnections()
{
    connect(m_okButton, &QPushButton::clicked, this, &LoginDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &LoginDialog::onCancelClicked);
    connect(m_hostEdit, &QLineEdit::textChanged, this, &LoginDialog::validateInputs);
    connect(m_portEdit, &QLineEdit::textChanged, this, &LoginDialog::validateInputs);
    connect(m_subscribeEdit, &QLineEdit::textChanged, this, &LoginDialog::validateInputs);
    connect(m_publishEdit, &QLineEdit::textChanged, this, &LoginDialog::validateInputs);
    connect(m_saveCheckBox, &QCheckBox::toggled, this, [this](bool checked) {
        m_saveSettingsFlag = checked;
    });
}

void LoginDialog::validateInputs()
{
    bool valid = !m_hostEdit->text().trimmed().isEmpty() &&
                 !m_portEdit->text().trimmed().isEmpty() &&
                 !m_subscribeEdit->text().trimmed().isEmpty() &&
                 !m_publishEdit->text().trimmed().isEmpty();
    m_okButton->setEnabled(valid);
}

QString LoginDialog::host() const
{
    return m_hostEdit->text().trimmed();
}

quint16 LoginDialog::port() const
{
    // validator 已圈定 1..65535，这里不会越界
    return static_cast<quint16>(m_portEdit->text().toUInt());
}

QString LoginDialog::subscribeTopic() const
{
    return m_subscribeEdit->text().trimmed();
}

QString LoginDialog::publishTopic() const
{
    return m_publishEdit->text().trimmed();
}

QString LoginDialog::username() const
{
    return m_usernameEdit->text().trimmed();
}

QString LoginDialog::password() const
{
    return m_passwordEdit->text();
}

void LoginDialog::setMandatory(bool mandatory)
{
    m_mandatory = mandatory;
}

void LoginDialog::setSaveSettings(bool save)
{
    m_saveSettingsFlag = save;
    m_saveCheckBox->setChecked(save);
}

bool LoginDialog::saveSettings() const
{
    return m_saveSettingsFlag;
}

void LoginDialog::loadSavedSettings()
{
    // host/port 总是回填：没存过时用默认值（与 MainWindow::loadSettings 的默认一致）
    m_hostEdit->setText(m_settings->value("mqtt/host", "139.196.24.105").toString());
    m_portEdit->setText(QString::number(m_settings->value("mqtt/port", 1883).toUInt()));

    QString savedSubscribe = m_settings->value("mqtt/subscribe_topic", "").toString();
    QString savedPublish = m_settings->value("mqtt/publish_topic", "").toString();
    QString savedUsername = m_settings->value("mqtt/username", "").toString();
    QString savedPassword = m_settings->value("mqtt/password", "").toString();

    if (!savedSubscribe.isEmpty()) {
        m_subscribeEdit->setText(savedSubscribe);
    }
    if (!savedPublish.isEmpty()) {
        m_publishEdit->setText(savedPublish);
    }
    m_usernameEdit->setText(savedUsername);
    m_passwordEdit->setText(savedPassword);

    // 检查是否有保存的设置
    bool hasSaved = !savedSubscribe.isEmpty() && !savedPublish.isEmpty();
    if (hasSaved) {
        m_titleLabel->setText(tr("确认MQTT主题（从STM32 OLED读取）"));
    }
}

void LoginDialog::onOkClicked()
{
    QString subscribe = subscribeTopic();
    QString publish = publishTopic();

    // 简单验证主题格式（可选，根据STM32规则）
    if (subscribe.isEmpty() || publish.isEmpty()) {
        QMessageBox::warning(this, tr("输入错误"),
                             tr("订阅主题和发布主题不能为空"));
        return;
    }

    // 服务器地址/端口不属于凭据，不受“记住主题设置”勾选影响，始终落盘
    //（MainWindow::saveSettings 退出时同样无条件写这两项，两处行为保持一致）
    m_settings->setValue("mqtt/host", host());
    m_settings->setValue("mqtt/port", port());

    // 保存设置（如果用户选择）；取消勾选时清除已存旧值，否则下次启动仍会回填
    if (m_saveSettingsFlag) {
        m_settings->setValue("mqtt/subscribe_topic", subscribe);
        m_settings->setValue("mqtt/publish_topic", publish);
        m_settings->setValue("mqtt/username", username());
        m_settings->setValue("mqtt/password", password());
        m_settings->sync();
    } else {
        m_settings->remove("mqtt/subscribe_topic");
        m_settings->remove("mqtt/publish_topic");
        m_settings->remove("mqtt/username");
        m_settings->remove("mqtt/password");
        m_settings->sync();
    }

    accept();
}

void LoginDialog::onCancelClicked()
{
    reject();
}

void LoginDialog::closeEvent(QCloseEvent *event)
{
    // 设置菜单打开（非强制）：关闭即取消修改，不弹确认、不退程序
    if (!m_mandatory) {
        event->accept();
        reject();
        return;
    }

    // 首次强制设置：阻止直接关闭，要求用户选择
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, tr("退出确认"),
                                  tr("未设置MQTT主题，确定要退出程序吗？"),
                                  QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        QApplication::exit(0);
        event->accept();
    } else {
        event->ignore();
    }
}