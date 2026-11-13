/*
 * 文件名:    RegisterDialog.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  注册对话框的实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
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
/* 连接 + 响应总超时（毫秒）。 */
constexpr int kTimeoutMs = 12000;
/* namespace */
}

CRegisterDialog::CRegisterDialog(const QString& host, quint16 port, QWidget* parent)
    : QDialog(parent), m_strHost(host), m_uPort(port) {
    setWindowTitle(QStringLiteral("注册新账号"));
    setModal(true);
    /* 固定 420 在手机竖屏（360–420dp）会超出屏幕右侧， */
    /* 按父窗口宽度自适应：窄屏取「父宽 - 两侧边距」。 */
    const int parentWidth = (parent != nullptr) ? parent->width() : 420;
    setMinimumWidth(qBound(300, parentWidth - 48, 420));

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 20, 24, 20);
    root->setSpacing(12);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
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

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pUserEdit = new QLineEdit(this);
    m_pUserEdit->setObjectName("Field");
    m_pUserEdit->setMaxLength(24);
    m_pUserEdit->setPlaceholderText(QStringLiteral("例如 alice"));
    form->addRow(QStringLiteral("登录名 *"), m_pUserEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPassEdit = new QLineEdit(this);
    m_pPassEdit->setObjectName("Field");
    m_pPassEdit->setEchoMode(QLineEdit::Password);
    m_pPassEdit->setPlaceholderText(QStringLiteral("至少 6 位"));
    form->addRow(QStringLiteral("密码 *"), m_pPassEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPass2Edit = new QLineEdit(this);
    m_pPass2Edit->setObjectName("Field");
    m_pPass2Edit->setEchoMode(QLineEdit::Password);
    m_pPass2Edit->setPlaceholderText(QStringLiteral("再输一次"));
    form->addRow(QStringLiteral("确认密码 *"), m_pPass2Edit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNickEdit = new QLineEdit(this);
    m_pNickEdit->setObjectName("Field");
    m_pNickEdit->setMaxLength(24);
    m_pNickEdit->setPlaceholderText(QStringLiteral("留空则与登录名相同"));
    form->addRow(QStringLiteral("昵称"), m_pNickEdit);

    /* 用 QLineEdit + 校验器而不是 QSpinBox：后者套上 QSS padding 后， */
    /* 上下按钮在窄屏会渲染成一条被裁切的残影（端口框踩过同样的坑）。 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAgeEdit = new QLineEdit(this);
    m_pAgeEdit->setObjectName("Field");
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAgeEdit->setValidator(new QIntValidator(0, 150, m_pAgeEdit));
    m_pAgeEdit->setPlaceholderText(QStringLiteral("不填"));
    form->addRow(QStringLiteral("年龄"), m_pAgeEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pDescEdit = new QPlainTextEdit(this);
    m_pDescEdit->setObjectName("Field");
    m_pDescEdit->setFixedHeight(64);
    m_pDescEdit->setPlaceholderText(QStringLiteral("一句话介绍自己（可选，最多 200 字节）"));
    form->addRow(QStringLiteral("简介"), m_pDescEdit);

    root->addLayout(form);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pStatusLabel = new QLabel(this);
    m_pStatusLabel->setWordWrap(true);
    m_pStatusLabel->setVisible(false);
    root->addWidget(m_pStatusLabel);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pCancelBtn = new QPushButton(QStringLiteral("取消"), this);
    m_pCancelBtn->setObjectName("LinkButton");
    m_pCancelBtn->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_pCancelBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pSubmitBtn = new QPushButton(QStringLiteral("注册"), this);
    m_pSubmitBtn->setDefault(true);
    m_pSubmitBtn->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_pSubmitBtn);

    root->addLayout(buttons);

    /* ---- 临时连接：注册发生在登录之前，用独立的 bridge，用完即销毁 ---- */
    m_pBridge = new CProtocolBridge(this);

    connect(m_pBridge, &CProtocolBridge::connected, this, [this] {
        m_pBridge->registerAccount(m_pUserEdit->text().trimmed(), m_pPassEdit->text(),
                                 m_pNickEdit->text().trimmed(), m_pAgeEdit->text().toInt(),
                                 m_pDescEdit->toPlainText().trimmed());
    });
    connect(m_pBridge, &CProtocolBridge::registerSucceeded, this,
            [this](qint64 /*userId*/, const QString& username, const QString& nickname) {
                DP_LOG_INFO(kTag, QStringLiteral("注册成功：%1（%2）").arg(username, nickname));
                m_pTimeoutTimer->stop();
                accept();
            });
    connect(m_pBridge, &CProtocolBridge::serverError, this,
            [this](const QString& code, const QString& text) {
                DP_LOG_WARN(kTag, QStringLiteral("注册失败 %1：%2").arg(code, text));
                setBusy(false);
                showError(text);
                teardown();
            });
    connect(m_pBridge, &CProtocolBridge::networkError, this, [this](const QString& message) {
        setBusy(false);
        showError(QStringLiteral("连接失败：%1").arg(message));
        teardown();
    });

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTimeoutTimer = new QTimer(this);
    m_pTimeoutTimer->setSingleShot(true);
    connect(m_pTimeoutTimer, &QTimer::timeout, this, [this] {
        setBusy(false);
        showError(QStringLiteral("连接超时，请检查服务器地址与端口"));
        teardown();
    });

    connect(m_pSubmitBtn, &QPushButton::clicked, this, &CRegisterDialog::submit);
    connect(m_pCancelBtn, &QPushButton::clicked, this, &CRegisterDialog::reject);
    connect(m_pUserEdit, &QLineEdit::returnPressed, this, &CRegisterDialog::submit);
    connect(m_pPassEdit, &QLineEdit::returnPressed, this, &CRegisterDialog::submit);
    connect(m_pPass2Edit, &QLineEdit::returnPressed, this, &CRegisterDialog::submit);

    m_pUserEdit->setFocus();
}

CRegisterDialog::~CRegisterDialog() = default;

/*
 * 功能描述:  执行 username 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
QString CRegisterDialog::username() const {
    return m_pUserEdit != nullptr ? m_pUserEdit->text().trimmed() : QString();
}

/*
 * 功能描述:  执行 password 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
QString CRegisterDialog::password() const {
    return m_pPassEdit != nullptr ? m_pPassEdit->text() : QString();
}

/*
 * 功能描述:  执行 submit 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CRegisterDialog::submit() {
    if (!m_pSubmitBtn->isEnabled()) return;

    const QString username = m_pUserEdit->text().trimmed();
    const QString password = m_pPassEdit->text();
    const QString confirm = m_pPass2Edit->text();

    /* 本地先挡一遍，省得为了一个笔误跑一趟网络 */
    if (username.size() < 3) {
        showError(QStringLiteral("登录名至少 3 个字符"));
        m_pUserEdit->setFocus();
        return;
    }
    if (password.size() < 6) {
        showError(QStringLiteral("密码至少 6 位"));
        m_pPassEdit->setFocus();
        return;
    }
    if (password != confirm) {
        showError(QStringLiteral("两次输入的密码不一致"));
        m_pPass2Edit->setFocus();
        return;
    }

    setBusy(true);
    m_pStatusLabel->setText(QStringLiteral("正在连接服务器…"));
    m_pStatusLabel->setStyleSheet(QStringLiteral("color:#5B6675;"));
    m_pStatusLabel->setVisible(true);

    m_pTimeoutTimer->start(kTimeoutMs);
    m_pBridge->connectToServer(m_strHost, m_uPort);
}

/*
 * 功能描述:  执行 setBusy 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  busy  参数。
 * return:    void
 */
void CRegisterDialog::setBusy(bool busy) {
    m_pUserEdit->setEnabled(!busy);
    m_pPassEdit->setEnabled(!busy);
    m_pPass2Edit->setEnabled(!busy);
    m_pNickEdit->setEnabled(!busy);
    m_pAgeEdit->setEnabled(!busy);
    m_pDescEdit->setEnabled(!busy);
    m_pSubmitBtn->setEnabled(!busy);
}

/*
 * 功能描述:  执行 showError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CRegisterDialog::showError(const QString& text) {
    m_pStatusLabel->setText(text);
    m_pStatusLabel->setStyleSheet(QStringLiteral("color:#E5484D;"));
    m_pStatusLabel->setVisible(true);
}

/*
 * 功能描述:  执行 teardown 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CRegisterDialog::teardown() {
    if (m_pTimeoutTimer != nullptr) m_pTimeoutTimer->stop();
    if (m_pBridge != nullptr) m_pBridge->disconnectFromServer();
}
