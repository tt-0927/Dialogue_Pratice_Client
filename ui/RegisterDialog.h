/**
 * @file  RegisterDialog.h
 * @brief 自助注册对话框。
 * @details 自己维护一条**临时连接**（独立的 `ProtocolBridge`）：
 *          注册发生在登录之前，此时主连接还不存在，而且注册失败不该影响
 *          主窗口的连接状态。用完即销毁，干净利落。
 */

#pragma once

#include <QDialog>
#include <QString>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QLineEdit;
class QTimer;
class ProtocolBridge;

/**
 * @brief 注册新账号。
 *
 * @details 必填项只有登录名与密码；昵称、年龄、简介留空时由服务端用默认规则填充。
 *          注册成功后对话框返回 `Accepted`，调用方用 `username()` / `password()`
 *          回填登录表单，省得用户再敲一遍。
 */
class RegisterDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 构造对话框。
     * @param host   服务器地址。
     * @param port   服务器端口。
     * @param parent 父窗口。
     */
    RegisterDialog(const QString& host, quint16 port, QWidget* parent = nullptr);

    ~RegisterDialog() override;

    /// @brief 注册成功的登录名（未成功时为空）。
    QString username() const;

    /// @brief 注册时填的密码（用于回填登录表单）。
    QString password() const;

private:
    /// @brief 校验输入并发起注册。
    void submit();

    /**
     * @brief 切换忙碌状态。
     * @param busy true 时禁用输入并显示「正在注册…」。
     */
    void setBusy(bool busy);

    /**
     * @brief 显示错误提示。
     * @param text 提示文本。
     */
    void showError(const QString& text);

    /// @brief 清理临时连接。
    void teardown();

    QString host_;              ///< 服务器地址
    quint16 port_ = 0;          ///< 服务器端口
    ProtocolBridge* bridge_ = nullptr;  ///< 临时连接（仅注册期间存在）
    QTimer*         timeoutTimer_ = nullptr; ///< 连接/响应超时

    QLineEdit*      userEdit_ = nullptr;   ///< 登录名
    QLineEdit*      passEdit_ = nullptr;   ///< 密码
    QLineEdit*      pass2Edit_ = nullptr;  ///< 确认密码
    QLineEdit*      nickEdit_ = nullptr;   ///< 昵称（可选）
    QLineEdit*      ageEdit_ = nullptr;    ///< 年龄（可选，留空=0）
    QPlainTextEdit* descEdit_ = nullptr;   ///< 简介（可选）
    QPushButton*    submitBtn_ = nullptr;  ///< 注册按钮
    QPushButton*    cancelBtn_ = nullptr;  ///< 取消按钮
    QLabel*         statusLabel_ = nullptr;///< 状态提示
};
