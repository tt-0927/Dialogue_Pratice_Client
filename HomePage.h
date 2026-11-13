// HomePage.h
//
// 登录后的主页：个人信息卡 + 编辑资料 + 房间号输入 + 进入房间。
#pragma once

#include <QWidget>

class Avatar;
class QLabel;
class QLineEdit;
class QPushButton;

class HomePage : public QWidget {
    Q_OBJECT

public:
    explicit HomePage(QWidget* parent = nullptr);

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

    Avatar*      avatar_ = nullptr;
    QLabel*      nameLabel_ = nullptr;
    QLabel*      idLabel_ = nullptr;
    QLabel*      nameValue_ = nullptr;
    QLabel*      ageValue_ = nullptr;
    QLabel*      descValue_ = nullptr;
    QLineEdit*   roomEdit_ = nullptr;
    QPushButton* joinBtn_ = nullptr;
    QLabel*      errorLabel_ = nullptr;
    QLabel*      serverLabel_ = nullptr;
};
