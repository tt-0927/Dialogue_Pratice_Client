/**
 * @file       HomePage.cpp
 * @brief      CHomePage 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CHomePage.cpp。
 */
#include "ui/HomePage.h"

#include "core/UiMetrics.h"

#include "ui/widgets/Avatar.h"
#include "core/Theme.h"

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

/**
 * @brief      构造密钥标签；实现上创建 Qt 控件并纳入父子对象树。
 * @param[in] strText        消息文本；类型 const QString&。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
 * @return     成功返回对象指针；未找到或失败时返回 nullptr。
 * @author     JaTin
 */
QLabel* makeKeyLabel(const QString& strText, QWidget* pParent) {
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pLabel = new QLabel(strText, pParent);
    pLabel->setObjectName("KeyLabel");
    pLabel->setFixedWidth(76);
    pLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return pLabel;
}

/**
 * @brief      构造数值标签；实现上创建 Qt 控件并纳入父子对象树。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
 * @return     成功返回对象指针；未找到或失败时返回 nullptr。
 * @author     JaTin
 */
QLabel* makeValueLabel(QWidget* pParent) {
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pLabel = new QLabel(pParent);
    pLabel->setObjectName("ValueLabel");
    pLabel->setWordWrap(true);
    pLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return pLabel;
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CHomePage 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QWidget(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CHomePage::CHomePage(QWidget* pParent) : QWidget(pParent) {
    setObjectName("AppRoot");

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(0, 0, 0, 0);
    pRoot->setSpacing(0);

    /**
     * @brief  ---- 顶栏 ---- 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
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

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pServerLabel = new QLabel(pTopBar);
    m_pServerLabel->setObjectName("HintLabel");
    pTopLayout->addWidget(m_pServerLabel);

    auto* pLogoutBtn = new QPushButton(QStringLiteral("退出登录"), pTopBar);
    pLogoutBtn->setObjectName("LinkButton");
    pLogoutBtn->setCursor(Qt::PointingHandCursor);
    pTopLayout->addWidget(pLogoutBtn);

    pRoot->addWidget(pTopBar);

    /**
     * @brief  ---- 主体 ---- 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pBody = new QWidget(this);
    auto* pBodyLayout = new QVBoxLayout(pBody);
    pBodyLayout->setContentsMargins(24, 28, 24, 28);
    pBodyLayout->addStretch(1);

    auto* pCard = new QFrame(pBody);
    m_pCard = pCard;
    pCard->setObjectName("Card");
    /**
     * @brief  初始值，resizeEvent 会再校准。
     */
    pCard->setFixedWidth(core::ui::cardWidth(width(), 560));

    auto* pShadow = new QGraphicsDropShadowEffect(pCard);
    pShadow->setBlurRadius(36);
    pShadow->setOffset(0, 10);
    pShadow->setColor(QColor(16, 24, 40, 26));
    pCard->setGraphicsEffect(pShadow);

    auto* pCardLayout = new QVBoxLayout(pCard);
    pCardLayout->setContentsMargins(32, 28, 32, 28);
    pCardLayout->setSpacing(16);

    /**
     * @brief  头像 + 姓名。
     */
    auto* pHeaderRow = new QHBoxLayout();
    pHeaderRow->setSpacing(16);

    m_pAvatar = new CAvatar(pCard);
    m_pAvatar->setFixedSize(72, 72);
    m_pAvatar->setFontPx(30);
    pHeaderRow->addWidget(m_pAvatar, 0, Qt::AlignTop);

    auto* pNameColumn = new QVBoxLayout();
    pNameColumn->setSpacing(4);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pNameLabel = new QLabel(pCard);
    m_pNameLabel->setStyleSheet(QStringLiteral("font-size: 18pt; font-weight: 700;"));
    pNameColumn->addWidget(m_pNameLabel);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pIdLabel = new QLabel(pCard);
    m_pIdLabel->setObjectName("SubTitle");
    pNameColumn->addWidget(m_pIdLabel);
    pNameColumn->addStretch(1);

    pHeaderRow->addLayout(pNameColumn, 1);
    pCardLayout->addLayout(pHeaderRow);

    /**
     * @brief  资料区。
     */
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

    /**
     * @brief  房间区。
     */
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

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pRoomEdit = new QLineEdit(pCard);
    m_pRoomEdit->setObjectName("Field");
    m_pRoomEdit->setPlaceholderText(QStringLiteral("房间号，例如 1001"));
    m_pRoomEdit->setMaxLength(32);
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pRoomEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9_-]{0,32}")), m_pRoomEdit));
    m_pRoomEdit->setMinimumHeight(42);
    pRoomRow->addWidget(m_pRoomEdit, 1);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pJoinBtn = new QPushButton(QStringLiteral("进入房间"), pCard);
    m_pJoinBtn->setObjectName("PrimaryButton");
    m_pJoinBtn->setCursor(Qt::PointingHandCursor);
    m_pJoinBtn->setMinimumHeight(42);
    m_pJoinBtn->setMinimumWidth(120);
    pRoomRow->addWidget(m_pJoinBtn, 0);

    pCardLayout->addLayout(pRoomRow);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
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

/**
 * @brief      重新计算尺寸事件。
 * @param[in] pEvent         事件；类型 QResizeEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::resizeEvent(QResizeEvent* pEvent) {
    QWidget::resizeEvent(pEvent);
    if (m_pCard == nullptr) return;

    const int nTarget = core::ui::cardWidth(width(), 560);
    if (m_pCard->width() != nTarget) m_pCard->setFixedWidth(nTarget);

    /**
     * @brief  窄屏顶栏塞不下「标题 + 连接状态 + 退出登录」三样， 不处理的话标题会被裁成「Dialogue Pr」这种半截文本。 手机端用户本来就不关心服务器地址，直接隐藏连接状态。
     */
    if (m_pServerLabel != nullptr) {
        m_pServerLabel->setVisible(!core::ui::isCompact(width()));
    }
}

/**
 * @brief      设置个人资料。
 * @param[in] lId            编号；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief      设置服务端文本。
 * @param[in] strText        消息文本；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::setServerText(const QString& strText) {
    m_pServerLabel->setText(strText);
}

/**
 * @brief      提交房间。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief      显示错误。
 * @param[in] strMessage     消息内容；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::showError(const QString& strMessage) {
    m_pErrorLabel->setText(strMessage);
    m_pErrorLabel->setVisible(true);
}

/**
 * @brief      清空错误。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::clearError() {
    m_pErrorLabel->clear();
    m_pErrorLabel->setVisible(false);
}

/**
 * @brief      设置 busy相关的数据并返回处理结果。
 * @param[in] nBusy          输入数据；类型 bool。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::setBusy(bool nBusy) {
    m_pJoinBtn->setEnabled(!nBusy);
    m_pJoinBtn->setText(nBusy ? QStringLiteral("进入中…") : QStringLiteral("进入房间"));
    m_pRoomEdit->setEnabled(!nBusy);
}

/**
 * @brief      聚焦房间输入。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::focusRoomInput() {
    m_pRoomEdit->setFocus();
    m_pRoomEdit->selectAll();
}
