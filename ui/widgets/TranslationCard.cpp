/*
 * 文件名:    TranslationCard.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CTranslationCard 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CTranslationCard.cpp */
#include "ui/widgets/TranslationCard.h"

#include "ui/widgets/PlayButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

/*
 * 功能描述:  构造 CTranslationCard 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QFrame(。
 * return:    无返回值（构造函数或析构函数）。
 */
CTranslationCard::CTranslationCard(QWidget* pParent) : QFrame(pParent) {
    setObjectName(QStringLiteral("CTranslationCard"));
    setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(14, 10, 14, 10);
    pRoot->setSpacing(4);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTitle = new QLabel(QStringLiteral("译文"), this);
    m_pTitle->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    pRoot->addWidget(m_pTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
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

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
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

/*
 * 功能描述:  显示pending。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationCard::showPending() {
    current_.clear();
    m_pBody->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:10pt;"));
    m_pBody->setText(QStringLiteral("翻译中…"));
    m_pPlay->setEnabled(false);
    m_pUse->setEnabled(false);
    setVisible(true);
}

/*
 * 功能描述:  显示译文。
 * 作者:      JaTin
 * param [in]   strEnglish       输入数据，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationCard::showTranslation(const QString& strEnglish) {
    current_ = strEnglish;
    m_pBody->setStyleSheet(QStringLiteral("color:#1F2A37; font-size:11pt;"));
    m_pBody->setText(strEnglish);
    m_pPlay->setEnabled(true);
    m_pUse->setEnabled(true);
    setVisible(true);
}

/*
 * 功能描述:  设置playing。
 * 作者:      JaTin
 * param [in]   nPlaying         输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationCard::setPlaying(bool nPlaying) {
    m_pPlay->setPlaying(nPlaying);
}

/*
 * 功能描述:  隐藏卡片。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationCard::hideCard() {
    current_.clear();
    setVisible(false);
}
