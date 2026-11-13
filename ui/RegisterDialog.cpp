/**
 * @file  RegisterDialog.cpp
 * @brief 注册对话框的实现。
 */

#include "ui/RegisterDialog.h"

#include "core/Logger.h"
#include "net/ProtocolBridge.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QIntValidator>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QString kTag = QStringLiteral("ui");
/// @brief 连接 + 响应总超时（毫秒）。
constexpr int kTimeoutMs = 12000;
}  // namespace

RegisterDialog::RegisterDialog(const QString& host, quint16 port, QWidget* parent)
    : QDialog(parent), host_(host), port_(port) {
    setWindowTitle(QStringLiteral("注册新账号"));
    setModal(true);
    // 固定 420 在手机竖屏（360–420dp）会超出屏幕右侧，
    // 按父窗口宽度自适应：窄屏取「父宽 - 两侧边距」。
    const int parentWidth = (parent != nullptr) ? parent->width() : 420;
    setMinimumWidth(qBound(300, parentWidth - 48, 420));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    auto* title = new QLabel(QStringLiteral("创建账号"), this);
    title->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 700;"));
    root->addWidget(title);

    auto* hint = new QLabel(
        QStringLiteral("登录名 3–24 位，可用字母、数字、下划线和短横线；密码至少 6 位。"),
        this);
    hint->setObjectName("HintLabel");
    hint->setWordWrap(true);
    root->addWidget(hint);

    auto* form = new QFormLayout();
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(10);

    userEdit_ = new QLineEdit(this);
    userEdit_->setObjectName("Field");
    userEdit_->setMaxLength(24);
    userEdit_->setPlaceholderText(QStringLiteral("例如 alice"));
    form->addRow(QStringLiteral("登录名 *"), userEdit_);

    passEdit_ = new QLineEdit(this);
    passEdit_->setObjectName("Field");
    passEdit_->setEchoMode(QLineEdit::Password);
    passEdit_->setPlaceholderText(QStringLiteral("至少 6 位"));
    form->addRow(QStringLiteral("密码 *"), passEdit_);

    pass2Edit_ = new QLineEdit(this);
    pass2Edit_->setObjectName("Field");
    pass2Edit_->setEchoMode(QLineEdit::Password);
    pass2Edit_->setPlaceholderText(QStringLiteral("再输一次"));
    form->addRow(QStringLiteral("确认密码 *"), pass2Edit_);

    nickEdit_ = new QLineEdit(this);
    nickEdit_->setObjectName("Field");
    nickEdit_->setMaxLength(24);
    nickEdit_->setPlaceholderText(QStringLiteral("留空则与登录名相同"));
    form->addRow(QStringLiteral("昵称"), nickEdit_);

    // 用 QLineEdit + 校验器而不是 QSpinBox：后者套上 QSS padding 后，
    // 上下按钮在窄屏会渲染成一条被裁切的残影（端口框踩过同样的坑）。
    ageEdit_ = new QLineEdit(this);
    ageEdit_->setObjectName("Field");
    ageEdit_->setValidator(new QIntValidator(0, 150, ageEdit_));
    ageEdit_->setPlaceholderText(QStringLiteral("不填"));
    form->addRow(QStringLiteral("年龄"), ageEdit_);

    descEdit_ = new QPlainTextEdit(this);
    descEdit_->setObjectName("Field");
    descEdit_->setFixedHeight(64);
    descEdit_->setPlaceholderText(QStringLiteral("一句话介绍自己（可选，最多 200 字节）"));
    form->addRow(QStringLiteral("简介"), descEdit_);

    root->addLayout(form);

    statusLabel_ = new QLabel(this);
    statusLabel_->setWordWrap(true);
    statusLabel_->setVisible(false);
    root->addWidget(statusLabel_);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);

    cancelBtn_ = new QPushButton(QStringLiteral("取消"), this);
    cancelBtn_->setObjectName("LinkButton");
    cancelBtn_->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(cancelBtn_);

    submitBtn_ = new QPushButton(QStringLiteral("注册"), this);
    submitBtn_->setDefault(true);
    submitBtn_->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(submitBtn_);

    root->addLayout(buttons);

    // ---- 临时连接：注册发生在登录之前，用独立的 bridge，用完即销毁 ----
    bridge_ = new ProtocolBridge(this);

    connect(bridge_, &ProtocolBridge::connected, this, [this] {
        bridge_->registerAccount(userEdit_->text().trimmed(), passEdit_->text(),
                                 nickEdit_->text().trimmed(), ageEdit_->text().toInt(),
                                 descEdit_->toPlainText().trimmed());
    });
    connect(bridge_, &ProtocolBridge::registerSucceeded, this,
            [this](qint64 /*userId*/, const QString& username, const QString& nickname) {
                DP_LOG_INFO(kTag, QStringLiteral("注册成功：%1（%2）").arg(username, nickname));
                timeoutTimer_->stop();
                accept();
            });
    connect(bridge_, &ProtocolBridge::serverError, this,
            [this](const QString& code, const QString& text) {
                DP_LOG_WARN(kTag, QStringLiteral("注册失败 %1：%2").arg(code, text));
                setBusy(false);
                showError(text);
                teardown();
            });
    connect(bridge_, &ProtocolBridge::networkError, this, [this](const QString& message) {
        setBusy(false);
        showError(QStringLiteral("连接失败：%1").arg(message));
        teardown();
    });

    timeoutTimer_ = new QTimer(this);
    timeoutTimer_->setSingleShot(true);
    connect(timeoutTimer_, &QTimer::timeout, this, [this] {
        setBusy(false);
        showError(QStringLiteral("连接超时，请检查服务器地址与端口"));
        teardown();
    });

    connect(submitBtn_, &QPushButton::clicked, this, &RegisterDialog::submit);
    connect(cancelBtn_, &QPushButton::clicked, this, &RegisterDialog::reject);
    connect(userEdit_, &QLineEdit::returnPressed, this, &RegisterDialog::submit);
    connect(passEdit_, &QLineEdit::returnPressed, this, &RegisterDialog::submit);
    connect(pass2Edit_, &QLineEdit::returnPressed, this, &RegisterDialog::submit);

    userEdit_->setFocus();
}

