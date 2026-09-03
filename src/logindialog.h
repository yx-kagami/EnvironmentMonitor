#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QSettings>

QT_BEGIN_NAMESPACE
class QLabel;
class QLineEdit;
class QPushButton;
class QCheckBox;
QT_END_NAMESPACE

// 登录对话框：输入 MQTT 订阅/发布主题，主题格式参考 pubXXXXXX / subXXXXXX
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    // 获取用户输入的连接参数、主题与凭据
    QString host() const;
    quint16 port() const;
    QString subscribeTopic() const;
    QString publishTopic() const;
    QString username() const;
    QString password() const;

    // mandatory=true（默认，首次启动强制设置）：关闭对话框弹退出确认；
    // mandatory=false（设置菜单打开）：关闭即取消修改
    void setMandatory(bool mandatory);

    // 设置保存设置
    void setSaveSettings(bool save);
    bool saveSettings() const;

    // 加载保存的设置
    void loadSavedSettings();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onOkClicked();
    void onCancelClicked();

private:
    void setupUI();
    void setupConnections();
    void validateInputs();

    // UI组件
    QLabel *m_titleLabel;
    QLabel *m_subscribeLabel;
    QLabel *m_publishLabel;
    QLineEdit *m_hostEdit;
    QLineEdit *m_portEdit;
    QLineEdit *m_subscribeEdit;
    QLineEdit *m_publishEdit;
    QLineEdit *m_usernameEdit;
    QLineEdit *m_passwordEdit;
    QCheckBox *m_saveCheckBox;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;

    // 设置
    QSettings *m_settings;
    bool m_saveSettingsFlag;
    bool m_mandatory = true;
};

#endif // LOGINDIALOG_H