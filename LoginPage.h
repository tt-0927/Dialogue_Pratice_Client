// LoginPage.h
#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

class LoginPage : public QWidget {
    Q_OBJECT

public:
    explicit LoginPage(QWidget* parent = nullptr);

    void setBusy(bool busy);
    void showError(const QString& message);
    void clearError();

signals:
    void loginRequested(const QString& host, quint16 port, const QString& name);

private:
    void submit();

    QLineEdit*   hostEdit_ = nullptr;
    QSpinBox*    portSpin_ = nullptr;
    QLineEdit*   nameEdit_ = nullptr;
    QPushButton* loginBtn_ = nullptr;
    QLabel*      errorLabel_ = nullptr;
};
