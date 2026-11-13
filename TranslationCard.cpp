/*
 * 文件名:    TranslationCard.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CTranslationCard 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CTranslationCard.cpp */
#include "TranslationCard.h"

#include "PlayButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

/*
 * 功能描述:  执行 CTranslationCard 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CTranslationCard::CTranslationCard(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("CTranslationCard"));
    setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(4);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTitle = new QLabel(QStringLiteral("译文"), this);
    m_pTitle->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    root->addWidget(m_pTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pBody = new QLabel(this);
    m_pBody->setWordWrap(true);
    m_pBody->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pBody->setStyleSheet(QStringLiteral("color:#1F2A37; font-size:11pt;"));
    root->addWidget(m_pBody);

    auto* buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 2, 0, 0);
    buttons->setSpacing(6);
    buttons->addStretch(1);

    m_pPlay = new CPlayButton(this);
    buttons->addWidget(m_pPlay);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pUse = new QPushButton(QStringLiteral("填入输入框"), this);
    m_pUse->setObjectName("LinkButton");
    m_pUse->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(m_pUse);

    root->addLayout(buttons);

    connect(m_pPlay, &QAbstractButton::clicked, this, [this] {
        if (!current_.isEmpty()) emit playRequested(current_);
    });
    connect(m_pUse, &QPushButton::clicked, this, [this] {
        if (!current_.isEmpty()) emit useRequested(current_);
    });
}

/*
 * 功能描述:  执行 showPending 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
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
 * 功能描述:  执行 showTranslation 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  english  参数。
 * return:    void
 */
void CTranslationCard::showTranslation(const QString& english) {
    current_ = english;
    m_pBody->setStyleSheet(QStringLiteral("color:#1F2A37; font-size:11pt;"));
    m_pBody->setText(english);
    m_pPlay->setEnabled(true);
    m_pUse->setEnabled(true);
    setVisible(true);
}

/*
 * 功能描述:  执行 setPlaying 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  playing  参数。
 * return:    void
 */
void CTranslationCard::setPlaying(bool playing) {
    m_pPlay->setPlaying(playing);
}

/*
 * 功能描述:  执行 hideCard 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CTranslationCard::hideCard() {
    current_.clear();
    setVisible(false);
}
