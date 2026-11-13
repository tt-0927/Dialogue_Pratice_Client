// LoginPage.cpp
#include "ui/LoginPage.h"

#include "core/UiMetrics.h"

#include "ui/widgets/Avatar.h"
#include "core/Theme.h"

#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

LoginPage::LoginPage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->addStretch(1);

    auto* card = new QFrame(this);
    card_ = card;
    card->setObjectName("Card");
    card->setFixedWidth(core::ui::cardWidth(width()));   // 初始值，resizeEvent 会再校准

    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(16, 24, 40, 30));
    card->setGraphicsEffect(shadow);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 32, 32, 28);
    cardLayout->setSpacing(10);

    auto* logo = new Avatar(card);
    logo->setFixedSize(64, 64);
    logo->setFontPx(28);
    logo->setText(QStringLiteral("D"));
    logo->setColor(theme::accent);

    auto* logoRow = new QHBoxLayout();
    logoRow->addStretch(1);
    logoRow->addWidget(logo);
    logoRow->addStretch(1);
    cardLayout->addLayout(logoRow);

    auto* title = new QLabel(QStringLiteral("Dialogue Pratice"), card);
    title->setObjectName("BigTitle");
    title->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(title);

    auto* subtitle = new QLabel(QStringLiteral("登录后即可用房间号加入对话"), card);
    subtitle->setObjectName("SubTitle");
    subtitle->setAlignment(Qt::AlignCenter);
    cardLayout->addWidget(subtitle);

    cardLayout->addSpacing(14);

    auto* serverLabel = new QLabel(QStringLiteral("服务器"), card);
    serverLabel->setObjectName("KeyLabel");
    cardLayout->addWidget(serverLabel);

    auto* serverRow = new QHBoxLayout();
    serverRow->setSpacing(8);

    hostEdit_ = new QLineEdit(card);
    hostEdit_->setObjectName("Field");
    hostEdit_->setPlaceholderText(QStringLiteral("192.168.80.128"));
    serverRow->addWidget(hostEdit_, 1);

    portEdit_ = new QLineEdit(card);
    portEdit_->setObjectName("Field");
    portEdit_->setPlaceholderText(QStringLiteral("9000"));
    portEdit_->setValidator(new QIntValidator(1, 65535, portEdit_));
    portEdit_->setAlignment(Qt::AlignCenter);
    portEdit_->setFixedWidth(88);
    portEdit_->setToolTip(QStringLiteral("服务器端口，默认 9000"));
    serverRow->addWidget(portEdit_, 0);
    cardLayout->addLayout(serverRow);

    auto* nameLabel = new QLabel(QStringLiteral("昵称"), card);
    nameLabel->setObjectName("KeyLabel");
    cardLayout->addWidget(nameLabel);

    nameEdit_ = new QLineEdit(card);
    nameEdit_->setObjectName("Field");
    nameEdit_->setPlaceholderText(QStringLiteral("例如 alice，最多 24 字节"));
    nameEdit_->setMaxLength(24);
    cardLayout->addWidget(nameEdit_);

    passwordEdit_ = new QLineEdit(card);
    passwordEdit_->setObjectName("Field");
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(
        QStringLiteral("密码（留空则按昵称匿名登录）"));
    cardLayout->addWidget(passwordEdit_);

    errorLabel_ = new QLabel(card);
    errorLabel_->setObjectName("ErrorText");
    errorLabel_->setWordWrap(true);
    errorLabel_->setVisible(false);
    cardLayout->addWidget(errorLabel_);

    cardLayout->addSpacing(4);

    loginBtn_ = new QPushButton(QStringLiteral("登  录"), card);
    loginBtn_->setObjectName("PrimaryButton");
    loginBtn_->setCursor(Qt::PointingHandCursor);
    loginBtn_->setDefault(true);
    loginBtn_->setMinimumHeight(44);
    cardLayout->addWidget(loginBtn_);

    registerBtn_ = new QPushButton(QStringLiteral("还没有账号？注册新账号"), card);
    registerBtn_->setObjectName("LinkButton");
    registerBtn_->setCursor(Qt::PointingHandCursor);
    registerBtn_->setFlat(true);
    cardLayout->addWidget(registerBtn_, 0, Qt::AlignHCenter);

    root->addWidget(card, 0, Qt::AlignHCenter);
    root->addStretch(1);

    // 恢复上次填写的服务器与昵称
    QSettings settings;
    hostEdit_->setText(settings.value("server/host", QStringLiteral("192.168.80.128")).toString());
    portEdit_->setText(settings.value("server/port", 9000).toString());
    nameEdit_->setText(settings.value("login/name").toString());

    connect(loginBtn_, &QPushButton::clicked, this, &LoginPage::submit);
    connect(nameEdit_, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(passwordEdit_, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(hostEdit_, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(portEdit_, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(registerBtn_, &QPushButton::clicked, this, &LoginPage::registerRequested);
}

void LoginPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (card_ == nullptr) return;

    // 只在宽度真的变了才动，避免 setFixedWidth 触发的递归布局
    const int target = core::ui::cardWidth(width());
    if (card_->width() != target) card_->setFixedWidth(target);
}

void LoginPage::submit() {
    clearError();

    const QString host = hostEdit_->text().trimmed();
    const QString name = nameEdit_->text().trimmed();

    if (host.isEmpty()) {
        showError(QStringLiteral("请填写服务器地址"));
        hostEdit_->setFocus();
        return;
    }
    if (name.isEmpty()) {
        showError(QStringLiteral("请填写昵称"));
        nameEdit_->setFocus();
        return;
    }
    if (name.toUtf8().size() > 24) {
        showError(QStringLiteral("昵称最多 24 字节（一个汉字约 3 字节）"));
        nameEdit_->setFocus();
        return;
    }

    bool portOk = false;
    const int portValue = portEdit_->text().toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535) {
        showError(QStringLiteral("端口必须是 1–65535 之间的数字"));
        portEdit_->setFocus();
        portEdit_->selectAll();
        return;
    }

    QSettings settings;
    settings.setValue("server/host", host);
    settings.setValue("server/port", portValue);
    settings.setValue("login/name", name);

    emit loginRequested(host, static_cast<quint16>(portValue), name,
                        passwordEdit_->text());
}

void LoginPage::setBusy(bool busy) {
    loginBtn_->setEnabled(!busy);
    loginBtn_->setText(busy ? QStringLiteral("连接中…") : QStringLiteral("登  录"));
    hostEdit_->setEnabled(!busy);
    portEdit_->setEnabled(!busy);
    nameEdit_->setEnabled(!busy);
    passwordEdit_->setEnabled(!busy);
    registerBtn_->setEnabled(!busy);
}

void LoginPage::showError(const QString& message) {
    errorLabel_->setText(message);
    errorLabel_->setVisible(true);
}

void LoginPage::clearError() {
    errorLabel_->clear();
    errorLabel_->setVisible(false);
}

QString LoginPage::host() const {
    return hostEdit_ != nullptr ? hostEdit_->text().trimmed() : QString();
}

quint16 LoginPage::port() const {
    if (portEdit_ == nullptr) return 0;
    bool ok = false;
    const uint value = portEdit_->text().toUInt(&ok);
    if (!ok || value < 1) return 0;
    return static_cast<quint16>(qMin(value, 65535u));
}

void LoginPage::setCredentials(const QString& name, const QString& password) {
    if (nameEdit_ != nullptr) nameEdit_->setText(name);
    if (passwordEdit_ != nullptr) passwordEdit_->setText(password);
    if (passwordEdit_ != nullptr) passwordEdit_->setFocus();
    clearError();
}
