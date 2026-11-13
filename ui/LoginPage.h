// LoginPage.h
#pragma once

#include <QWidget>

class QResizeEvent;

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

class LoginPage : public QWidget {
    Q_OBJECT

public:
    explicit LoginPage(QWidget* parent = nullptr);

    void setBusy(bool busy);
    void showError(const QString& message);
    void clearError();

    /// @brief 当前填写的服务器地址（供注册对话框使用）。
    QString host() const;

    /// @brief 当前填写的端口。
    quint16 port() const;

    /**
     * @brief 回填账号与密码。
     * @param name     登录名。
     * @param password 密码。
     * @details 注册成功后调用，省得用户再敲一遍。
     */
    void setCredentials(const QString& name, const QString& password);

signals:
    /// @brief 用户点击登录。
    /// @param host     服务器地址。
    /// @param port     端口。
    /// @param name     账号名（或昵称）。
    /// @param password 密码；为空表示按昵称匿名登录。
    void loginRequested(const QString& host, quint16 port, const QString& name,
                        const QString& password);

    /// @brief 用户点击「注册新账号」。
    void registerRequested();

protected:
    /// @brief 窗口尺寸变化时按可用宽度调整卡片宽度。
    void resizeEvent(QResizeEvent* event) override;

private:
    void submit();

    QFrame*      card_ = nullptr;
    QLineEdit*   hostEdit_ = nullptr;
    /// @brief 端口输入。用 QLineEdit + QIntValidator 而非 QSpinBox：
    ///        QSpinBox 套上 QSS 的 padding 后，内部 lineEdit 的几何计算会出错，
    ///        窄屏（尤其 Android）下文本被裁成只剩一个字符，且 setFixedWidth 失效。
    QLineEdit*   portEdit_ = nullptr;
    QLineEdit*   nameEdit_ = nullptr;
    QLineEdit*   passwordEdit_ = nullptr;
    QPushButton* loginBtn_ = nullptr;
    QPushButton* registerBtn_ = nullptr;
    QLabel*      errorLabel_ = nullptr;
};
