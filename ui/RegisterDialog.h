/**
 * @file       RegisterDialog.h
 * @brief      自助注册对话框。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
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
 * @brief   注册新账号。
 * @details 必填项只有登录名与密码；昵称、年龄、简介留空时由服务端用默认规则填充。 注册成功后对话框返回 `Accepted`，调用方用 `username()` / `password()` 回填登录表单，省得用户再敲一遍。
 * @author  JaTin
 */
class CRegisterDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief      构造对话框。
     * @param[in] host           服务器地址；类型 const QString&。
     * @param[in] port           服务器端口；类型 quint16。
     * @param[in] parent         父窗口；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    CRegisterDialog(const QString& host, quint16 port, QWidget* parent = nullptr);

    ~CRegisterDialog() override;

    /**
     * @brief      注册成功的登录名（未成功时为空）。
     * @note       本函数无输入参数。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    QString username() const;

    /**
     * @brief      注册时填的密码（用于回填登录表单）。
     * @note       本函数无输入参数。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    QString password() const;

private:
    /**
     * @brief      校验输入并发起注册。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit();

    /**
     * @brief      切换忙碌状态。
     * @param[in] busy           true 时禁用输入并显示「正在注册…」；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setBusy(bool busy);

    /**
     * @brief      显示错误提示。
     * @param[in] text           提示文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showError(const QString& text);

    /**
     * @brief      清理临时连接。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void teardown();

    /**
     * @brief  服务器地址
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strHost;
    /**
     * @brief  服务器端口
     * @note   类型 quint16；类/结构体成员变量。
     */
    quint16 m_uPort = 0;
    /**
     * @brief  临时连接（仅注册期间存在）
     * @note   类型 CProtocolBridge*；类/结构体成员变量。
     */
    CProtocolBridge* m_pBridge = nullptr;
    /**
     * @brief  连接/响应超时
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*         m_pTimeoutTimer = nullptr;

    /**
     * @brief  登录名
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pUserEdit = nullptr;
    /**
     * @brief  密码
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pPassEdit = nullptr;
    /**
     * @brief  确认密码
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pPass2Edit = nullptr;
    /**
     * @brief  昵称（可选）
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pNickEdit = nullptr;
    /**
     * @brief  年龄（可选，留空=0）
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pAgeEdit = nullptr;
    /**
     * @brief  简介（可选）
     * @note   类型 QPlainTextEdit*；类/结构体成员变量。
     */
    QPlainTextEdit* m_pDescEdit = nullptr;
    /**
     * @brief  注册按钮
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton*    m_pSubmitBtn = nullptr;
    /**
     * @brief  取消按钮
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton*    m_pCancelBtn = nullptr;
    /**
     * @brief  状态提示
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*         m_pStatusLabel = nullptr;
};
