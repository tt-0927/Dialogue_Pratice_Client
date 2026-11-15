/**
 * @file       HomePage.cpp
 * @brief      CHomePage 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释；
 *             2026-11-15 JaTin 改为历史房间列表 + “＋”进房 + 单词练习占位。
 */
#include "ui/HomePage.h"

#include "core/UiMetrics.h"

#include "ui/widgets/Avatar.h"
#include "core/Theme.h"

#include <QDateTime>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
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
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
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
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pLabel = new QLabel(pParent);
    pLabel->setObjectName("ValueLabel");
    pLabel->setWordWrap(true);
    pLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return pLabel;
}

/**
 * @brief      把毫秒时间戳格式化成简短展示文本。
 * @param[in] lTs            Unix 毫秒时间戳，0 表示没有消息；类型 qint64。
 * @return     当天显示 HH:mm，否则显示 M-d HH:mm；无消息返回固定提示。
 * @author     JaTin
 */
QString formatTime(qint64 lTs) {
    if (lTs <= 0) return QStringLiteral("暂无消息");
    const QDateTime stTime = QDateTime::fromMSecsSinceEpoch(lTs);
    const QDateTime stNow = QDateTime::currentDateTime();
    if (stTime.date() == stNow.date()) return stTime.toString(QStringLiteral("HH:mm"));
    return stTime.toString(QStringLiteral("M-d HH:mm"));
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

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(0, 0, 0, 0);
    pRoot->setSpacing(0);

    /* ---- 顶栏 ---- */
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
    auto* pBody = new QWidget(this);
    auto* pBodyLayout = new QVBoxLayout(pBody);
    pBodyLayout->setContentsMargins(24, 28, 24, 28);
    pBodyLayout->setSpacing(0);

    auto* pCard = new QFrame(pBody);
    m_pCard = pCard;
    pCard->setObjectName("Card");
    /* 初始值，resizeEvent 会再校准。 */
    pCard->setFixedWidth(core::ui::cardWidth(width(), 560));

    auto* pShadow = new QGraphicsDropShadowEffect(pCard);
    pShadow->setBlurRadius(36);
    pShadow->setOffset(0, 10);
    pShadow->setColor(QColor(16, 24, 40, 26));
    pCard->setGraphicsEffect(pShadow);

    auto* pCardLayout = new QVBoxLayout(pCard);
    pCardLayout->setContentsMargins(32, 28, 32, 28);
    pCardLayout->setSpacing(16);

    /* 头像 + 姓名。 */
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

    /* 资料区。 */
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

    /* ---- 对话练习：历史房间 + “＋”进房 ---- */
    auto* pSectionRow = new QHBoxLayout();
    pSectionRow->setSpacing(8);
    auto* pSectionTitle = new QLabel(QStringLiteral("对话练习"), pCard);
    pSectionTitle->setObjectName("SectionTitle");
    pSectionRow->addWidget(pSectionTitle);
    pSectionRow->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAddBtn = new QPushButton(QStringLiteral("＋"), pCard);
    m_pAddBtn->setObjectName("GhostButton");
    m_pAddBtn->setCursor(Qt::PointingHandCursor);
    m_pAddBtn->setToolTip(QStringLiteral("输入房间号加入新房间"));
    m_pAddBtn->setFixedSize(36, 36);
    pSectionRow->addWidget(m_pAddBtn);
    pCardLayout->addLayout(pSectionRow);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomList = new QListWidget(pCard);
    m_pRoomList->setObjectName("RoomList");
    m_pRoomList->setUniformItemSizes(true);
    m_pRoomList->setWordWrap(false);
    m_pRoomList->setAlternatingRowColors(false);
    /* 最大可滚动高度：列表长了往下滚，不把主页撑开。 */
    m_pRoomList->setMinimumHeight(64);
    m_pRoomList->setMaximumHeight(240);
    pCardLayout->addWidget(m_pRoomList);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pEmptyHint = new QLabel(
        QStringLiteral("还没有练习记录，点右上角“＋”输入房间号开始对话练习。"), pCard);
    m_pEmptyHint->setObjectName("HintLabel");
    m_pEmptyHint->setWordWrap(true);
    m_pEmptyHint->setVisible(true);
    pCardLayout->addWidget(m_pEmptyHint);

    /* ---- 单词练习：占位入口 ---- */
    auto* pWordDivider = new QFrame(pCard);
    pWordDivider->setObjectName("Divider");
    pWordDivider->setFrameShape(QFrame::HLine);
    pCardLayout->addWidget(pWordDivider);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pWordBtn = new QPushButton(QStringLiteral("单词练习（敬请期待）"), pCard);
    m_pWordBtn->setObjectName("GhostButton");
    m_pWordBtn->setEnabled(false);
    m_pWordBtn->setMinimumHeight(40);
    pCardLayout->addWidget(m_pWordBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pErrorLabel = new QLabel(pCard);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    pCardLayout->addWidget(m_pErrorLabel);

    pBodyLayout->addWidget(pCard, 0, Qt::AlignHCenter);
    pBodyLayout->addStretch(1);
    pRoot->addWidget(pBody, 1);

    connect(pEditBtn, &QPushButton::clicked, this, &CHomePage::editProfileRequested);
    connect(pLogoutBtn, &QPushButton::clicked, this, &CHomePage::logoutRequested);
    connect(m_pAddBtn, &QPushButton::clicked, this, &CHomePage::onAddRoomClicked);
    connect(m_pRoomList, &QListWidget::itemClicked, this, &CHomePage::onRoomItemClicked);
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

    /* 窄屏顶栏塞不下「标题 + 连接状态 + 退出登录」三样， 手机端用户本来就不关心服务器地址，直接隐藏连接状态。 */
    if (m_pServerLabel != nullptr) {
        m_pServerLabel->setVisible(!core::ui::isCompact(width()));
    }
}

/**
 * @brief      设置个人资料并刷新头像。
 * @param[in] lId            编号；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @param[in] byAvatar       头像图片二进制，空表示无自定义头像；类型 const QByteArray&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::setProfile(qint64 lId, const QString& strName, int nAge, const QString& strDesc,
                           const QByteArray& byAvatar) {
    m_pAvatar->setText(strName);
    m_pAvatar->setColor(CAvatar::colorForId(lId));
    m_pAvatar->setImage(byAvatar);
    m_pNameLabel->setText(strName);
    m_pIdLabel->setText(QStringLiteral("账号 ID %1").arg(lId));
    m_pNameValue->setText(strName);
    m_pAgeValue->setText(nAge > 0 ? QStringLiteral("%1 岁").arg(nAge)
                               : QStringLiteral("未填写"));
    m_pDescValue->setText(strDesc.isEmpty() ? QStringLiteral("还没有填写个人描述")
                                       : strDesc);
}

/**
 * @brief      设置历史房间列表；空列表时显示占位提示。
 * @param[in] aRooms         历史房间，按最近访问时间倒序；类型 const QVector<VisitedRoom_S>&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::setHistoryRooms(const QVector<VisitedRoom_S>& aRooms) {
    m_pRoomList->clear();
    for (const VisitedRoom_S& stRoom : aRooms) {
        const QString strPreview = stRoom.lastMessage.isEmpty()
            ? QStringLiteral("暂无消息")
            : (stRoom.lastSender.isEmpty() ? stRoom.lastMessage
                                           : QStringLiteral("%1：%2").arg(stRoom.lastSender,
                                                                          stRoom.lastMessage));
        const QString strText = QStringLiteral("房间 %1    %2    %3")
                                    .arg(stRoom.room, formatTime(stRoom.lastTs), strPreview);
        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        auto* pItem = new QListWidgetItem(strText, m_pRoomList);
        pItem->setData(Qt::UserRole, stRoom.room);
        pItem->setToolTip(QStringLiteral("点击重新进入房间 %1").arg(stRoom.room));
    }
    m_pEmptyHint->setVisible(m_pRoomList->count() == 0);
    m_pRoomList->setVisible(m_pRoomList->count() > 0);
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
 * @brief      点击“＋”弹出输入框，校验后请求进房。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::onAddRoomClicked() {
    clearError();
    bool nOk = false;
    const QString strRoom = QInputDialog::getText(
        this, QStringLiteral("加入房间"),
        QStringLiteral("房间号（1..32 位字母 / 数字 / 下划线 / 短横线，不存在时自动创建）："),
        QLineEdit::Normal, QString(), &nOk).trimmed();
    if (!nOk) return;
    if (strRoom.isEmpty()) {
        showError(QStringLiteral("房间号不能为空"));
        return;
    }
    static const QRegularExpression stPattern(QStringLiteral("^[A-Za-z0-9_-]{1,32}$"));
    if (!stPattern.match(strRoom).hasMatch()) {
        showError(QStringLiteral("房间号需为 1..32 位字母、数字、下划线或短横线"));
        return;
    }
    emit joinRoomRequested(strRoom);
}

/**
 * @brief      点击历史房间条目请求再次进房。
 * @param[in] pItem          被点击的条目；类型 QListWidgetItem*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::onRoomItemClicked(QListWidgetItem* pItem) {
    if (pItem == nullptr) return;
    clearError();
    const QString strRoom = pItem->data(Qt::UserRole).toString();
    if (strRoom.isEmpty()) return;
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
    m_pAddBtn->setEnabled(!nBusy);
    m_pRoomList->setEnabled(!nBusy);
}

/**
 * @brief      聚焦历史房间列表。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CHomePage::focusRoomInput() {
    if (m_pRoomList->count() > 0) {
        m_pRoomList->setCurrentRow(0);
        m_pRoomList->setFocus();
    } else {
        m_pAddBtn->setFocus();
    }
}
