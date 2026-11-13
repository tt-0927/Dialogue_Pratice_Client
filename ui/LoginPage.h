/*
 * 文件名:    LoginPage.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CLoginPage 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CLoginPage.h */
#pragma once

#include <QWidget>

class QResizeEvent;

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

class CLoginPage : public QWidget {
    Q_OBJECT

public:
    explicit CLoginPage(QWidget* parent = nullptr);

    void setBusy(bool busy);
    void showError(const QString& message);
    void clearError();

    /* 当前填写的服务器地址（供注册对话框使用）。 */
    QString host() const;

    /* 当前填写的端口。 */
    quint16 port() const;

    /**
     * @brief 回填账号与密码。
     * @param name     登录名。
     * @param password 密码。
     * @details 注册成功后调用，省得用户再敲一遍。
     */
    void setCredentials(const QString& name, const QString& password);

signals:
    /* 用户点击登录。 */
    /* host     服务器地址。 */
    /* port     端口。 */
    /* name     账号名（或昵称）。 */
    /* password 密码；为空表示按昵称匿名登录。 */
    void loginRequested(const QString& host, quint16 port, const QString& name,
                        const QString& password);

    /* 用户点击「注册新账号」。 */
    void registerRequested();

protected:
    /* 窗口尺寸变化时按可用宽度调整卡片宽度。 */
    void resizeEvent(QResizeEvent* event) override;

private:
    void submit();

    QFrame*      m_pCard = nullptr;
    QLineEdit*   m_pHostEdit = nullptr;
    /* 端口输入。用 QLineEdit + QIntValidator 而非 QSpinBox： */
    /* QSpinBox 套上 QSS 的 padding 后，内部 lineEdit 的几何计算会出错， */
    /* 窄屏（尤其 Android）下文本被裁成只剩一个字符，且 setFixedWidth 失效。 */
    QLineEdit*   m_pPortEdit = nullptr;
    QLineEdit*   m_pNameEdit = nullptr;
    QLineEdit*   m_pPasswordEdit = nullptr;
    QPushButton* m_pLoginBtn = nullptr;
    QPushButton* m_pRegisterBtn = nullptr;
    QLabel*      m_pErrorLabel = nullptr;
};
