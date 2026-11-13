// LoginPage.cpp
#include "LoginPage.h"

#include "Avatar.h"
#include "Theme.h"

#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

LoginPage::LoginPage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->addStretch(1);

    auto* card = new QFrame(this);
    card->setObjectName("Card");
    card->setFixedWidth(420);

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

    portSpin_ = new QSpinBox(card);
    portSpin_->setObjectName("Field");
    portSpin_->setRange(1, 65535);
    portSpin_->setValue(9000);
    portSpin_->setButtonSymbols(QAbstractSpinBox::NoButtons);
    portSpin_->setFixedWidth(96);
    serverRow->addWidget(portSpin_, 0);
    cardLayout->addLayout(serverRow);

    auto* nameLabel = new QLabel(QStringLiteral("昵称"), card);
    nameLabel->setObjectName("KeyLabel");
    cardLayout->addWidget(nameLabel);

    nameEdit_ = new QLineEdit(card);
    nameEdit_->setObjectName("Field");
    nameEdit_->setPlaceholderText(QStringLiteral("例如 alice，最多 24 字节"));
    nameEdit_->setMaxLength(24);
    cardLayout->addWidget(nameEdit_);

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

    root->addWidget(card, 0, Qt::AlignHCenter);
    root->addStretch(1);

    // 恢复上次填写的服务器与昵称
    QSettings settings;
    hostEdit_->setText(settings.value("server/host", QStringLiteral("192.168.80.128")).toString());
    portSpin_->setValue(settings.value("server/port", 9000).toInt());
    nameEdit_->setText(settings.value("login/name").toString());

    connect(loginBtn_, &QPushButton::clicked, this, &LoginPage::submit);
    connect(nameEdit_, &QLineEdit::returnPressed, this, &LoginPage::submit);
    connect(hostEdit_, &QLineEdit::returnPressed, this, &LoginPage::submit);
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

    QSettings settings;
    settings.setValue("server/host", host);
    settings.setValue("server/port", portSpin_->value());
    settings.setValue("login/name", name);

    emit loginRequested(host, static_cast<quint16>(portSpin_->value()), name);
}

void LoginPage::setBusy(bool busy) {
    loginBtn_->setEnabled(!busy);
    loginBtn_->setText(busy ? QStringLiteral("连接中…") : QStringLiteral("登  录"));
    hostEdit_->setEnabled(!busy);
    portSpin_->setEnabled(!busy);
    nameEdit_->setEnabled(!busy);
}

void LoginPage::showError(const QString& message) {
    errorLabel_->setText(message);
    errorLabel_->setVisible(true);
}

void LoginPage::clearError() {
    errorLabel_->clear();
    errorLabel_->setVisible(false);
}
