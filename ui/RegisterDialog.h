/*
 * 文件名:    RegisterDialog.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  自助注册对话框。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
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
class CProtocolBridge;

/**
 * @brief 注册新账号。
 *
 * @details 必填项只有登录名与密码；昵称、年龄、简介留空时由服务端用默认规则填充。
 *          注册成功后对话框返回 `Accepted`，调用方用 `username()` / `password()`
 *          回填登录表单，省得用户再敲一遍。
 */
class CRegisterDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief 构造对话框。
     * @param host   服务器地址。
     * @param port   服务器端口。
     * @param parent 父窗口。
     */
    CRegisterDialog(const QString& host, quint16 port, QWidget* parent = nullptr);

    ~CRegisterDialog() override;

    /* 注册成功的登录名（未成功时为空）。 */
    QString username() const;

    /* 注册时填的密码（用于回填登录表单）。 */
    QString password() const;

private:
    /* 校验输入并发起注册。 */
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

    /* 清理临时连接。 */
    void teardown();

    /* < 服务器地址 */
    QString m_strHost;
    /* < 服务器端口 */
    quint16 m_uPort = 0;
    /* < 临时连接（仅注册期间存在） */
    CProtocolBridge* m_pBridge = nullptr;
    /* < 连接/响应超时 */
    QTimer*         m_pTimeoutTimer = nullptr;

    /* < 登录名 */
    QLineEdit*      m_pUserEdit = nullptr;
    /* < 密码 */
    QLineEdit*      m_pPassEdit = nullptr;
    /* < 确认密码 */
    QLineEdit*      m_pPass2Edit = nullptr;
    /* < 昵称（可选） */
    QLineEdit*      m_pNickEdit = nullptr;
    /* < 年龄（可选，留空=0） */
    QLineEdit*      m_pAgeEdit = nullptr;
    /* < 简介（可选） */
    QPlainTextEdit* m_pDescEdit = nullptr;
    /* < 注册按钮 */
    QPushButton*    m_pSubmitBtn = nullptr;
    /* < 取消按钮 */
    QPushButton*    m_pCancelBtn = nullptr;
    /* < 状态提示 */
    QLabel*         m_pStatusLabel = nullptr;
};
