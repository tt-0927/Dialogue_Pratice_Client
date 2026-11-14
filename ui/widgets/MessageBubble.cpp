/*
 * 文件名:    MessageBubble.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMessageBubble 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMessageBubble.cpp */
#include "ui/widgets/MessageBubble.h"

#include "ui/widgets/Avatar.h"
#include "ui/widgets/PlayButton.h"
#include "ui/widgets/VoiceBar.h"
#include "core/Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QDebug>
#include <QVBoxLayout>

namespace {
/* 兜底上限：调用方没传或传了非法值时用它。 */
constexpr int kDefaultBubbleWidth = 520;
}

/*
 * 功能描述:  构造 CMessageBubble 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   strSenderName    输入数据，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   strTimeText      输入数据，类型 const QString&。
 * param [in]   nOutgoing        输入数据，类型 bool。
 * param [in]   lSenderId        输入数据，类型 qint64。
 * param [in]   nPlayable        输入数据，类型 bool。
 * param [in]   nMaxWidth        输入数据，类型 int。
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget*。
 * return:    无返回值（构造函数或析构函数）。
 */
CMessageBubble::CMessageBubble(const QString& strSenderName,
                             const QString& strText,
                             const QString& strTimeText,
                             bool nOutgoing,
                             qint64 lSenderId,
                             bool nPlayable,
                             int nMaxWidth,
                             QWidget* pParent)
    : QWidget(pParent) {
    const int nKMaxBubbleWidth =
        (nMaxWidth > 80) ? nMaxWidth : kDefaultBubbleWidth;
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QHBoxLayout(this);
    pRoot->setContentsMargins(0, 0, 0, 0);
    pRoot->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pBubble = new QFrame(this);
    m_pBubble = pBubble;
    pBubble->setObjectName(nOutgoing ? "BubbleOut" : "BubbleIn");
    pBubble->setMaximumWidth(nKMaxBubbleWidth);
    pBubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

    auto* pBubbleLayout = new QVBoxLayout(pBubble);
    m_pBubbleLayout = pBubbleLayout;
    pBubbleLayout->setContentsMargins(12, 8, 12, 6);
    pBubbleLayout->setSpacing(2);

    auto* pBody = new QLabel(strText, pBubble);
    m_pBody = pBody;
    pBody->setObjectName("BubbleText");
    pBody->setWordWrap(true);
    pBody->setTextInteractionFlags(Qt::TextSelectableByMouse);

    /* QLabel 在 wordWrap=true 时的 sizeHint 会偏窄，短句会被提前折行。 */
    /* 这里用字体度量算出「单行实际需要多宽」，再夹到气泡上限。 */
    pBody->ensurePolished();
    const int nMaxTextWidth = nKMaxBubbleWidth - 28;
    const QRect stNeeded = pBody->fontMetrics().boundingRect(
        QRect(0, 0, nMaxTextWidth, 100000), Qt::TextWordWrap | Qt::AlignLeft, strText);
    pBody->setFixedWidth(qBound(16, stNeeded.width() + 2, nMaxTextWidth));
    pBubbleLayout->addWidget(pBody);

    /* 语音转写（英文原文），显示在译文上方 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTranscriptLabel = new QLabel(pBubble);
    m_pTranscriptLabel->setObjectName("BubbleTranscript");
    m_pTranscriptLabel->setWordWrap(true);
    m_pTranscriptLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pTranscriptLabel->setMaximumWidth(nKMaxBubbleWidth - 28);
    m_pTranscriptLabel->setVisible(false);
    pBubbleLayout->addWidget(m_pTranscriptLabel);

    /* 译文区：分隔线 + 译文，默认隐藏 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTranslationDivider = new QFrame(pBubble);
    m_pTranslationDivider->setObjectName("BubbleDivider");
    m_pTranslationDivider->setFrameShape(QFrame::HLine);
    m_pTranslationDivider->setVisible(false);
    pBubbleLayout->addWidget(m_pTranslationDivider);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTranslationLabel = new QLabel(pBubble);
    m_pTranslationLabel->setObjectName("BubbleTranslation");
    m_pTranslationLabel->setWordWrap(true);
    m_pTranslationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pTranslationLabel->setMaximumWidth(nKMaxBubbleWidth - 28);
    m_pTranslationLabel->setVisible(false);
    pBubbleLayout->addWidget(m_pTranslationLabel);

    /* 底部一行：播放按钮（仅他人英文消息）+ 时间 */
    auto* pFooter = new QHBoxLayout();
    pFooter->setContentsMargins(0, 0, 0, 0);
    pFooter->setSpacing(4);

    if (nPlayable) {
        m_pPlayBtn = new CPlayButton(pBubble);
        connect(m_pPlayBtn, &QAbstractButton::clicked, this, [this, strText] {
            emit playRequested(strText);
        });
        pFooter->addWidget(m_pPlayBtn, 0, Qt::AlignVCenter);
    }
    pFooter->addStretch(1);

    auto* pTimeLabel = new QLabel(strTimeText, pBubble);
    pTimeLabel->setObjectName("TimeLabel");
    pTimeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pFooter->addWidget(pTimeLabel, 0, Qt::AlignBottom);

    pBubbleLayout->addLayout(pFooter);

    if (nOutgoing) {
        pRoot->addStretch(1);
        pRoot->addWidget(pBubble, 0, Qt::AlignRight | Qt::AlignTop);
        return;
    }

    auto* pAvatar = new CAvatar(this);
    pAvatar->setFixedSize(32, 32);
    pAvatar->setFontPx(14);
    pAvatar->setText(strSenderName);
    pAvatar->setColor(CAvatar::colorForId(lSenderId));

    auto* pColumn = new QVBoxLayout();
    pColumn->setContentsMargins(0, 0, 0, 0);
    pColumn->setSpacing(3);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pNameLabel = new QLabel(strSenderName, this);
    pNameLabel->setObjectName("NameLabel");
    pNameLabel->setStyleSheet(QStringLiteral("color: %1;").arg(CAvatar::colorForId(lSenderId).name()));

    pColumn->addWidget(pNameLabel);
    pColumn->addWidget(pBubble, 0, Qt::AlignLeft);

    pRoot->addWidget(pAvatar, 0, Qt::AlignTop);
    pRoot->addLayout(pColumn, 0);
    pRoot->addStretch(1);
}

