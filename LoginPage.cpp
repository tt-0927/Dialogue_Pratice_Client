/**
 * @file       LoginPage.cpp
 * @brief      CLoginPage 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CLoginPage.cpp。
 */
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

/**
 * @brief      构造 CLoginPage 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QWidget(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CLoginPage::CLoginPage(QWidget* pParent) : QWidget(pParent) {
    setObjectName("AppRoot");

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(24, 24, 24, 24);
    pRoot->addStretch(1);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pCard = new QFrame(this);
    pCard->setObjectName("Card");
    pCard->setFixedWidth(420);

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

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pHostEdit = new QLineEdit(pCard);
    m_pHostEdit->setObjectName("Field");
    m_pHostEdit->setPlaceholderText(QStringLiteral("192.168.80.128"));
    pServerRow->addWidget(m_pHostEdit, 1);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pPortSpin = new QSpinBox(pCard);
    m_pPortSpin->setObjectName("Field");
    m_pPortSpin->setRange(1, 65535);
    m_pPortSpin->setValue(9000);
    m_pPortSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    m_pPortSpin->setFixedWidth(96);
    pServerRow->addWidget(m_pPortSpin, 0);
    pCardLayout->addLayout(pServerRow);

    auto* pNameLabel = new QLabel(QStringLiteral("昵称"), pCard);
    pNameLabel->setObjectName("KeyLabel");
    pCardLayout->addWidget(pNameLabel);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pNameEdit = new QLineEdit(pCard);
    m_pNameEdit->setObjectName("Field");
    m_pNameEdit->setPlaceholderText(QStringLiteral("例如 alice，最多 24 字节"));
    m_pNameEdit->setMaxLength(24);
    pCardLayout->addWidget(m_pNameEdit);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pErrorLabel = new QLabel(pCard);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    pCardLayout->addWidget(m_pErrorLabel);

    pCardLayout->addSpacing(4);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pLoginBtn = new QPushButton(QStringLiteral("登  录"), pCard);
    m_pLoginBtn->setObjectName("PrimaryButton");
    m_pLoginBtn->setCursor(Qt::PointingHandCursor);
    m_pLoginBtn->setDefault(true);
    m_pLoginBtn->setMinimumHeight(44);
    pCardLayout->addWidget(m_pLoginBtn);

    pRoot->addWidget(pCard, 0, Qt::AlignHCenter);
    pRoot->addStretch(1);

    /**
     * @brief  恢复上次填写的服务器与昵称。
     */
    QSettings aSettings;
    m_pHostEdit->setText(aSettings.value("server/host", QStringLiteral("192.168.80.128")).toString());
    m_pPortSpin->setValue(aSettings.value("server/port", 9000).toInt());
    m_pNameEdit->setText(aSettings.value("login/name").toString());

    connect(m_pLoginBtn, &QPushButton::clicked, this, &CLoginPage::submit);
    connect(m_pNameEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
    connect(m_pHostEdit, &QLineEdit::returnPressed, this, &CLoginPage::submit);
}

/**
 * @brief      提交 submit；实现上读写本地持久化设置。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

    QSettings aSettings;
    aSettings.setValue("server/host", strHost);
    aSettings.setValue("server/port", m_pPortSpin->value());
    aSettings.setValue("login/name", strName);

    emit loginRequested(strHost, static_cast<quint16>(m_pPortSpin->value()), strName);
}

/**
 * @brief      设置 busy相关的数据并返回处理结果。
 * @param[in] nBusy          输入数据；类型 bool。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CLoginPage::setBusy(bool nBusy) {
    m_pLoginBtn->setEnabled(!nBusy);
    m_pLoginBtn->setText(nBusy ? QStringLiteral("连接中…") : QStringLiteral("登  录"));
    m_pHostEdit->setEnabled(!nBusy);
    m_pPortSpin->setEnabled(!nBusy);
    m_pNameEdit->setEnabled(!nBusy);
}

/**
 * @brief      显示错误。
 * @param[in] strMessage     消息内容；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CLoginPage::showError(const QString& strMessage) {
    m_pErrorLabel->setText(strMessage);
    m_pErrorLabel->setVisible(true);
}

/**
 * @brief      清空错误。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CLoginPage::clearError() {
    m_pErrorLabel->clear();
    m_pErrorLabel->setVisible(false);
}
