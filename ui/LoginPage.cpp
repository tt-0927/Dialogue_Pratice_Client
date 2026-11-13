/*
 * 文件名:    LoginPage.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CLoginPage 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CLoginPage.cpp */
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

/*
 * 功能描述:  执行 CLoginPage 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CLoginPage::CLoginPage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(24, 24, 24, 24);
    root->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* card = new QFrame(this);
    m_pCard = card;
    card->setObjectName("Card");
    /* 初始值，resizeEvent 会再校准 */
    card->setFixedWidth(core::ui::cardWidth(width()));

    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(16, 24, 40, 30));
    card->setGraphicsEffect(shadow);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 32, 32, 28);
    cardLayout->setSpacing(10);

    auto* logo = new CAvatar(card);
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

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pHostEdit = new QLineEdit(card);
    m_pHostEdit->setObjectName("Field");
    m_pHostEdit->setPlaceholderText(QStringLiteral("192.168.80.128"));
    serverRow->addWidget(m_pHostEdit, 1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPortEdit = new QLineEdit(card);
    m_pPortEdit->setObjectName("Field");
    m_pPortEdit->setPlaceholderText(QStringLiteral("9000"));
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPortEdit->setValidator(new QIntValidator(1, 65535, m_pPortEdit));
    m_pPortEdit->setAlignment(Qt::AlignCenter);
    m_pPortEdit->setFixedWidth(88);
    m_pPortEdit->setToolTip(QStringLiteral("服务器端口，默认 9000"));
    serverRow->addWidget(m_pPortEdit, 0);
    cardLayout->addLayout(serverRow);

    auto* nameLabel = new QLabel(QStringLiteral("昵称"), card);
    nameLabel->setObjectName("KeyLabel");
    cardLayout->addWidget(nameLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNameEdit = new QLineEdit(card);
    m_pNameEdit->setObjectName("Field");
    m_pNameEdit->setPlaceholderText(QStringLiteral("例如 alice，最多 24 字节"));
    m_pNameEdit->setMaxLength(24);
    cardLayout->addWidget(m_pNameEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPasswordEdit = new QLineEdit(card);
    m_pPasswordEdit->setObjectName("Field");
    m_pPasswordEdit->setEchoMode(QLineEdit::Password);
    m_pPasswordEdit->setPlaceholderText(
        QStringLiteral("密码（留空则按昵称匿名登录）"));
    cardLayout->addWidget(m_pPasswordEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pErrorLabel = new QLabel(card);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    cardLayout->addWidget(m_pErrorLabel);

    cardLayout->addSpacing(4);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pLoginBtn = new QPushButton(QStringLiteral("登  录"), card);
    m_pLoginBtn->setObjectName("PrimaryButton");
    m_pLoginBtn->setCursor(Qt::PointingHandCursor);
    m_pLoginBtn->setDefault(true);
    m_pLoginBtn->setMinimumHeight(44);
    cardLayout->addWidget(m_pLoginBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRegisterBtn = new QPushButton(QStringLiteral("还没有账号？注册新账号"), card);
    m_pRegisterBtn->setObjectName("LinkButton");
    m_pRegisterBtn->setCursor(Qt::PointingHandCursor);
    m_pRegisterBtn->setFlat(true);
    cardLayout->addWidget(m_pRegisterBtn, 0, Qt::AlignHCenter);

    root->addWidget(card, 0, Qt::AlignHCenter);
    root->addStretch(1);

    /* 恢复上次填写的服务器与昵称 */
    QSettings settings;
    m_pHostEdit->setText(settings.value("server/host", QStringLiteral("192.168.80.128")).toString());
    m_pPortEdit->setText(settings.value("server/port", 9000).toString());
    m_pNameEdit->setText(settings.value("login/name").toString());

    connect(m_pLoginBtn, &QPushButton::clicked, this, &CLoginPage::submit);
    connect(m_pNameEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pPasswordEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pHostEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pPortEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pRegisterBtn, &QPushButton::clicked, this, &CLoginPage::registerRequested);
}

/*
 * 功能描述:  执行 resizeEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CLoginPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (m_pCard == nullptr) return;

    /* 只在宽度真的变了才动，避免 setFixedWidth 触发的递归布局 */
    const int target = core::ui::cardWidth(width());
    if (m_pCard->width() != target) m_pCard->setFixedWidth(target);
}

/*
 * 功能描述:  执行 submit 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CLoginPage::submit() {
    clearError();

    const QString host = m_pHostEdit->text().trimmed();
    const QString name = m_pNameEdit->text().trimmed();

    if (host.isEmpty()) {
        showError(QStringLiteral("请填写服务器地址"));
        m_pHostEdit->setFocus();
        return;
    }
    if (name.isEmpty()) {
        showError(QStringLiteral("请填写昵称"));
        m_pNameEdit->setFocus();
        return;
    }
    if (name.toUtf8().size() > 24) {
        showError(QStringLiteral("昵称最多 24 字节（一个汉字约 3 字节）"));
        m_pNameEdit->setFocus();
        return;
    }

    bool portOk = false;
    const int portValue = m_pPortEdit->text().toInt(&portOk);
    if (!portOk || portValue < 1 || portValue > 65535) {
        showError(QStringLiteral("端口必须是 1–65535 之间的数字"));
        m_pPortEdit->setFocus();
        m_pPortEdit->selectAll();
        return;
    }

    QSettings settings;
    settings.setValue("server/host", host);
    settings.setValue("server/port", portValue);
    settings.setValue("login/name", name);

    emit loginRequested(host, static_cast<quint16>(portValue), name,
                        m_pPasswordEdit->text());
}

/*
 * 功能描述:  执行 setBusy 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  busy  参数。
 * return:    void
 */
void CLoginPage::setBusy(bool busy) {
    m_pLoginBtn->setEnabled(!busy);
    m_pLoginBtn->setText(busy ? QStringLiteral("连接中…") : QStringLiteral("登  录"));
    m_pHostEdit->setEnabled(!busy);
    m_pPortEdit->setEnabled(!busy);
    m_pNameEdit->setEnabled(!busy);
    m_pPasswordEdit->setEnabled(!busy);
    m_pRegisterBtn->setEnabled(!busy);
}

/*
 * 功能描述:  执行 showError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  message  参数。
 * return:    void
 */
void CLoginPage::showError(const QString& message) {
    m_pErrorLabel->setText(message);
    m_pErrorLabel->setVisible(true);
}

/*
 * 功能描述:  执行 clearError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CLoginPage::clearError() {
    m_pErrorLabel->clear();
    m_pErrorLabel->setVisible(false);
}

/*
 * 功能描述:  执行 host 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
QString CLoginPage::host() const {
    return m_pHostEdit != nullptr ? m_pHostEdit->text().trimmed() : QString();
}

/*
 * 功能描述:  执行 port 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
quint16 CLoginPage::port() const {
    if (m_pPortEdit == nullptr) return 0;
    bool ok = false;
    const uint value = m_pPortEdit->text().toUInt(&ok);
    if (!ok || value < 1) return 0;
    return static_cast<quint16>(qMin(value, 65535u));
}

/*
 * 功能描述:  执行 setCredentials 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  name  参数。
 * param [in]  password  参数。
 * return:    void
 */
void CLoginPage::setCredentials(const QString& name, const QString& password) {
    if (m_pNameEdit != nullptr) m_pNameEdit->setText(name);
    if (m_pPasswordEdit != nullptr) m_pPasswordEdit->setText(password);
    if (m_pPasswordEdit != nullptr) m_pPasswordEdit->setFocus();
    clearError();
}
