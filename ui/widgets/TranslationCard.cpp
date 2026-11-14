/**
 * @file       TranslationCard.cpp
 * @brief      CTranslationCard 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CTranslationCard.cpp。
 */
#include "ui/widgets/TranslationCard.h"

#include "ui/widgets/PlayButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

/**
 * @brief      构造 CTranslationCard 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QFrame(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CTranslationCard::CTranslationCard(QWidget* pParent) : QFrame(pParent) {
    setObjectName(QStringLiteral("CTranslationCard"));
    setVisible(false);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(14, 10, 14, 10);
    pRoot->setSpacing(4);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pTitle = new QLabel(QStringLiteral("译文"), this);
    m_pTitle->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    pRoot->addWidget(m_pTitle);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pBody = new QLabel(this);
    m_pBody->setWordWrap(true);
    m_pBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pBody->setStyleSheet(QStringLiteral("color:#1F2A37; font-size:11pt;"));
    pRoot->addWidget(m_pBody);

    auto* pButtons = new QHBoxLayout();
    pButtons->setContentsMargins(0, 2, 0, 0);
    pButtons->setSpacing(6);
    pButtons->addStretch(1);

    m_pPlay = new CPlayButton(this);
    pButtons->addWidget(m_pPlay);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pUse = new QPushButton(QStringLiteral("填入输入框"), this);
    m_pUse->setObjectName("LinkButton");
    m_pUse->setCursor(Qt::PointingHandCursor);
    pButtons->addWidget(m_pUse);

    pRoot->addLayout(pButtons);

    connect(m_pPlay, &QAbstractButton::clicked, this, [this] {
        if (!current_.isEmpty()) emit playRequested(current_);
    });
    connect(m_pUse, &QPushButton::clicked, this, [this] {
        if (!current_.isEmpty()) emit useRequested(current_);
    });
}

/**
 * @brief      显示 pending。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTranslationCard::showPending() {
    current_.clear();
    m_pBody->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:10pt;"));
    m_pBody->setText(QStringLiteral("翻译中…"));
    m_pPlay->setEnabled(false);
    m_pUse->setEnabled(false);
    setVisible(true);
}

/**
 * @brief      显示译文。
 * @param[in] strEnglish     输入数据；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTranslationCard::showTranslation(const QString& strEnglish) {
    current_ = strEnglish;
    m_pBody->setStyleSheet(QStringLiteral("color:#1F2A37; font-size:11pt;"));
    m_pBody->setText(strEnglish);
    m_pPlay->setEnabled(true);
    m_pUse->setEnabled(true);
    setVisible(true);
}

/**
 * @brief      设置 playing相关的数据并返回处理结果。
 * @param[in] nPlaying       输入数据；类型 bool。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTranslationCard::setPlaying(bool nPlaying) {
    m_pPlay->setPlaying(nPlaying);
}

/**
 * @brief      隐藏卡片。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTranslationCard::hideCard() {
    current_.clear();
    setVisible(false);
}
