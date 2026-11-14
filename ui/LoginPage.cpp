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
 * 功能描述:  构造 CLoginPage 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QWidget(。
 * return:    无返回值（构造函数或析构函数）。
 */
CLoginPage::CLoginPage(QWidget* pParent) : QWidget(pParent) {
    setObjectName("AppRoot");

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(24, 24, 24, 24);
    pRoot->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pCard = new QFrame(this);
    m_pCard = pCard;
    pCard->setObjectName("Card");
    /* 初始值，resizeEvent 会再校准 */
    pCard->setFixedWidth(core::ui::cardWidth(width()));

    auto* pShadow = new QGraphicsDropShadowEffect(pCard);
    pShadow->setBlurRadius(36);
    pShadow->setOffset(0, 10);
    pShadow->setColor(QColor(16, 24, 40, 30));
    pCard->setGraphicsEffect(pShadow);

    auto* pCardLayout = new QVBoxLayout(pCard);
    pCardLayout->setContentsMargins(32, 32, 32, 28);
    pCardLayout->setSpacing(10);

    auto* pLogo = new CAvatar(pCard);
    pLogo->setFixedSize(64, 64);
    pLogo->setFontPx(28);
    pLogo->setText(QStringLiteral("D"));
    pLogo->setColor(theme::accent);

    auto* pLogoRow = new QHBoxLayout();
    pLogoRow->addStretch(1);
    pLogoRow->addWidget(pLogo);
    pLogoRow->addStretch(1);
    pCardLayout->addLayout(pLogoRow);

    auto* pTitle = new QLabel(QStringLiteral("Dialogue Pratice"), pCard);
    pTitle->setObjectName("BigTitle");
    pTitle->setAlignment(Qt::AlignCenter);
    pCardLayout->addWidget(pTitle);

    auto* pSubtitle = new QLabel(QStringLiteral("登录后即可用房间号加入对话"), pCard);
    pSubtitle->setObjectName("SubTitle");
    pSubtitle->setAlignment(Qt::AlignCenter);
    pCardLayout->addWidget(pSubtitle);

    pCardLayout->addSpacing(14);

    auto* pServerLabel = new QLabel(QStringLiteral("服务器"), pCard);
    pServerLabel->setObjectName("KeyLabel");
    pCardLayout->addWidget(pServerLabel);

    auto* pServerRow = new QHBoxLayout();
    pServerRow->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pHostEdit = new QLineEdit(pCard);
    m_pHostEdit->setObjectName("Field");
    m_pHostEdit->setPlaceholderText(QStringLiteral("192.168.80.128"));
    pServerRow->addWidget(m_pHostEdit, 1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPortEdit = new QLineEdit(pCard);
    m_pPortEdit->setObjectName("Field");
    m_pPortEdit->setPlaceholderText(QStringLiteral("9000"));
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPortEdit->setValidator(new QIntValidator(1, 65535, m_pPortEdit));
    m_pPortEdit->setAlignment(Qt::AlignCenter);
    m_pPortEdit->setFixedWidth(88);
    m_pPortEdit->setToolTip(QStringLiteral("服务器端口，默认 9000"));
    pServerRow->addWidget(m_pPortEdit, 0);
    pCardLayout->addLayout(pServerRow);

    auto* pNameLabel = new QLabel(QStringLiteral("昵称"), pCard);
    pNameLabel->setObjectName("KeyLabel");
    pCardLayout->addWidget(pNameLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNameEdit = new QLineEdit(pCard);
    m_pNameEdit->setObjectName("Field");
    m_pNameEdit->setPlaceholderText(QStringLiteral("例如 alice，最多 24 字节"));
    m_pNameEdit->setMaxLength(24);
    pCardLayout->addWidget(m_pNameEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPasswordEdit = new QLineEdit(pCard);
    m_pPasswordEdit->setObjectName("Field");
    m_pPasswordEdit->setEchoMode(QLineEdit::Password);
    m_pPasswordEdit->setPlaceholderText(
        QStringLiteral("密码（留空则按昵称匿名登录）"));
    pCardLayout->addWidget(m_pPasswordEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pErrorLabel = new QLabel(pCard);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    pCardLayout->addWidget(m_pErrorLabel);

    pCardLayout->addSpacing(4);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pLoginBtn = new QPushButton(QStringLiteral("登  录"), pCard);
    m_pLoginBtn->setObjectName("PrimaryButton");
    m_pLoginBtn->setCursor(Qt::PointingHandCursor);
    m_pLoginBtn->setDefault(true);
    m_pLoginBtn->setMinimumHeight(44);
    pCardLayout->addWidget(m_pLoginBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRegisterBtn = new QPushButton(QStringLiteral("还没有账号？注册新账号"), pCard);
    m_pRegisterBtn->setObjectName("LinkButton");
    m_pRegisterBtn->setCursor(Qt::PointingHandCursor);
    m_pRegisterBtn->setFlat(true);
    pCardLayout->addWidget(m_pRegisterBtn, 0, Qt::AlignHCenter);

    pRoot->addWidget(pCard, 0, Qt::AlignHCenter);
    pRoot->addStretch(1);

    /* 恢复上次填写的服务器与昵称 */
    QSettings aSettings;
    m_pHostEdit->setText(aSettings.value("server/host", QStringLiteral("192.168.80.128")).toString());
    m_pPortEdit->setText(aSettings.value("server/port", 9000).toString());
    m_pNameEdit->setText(aSettings.value("login/name").toString());

    connect(m_pLoginBtn, &QPushButton::clicked, this, &CLoginPage::submit);
    connect(m_pNameEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pPasswordEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pHostEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pPortEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pRegisterBtn, &QPushButton::clicked, this, &CLoginPage::registerRequested);
}

/*
 * 功能描述:  重新计算尺寸事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 QResizeEvent*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLoginPage::resizeEvent(QResizeEvent* pEvent) {
    QWidget::resizeEvent(pEvent);
    if (m_pCard == nullptr) return;

    /* 只在宽度真的变了才动，避免 setFixedWidth 触发的递归布局 */
    const int nTarget = core::ui::cardWidth(width());
    if (m_pCard->width() != nTarget) m_pCard->setFixedWidth(nTarget);
}

/*
 * 功能描述:  提交submit；实现上读写本地持久化设置。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLoginPage::submit() {
    clearError();

    const QString strHost = m_pHostEdit->text().trimmed();
    const QString strName = m_pNameEdit->text().trimmed();

    if (strHost.isEmpty()) {
        showError(QStringLiteral("请填写服务器地址"));
        m_pHostEdit->setFocus();
        return;
    }
    if (strName.isEmpty()) {
        showError(QStringLiteral("请填写昵称"));
        m_pNameEdit->setFocus();
        return;
    }
    if (strName.toUtf8().size() > 24) {
        showError(QStringLiteral("昵称最多 24 字节（一个汉字约 3 字节）"));
        m_pNameEdit->setFocus();
        return;
    }

    bool nPortOk = false;
    const int nPortValue = m_pPortEdit->text().toInt(&nPortOk);
    if (!nPortOk || nPortValue < 1 || nPortValue > 65535) {
        showError(QStringLiteral("端口必须是 1–65535 之间的数字"));
        m_pPortEdit->setFocus();
        m_pPortEdit->selectAll();
        return;
    }

    QSettings aSettings;
    aSettings.setValue("server/host", strHost);
    aSettings.setValue("server/port", nPortValue);
    aSettings.setValue("login/name", strName);

    emit loginRequested(strHost, static_cast<quint16>(nPortValue), strName,
                        m_pPasswordEdit->text());
}

/*
 * 功能描述:  设置busy。
 * 作者:      JaTin
 * param [in]   nBusy            输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLoginPage::setBusy(bool nBusy) {
    m_pLoginBtn->setEnabled(!nBusy);
    m_pLoginBtn->setText(nBusy ? QStringLiteral("连接中…") : QStringLiteral("登  录"));
    m_pHostEdit->setEnabled(!nBusy);
    m_pPortEdit->setEnabled(!nBusy);
    m_pNameEdit->setEnabled(!nBusy);
    m_pPasswordEdit->setEnabled(!nBusy);
    m_pRegisterBtn->setEnabled(!nBusy);
}

/*
 * 功能描述:  显示错误。
 * 作者:      JaTin
 * param [in]   strMessage       消息内容，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLoginPage::showError(const QString& strMessage) {
    m_pErrorLabel->setText(strMessage);
    m_pErrorLabel->setVisible(true);
}

/*
 * 功能描述:  清空错误。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLoginPage::clearError() {
    m_pErrorLabel->clear();
    m_pErrorLabel->setVisible(false);
}

/*
 * 功能描述:  获取主机地址。
 * 作者:      JaTin
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString CLoginPage::host() const {
    return m_pHostEdit != nullptr ? m_pHostEdit->text().trimmed() : QString();
}

/*
 * 功能描述:  获取端口。
 * 作者:      JaTin
 * return:    返回计数或长度；无有效数据时返回 0。
 */
quint16 CLoginPage::port() const {
    if (m_pPortEdit == nullptr) return 0;
    bool nOk = false;
    const uint stValue = m_pPortEdit->text().toUInt(&nOk);
    if (!nOk || stValue < 1) return 0;
    return static_cast<quint16>(qMin(stValue, 65535u));
}

/*
 * 功能描述:  设置credentials。
 * 作者:      JaTin
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strPassword      登录密码，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLoginPage::setCredentials(const QString& strName, const QString& strPassword) {
    if (m_pNameEdit != nullptr) m_pNameEdit->setText(strName);
    if (m_pPasswordEdit != nullptr) m_pPasswordEdit->setText(strPassword);
    if (m_pPasswordEdit != nullptr) m_pPasswordEdit->setFocus();
    clearError();
}
