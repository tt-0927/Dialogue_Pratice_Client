/*
 * 文件名:    HomePage.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CHomePage 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CHomePage.h */

/* 登录后的主页：个人信息卡 + 编辑资料 + 房间号输入 + 进入房间。 */
#pragma once

#include <QWidget>

class CAvatar;
class QLabel;
class QLineEdit;
class QPushButton;

class CHomePage : public QWidget {
    Q_OBJECT

public:
    explicit CHomePage(QWidget* parent = nullptr);

    void setProfile(qint64 id, const QString& name, int age, const QString& desc);
    void setServerText(const QString& text);
    void showError(const QString& message);
    void clearError();
    void setBusy(bool busy);
    void focusRoomInput();

signals:
    void editProfileRequested();
    void joinRoomRequested(const QString& room);
    void logoutRequested();

private:
    void submitRoom();

    CAvatar*      m_pAvatar = nullptr;
    QLabel*      m_pNameLabel = nullptr;
    QLabel*      m_pIdLabel = nullptr;
    QLabel*      m_pNameValue = nullptr;
    QLabel*      m_pAgeValue = nullptr;
    QLabel*      m_pDescValue = nullptr;
    QLineEdit*   m_pRoomEdit = nullptr;
    QPushButton* m_pJoinBtn = nullptr;
    QLabel*      m_pErrorLabel = nullptr;
    QLabel*      m_pServerLabel = nullptr;
};
