/*
 * 文件名:    HomePage.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CHomePage 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CHomePage.cpp */
#include "HomePage.h"

#include "Avatar.h"
#include "Theme.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace {

/*
 * 功能描述:  构造密钥标签；实现上创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget*。
 * return:    成功返回对象指针；未找到或失败时返回 nullptr。
 */
QLabel* makeKeyLabel(const QString& strText, QWidget* pParent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pLabel = new QLabel(strText, pParent);
    pLabel->setObjectName("KeyLabel");
    pLabel->setFixedWidth(76);
    pLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return pLabel;
}

/*
 * 功能描述:  构造数值标签；实现上创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget*。
 * return:    成功返回对象指针；未找到或失败时返回 nullptr。
 */
QLabel* makeValueLabel(QWidget* pParent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pLabel = new QLabel(pParent);
    pLabel->setObjectName("ValueLabel");
    pLabel->setWordWrap(true);
    pLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return pLabel;
}

/* namespace */
}

/*
 * 功能描述:  构造 CHomePage 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QWidget(。
 * return:    无返回值（构造函数或析构函数）。
 */
CHomePage::CHomePage(QWidget* pParent) : QWidget(pParent) {
    setObjectName("AppRoot");

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(0, 0, 0, 0);
    pRoot->setSpacing(0);

    /* ---- 顶栏 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pTopBar = new QFrame(this);
    pTopBar->setObjectName("TopBar");
    pTopBar->setFixedHeight(60);
    auto* pTopLayout = new QHBoxLayout(pTopBar);
    pTopLayout->setContentsMargins(24, 0, 24, 0);
    pTopLayout->setSpacing(12);

    auto* pAppTitle = new QLabel(QStringLiteral("Dialogue Pratice"), pTopBar);
    pAppTitle->setObjectName("AppTitle");
    pTopLayout->addWidget(pAppTitle);
    pTopLayout->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pServerLabel = new QLabel(pTopBar);
    m_pServerLabel->setObjectName("HintLabel");
    pTopLayout->addWidget(m_pServerLabel);

    auto* pLogoutBtn = new QPushButton(QStringLiteral("退出登录"), pTopBar);
    pLogoutBtn->setObjectName("LinkButton");
    pLogoutBtn->setCursor(Qt::PointingHandCursor);
    pTopLayout->addWidget(pLogoutBtn);

    pRoot->addWidget(pTopBar);

    /* ---- 主体 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pBody = new QWidget(this);
    auto* pBodyLayout = new QVBoxLayout(pBody);
    pBodyLayout->setContentsMargins(24, 28, 24, 28);
    pBodyLayout->addStretch(1);

    auto* pCard = new QFrame(pBody);
    pCard->setObjectName("Card");
    pCard->setFixedWidth(560);

    auto* pShadow = new QGraphicsDropShadowEffect(pCard);
    pShadow->setBlurRadius(36);
    pShadow->setOffset(0, 10);
    pShadow->setColor(QColor(16, 24, 40, 26));
    pCard->setGraphicsEffect(pShadow);

    auto* pCardLayout = new QVBoxLayout(pCard);
    pCardLayout->setContentsMargins(32, 28, 32, 28);
    pCardLayout->setSpacing(16);

    /* 头像 + 姓名 */
    auto* pHeaderRow = new QHBoxLayout();
    pHeaderRow->setSpacing(16);

    m_pAvatar = new CAvatar(pCard);
    m_pAvatar->setFixedSize(72, 72);
    m_pAvatar->setFontPx(30);
    pHeaderRow->addWidget(m_pAvatar, 0, Qt::AlignTop);

    auto* pNameColumn = new QVBoxLayout();
    pNameColumn->setSpacing(4);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNameLabel = new QLabel(pCard);
    m_pNameLabel->setStyleSheet(QStringLiteral("font-size: 18pt; font-weight: 700;"));
    pNameColumn->addWidget(m_pNameLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pIdLabel = new QLabel(pCard);
    m_pIdLabel->setObjectName("SubTitle");
    pNameColumn->addWidget(m_pIdLabel);
    pNameColumn->addStretch(1);

    pHeaderRow->addLayout(pNameColumn, 1);
    pCardLayout->addLayout(pHeaderRow);

    /* 资料区 */
    auto* pInfoBox = new QFrame(pCard);
    pInfoBox->setObjectName("InfoBox");
    auto* pGrid = new QGridLayout(pInfoBox);
    pGrid->setContentsMargins(18, 14, 18, 14);
    pGrid->setHorizontalSpacing(12);
    pGrid->setVerticalSpacing(10);

    m_pNameValue = makeValueLabel(pInfoBox);
    m_pAgeValue = makeValueLabel(pInfoBox);
    m_pDescValue = makeValueLabel(pInfoBox);

    pGrid->addWidget(makeKeyLabel(QStringLiteral("姓名"), pInfoBox), 0, 0);
    pGrid->addWidget(m_pNameValue, 0, 1);
    pGrid->addWidget(makeKeyLabel(QStringLiteral("年龄"), pInfoBox), 1, 0);
    pGrid->addWidget(m_pAgeValue, 1, 1);
    pGrid->addWidget(makeKeyLabel(QStringLiteral("个人描述"), pInfoBox), 2, 0);
    pGrid->addWidget(m_pDescValue, 2, 1);
    pGrid->setColumnStretch(1, 1);

    pCardLayout->addWidget(pInfoBox);

    auto* pEditBtn = new QPushButton(QStringLiteral("编辑个人信息"), pCard);
    pEditBtn->setObjectName("GhostButton");
    pEditBtn->setCursor(Qt::PointingHandCursor);
    pEditBtn->setMinimumHeight(40);
    pCardLayout->addWidget(pEditBtn);

    auto* pDivider = new QFrame(pCard);
    pDivider->setObjectName("Divider");
    pDivider->setFrameShape(QFrame::HLine);
    pCardLayout->addWidget(pDivider);

    /* 房间区 */
    auto* pSectionTitle = new QLabel(QStringLiteral("加入房间"), pCard);
    pSectionTitle->setObjectName("SectionTitle");
    pCardLayout->addWidget(pSectionTitle);

    auto* pSectionHint = new QLabel(
        QStringLiteral("输入房间号即可加入；房间不存在时会自动创建。"), pCard);
    pSectionHint->setObjectName("HintLabel");
    pSectionHint->setWordWrap(true);
    pCardLayout->addWidget(pSectionHint);

    auto* pRoomRow = new QHBoxLayout();
    pRoomRow->setSpacing(10);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomEdit = new QLineEdit(pCard);
    m_pRoomEdit->setObjectName("Field");
    m_pRoomEdit->setPlaceholderText(QStringLiteral("房间号，例如 1001"));
    m_pRoomEdit->setMaxLength(32);
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9_-]{0,32}")), m_pRoomEdit));
    m_pRoomEdit->setMinimumHeight(42);
    pRoomRow->addWidget(m_pRoomEdit, 1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pJoinBtn = new QPushButton(QStringLiteral("进入房间"), pCard);
    m_pJoinBtn->setObjectName("PrimaryButton");
    m_pJoinBtn->setCursor(Qt::PointingHandCursor);
    m_pJoinBtn->setMinimumHeight(42);
    m_pJoinBtn->setMinimumWidth(120);
    pRoomRow->addWidget(m_pJoinBtn, 0);

    pCardLayout->addLayout(pRoomRow);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pErrorLabel = new QLabel(pCard);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    pCardLayout->addWidget(m_pErrorLabel);

    pBodyLayout->addWidget(pCard, 0, Qt::AlignHCenter);
    pBodyLayout->addStretch(2);
    pRoot->addWidget(pBody, 1);

    connect(pEditBtn, &QPushButton::clicked, this, &CHomePage::editProfileRequested);
    connect(m_pJoinBtn, &QPushButton::clicked, this, &CHomePage::submitRoom);
    connect(m_pRoomEdit, &QLineEdit::returnPressed, this, &CHomePage::submitRoom);
    connect(pLogoutBtn, &QPushButton::clicked, this, &CHomePage::logoutRequested);
}

/*
 * 功能描述:  设置个人资料。
 * 作者:      JaTin
 * param [in]   lId              编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CHomePage::setProfile(qint64 lId, const QString& strName, int nAge, const QString& strDesc) {
    m_pAvatar->setText(strName);
    m_pAvatar->setColor(CAvatar::colorForId(lId));
    m_pNameLabel->setText(strName);
    m_pIdLabel->setText(QStringLiteral("账号 ID %1").arg(lId));
    m_pNameValue->setText(strName);
    m_pAgeValue->setText(nAge > 0 ? QStringLiteral("%1 岁").arg(nAge)
                               : QStringLiteral("未填写"));
    m_pDescValue->setText(strDesc.isEmpty() ? QStringLiteral("还没有填写个人描述")
                                       : strDesc);
}

/*
 * 功能描述:  设置服务端文本。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CHomePage::setServerText(const QString& strText) {
    m_pServerLabel->setText(strText);
}

/*
 * 功能描述:  提交房间。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CHomePage::submitRoom() {
    clearError();
    const QString strRoom = m_pRoomEdit->text().trimmed();
    if (strRoom.isEmpty()) {
        showError(QStringLiteral("请先输入房间号"));
        m_pRoomEdit->setFocus();
        return;
    }
    emit joinRoomRequested(strRoom);
}

/*
 * 功能描述:  显示错误。
 * 作者:      JaTin
 * param [in]   strMessage       消息内容，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CHomePage::showError(const QString& strMessage) {
    m_pErrorLabel->setText(strMessage);
    m_pErrorLabel->setVisible(true);
}

/*
 * 功能描述:  清空错误。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CHomePage::clearError() {
    m_pErrorLabel->clear();
    m_pErrorLabel->setVisible(false);
}

/*
 * 功能描述:  设置busy。
 * 作者:      JaTin
 * param [in]   nBusy            输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CHomePage::setBusy(bool nBusy) {
    m_pJoinBtn->setEnabled(!nBusy);
    m_pJoinBtn->setText(nBusy ? QStringLiteral("进入中…") : QStringLiteral("进入房间"));
    m_pRoomEdit->setEnabled(!nBusy);
}

/*
 * 功能描述:  聚焦房间输入。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CHomePage::focusRoomInput() {
    m_pRoomEdit->setFocus();
    m_pRoomEdit->selectAll();
}