RegisterDialog::~RegisterDialog() = default;

QString RegisterDialog::username() const {
    return userEdit_ != nullptr ? userEdit_->text().trimmed() : QString();
}

QString RegisterDialog::password() const {
    return passEdit_ != nullptr ? passEdit_->text() : QString();
}

void RegisterDialog::submit() {
    if (!submitBtn_->isEnabled()) return;

    const QString username = userEdit_->text().trimmed();
    const QString password = passEdit_->text();
    const QString confirm = pass2Edit_->text();

    // 本地先挡一遍，省得为了一个笔误跑一趟网络
    if (username.size() < 3) {
        showError(QStringLiteral("登录名至少 3 个字符"));
        userEdit_->setFocus();
        return;
    }
    if (password.size() < 6) {
        showError(QStringLiteral("密码至少 6 位"));
        passEdit_->setFocus();
        return;
    }
    if (password != confirm) {
        showError(QStringLiteral("两次输入的密码不一致"));
        pass2Edit_->setFocus();
        return;
    }

    setBusy(true);
    statusLabel_->setText(QStringLiteral("正在连接服务器…"));
    statusLabel_->setStyleSheet(QStringLiteral("color:#5B6675;"));
    statusLabel_->setVisible(true);

    timeoutTimer_->start(kTimeoutMs);
    bridge_->connectToServer(host_, port_);
}

void RegisterDialog::setBusy(bool busy) {
    userEdit_->setEnabled(!busy);
    passEdit_->setEnabled(!busy);
    pass2Edit_->setEnabled(!busy);
    nickEdit_->setEnabled(!busy);
    ageEdit_->setEnabled(!busy);
    descEdit_->setEnabled(!busy);
    submitBtn_->setEnabled(!busy);
}

void RegisterDialog::showError(const QString& text) {
    statusLabel_->setText(text);
    statusLabel_->setStyleSheet(QStringLiteral("color:#E5484D;"));
    statusLabel_->setVisible(true);
}

void RegisterDialog::teardown() {
    if (timeoutTimer_ != nullptr) timeoutTimer_->stop();
    if (bridge_ != nullptr) bridge_->disconnectFromServer();
}
