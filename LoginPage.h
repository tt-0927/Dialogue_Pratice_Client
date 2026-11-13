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

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class CLoginPage : public QWidget {
    Q_OBJECT

public:
    explicit CLoginPage(QWidget* parent = nullptr);

    void setBusy(bool busy);
    void showError(const QString& message);
    void clearError();

signals:
    void loginRequested(const QString& host, quint16 port, const QString& name);

private:
    void submit();

    QLineEdit*   m_pHostEdit = nullptr;
    QSpinBox*    m_pPortSpin = nullptr;
    QLineEdit*   m_pNameEdit = nullptr;
    QPushButton* m_pLoginBtn = nullptr;
    QLabel*      m_pErrorLabel = nullptr;
};