/*
 * 功能描述:  设置译文。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::setTranslation(const QString& strText) {
    if (strText.isEmpty()) {
        clearTranslation();
        return;
    }
    m_pTranslationDivider->setVisible(true);
    m_pTranslationLabel->setStyleSheet(QString());
    m_pTranslationLabel->setText(strText);
    m_pTranslationLabel->setVisible(true);
}

/*
 * 功能描述:  设置译文 pending。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::setTranslationPending() {
    m_pTranslationDivider->setVisible(true);
    m_pTranslationLabel->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:9pt;"));
    m_pTranslationLabel->setText(QStringLiteral("翻译中…"));
    m_pTranslationLabel->setVisible(true);
}

/*
 * 功能描述:  设置译文 failed。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::setTranslationFailed() {
    m_pTranslationDivider->setVisible(true);
    m_pTranslationLabel->setStyleSheet(QStringLiteral("color:#E5484D; font-size:9pt;"));
    m_pTranslationLabel->setText(QStringLiteral("翻译失败"));
    m_pTranslationLabel->setVisible(true);
}

/*
 * 功能描述:  清空译文。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::clearTranslation() {
    m_pTranslationDivider->setVisible(false);
    m_pTranslationLabel->setVisible(false);
    m_pTranslationLabel->clear();
}

/*
 * 功能描述:  设置语音消息；实现上连接 Qt 信号与槽。
 * 作者:      JaTin
 * param [in]   byAudio          Opus 编码的音频数据，类型 const QByteArray&。
 * param [in]   nDurationMs      语音时长，单位毫秒，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::setVoice(const QByteArray& byAudio, int nDurationMs) {
    if (byAudio.isEmpty() || m_pBubbleLayout == nullptr) return;

    m_byVoiceAudio = byAudio;
    m_pBody->setVisible(false);

    m_pVoiceBar = new CVoiceBar(nDurationMs, byAudio, false, m_pBubble);
    /* 插在正文原来的位置（正文仍是索引 0，语音条紧跟其后） */
    m_pBubbleLayout->insertWidget(1, m_pVoiceBar);
    connect(m_pVoiceBar, &CVoiceBar::playRequested, this, [this] {
        emit voicePlayRequested(m_byVoiceAudio);
    });

}

/*
 * 功能描述:  设置playing。
 * 作者:      JaTin
 * param [in]   nPlaying         输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::setPlaying(bool nPlaying) {
    if (m_pVoiceBar != nullptr) {
        m_pVoiceBar->setPlaying(nPlaying);
        return;
    }
    if (m_pPlayBtn != nullptr) m_pPlayBtn->setPlaying(nPlaying);
}

/*
 * 功能描述:  设置语音消息的转写文本及其状态。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::setTranscript(const QString& strText) {
    if (strText.isEmpty()) {
        m_pTranscriptLabel->setVisible(false);
        m_pTranscriptLabel->clear();
        return;
    }
    m_pTranscriptLabel->setStyleSheet(QString());
    m_pTranscriptLabel->setText(strText);
    m_pTranscriptLabel->setVisible(true);
}

/*
 * 功能描述:  设置转写文本 pending。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageBubble::setTranscriptPending() {
    m_pTranscriptLabel->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:9pt;"));
    m_pTranscriptLabel->setText(QStringLiteral("识别中…"));
    m_pTranscriptLabel->setVisible(true);
}
