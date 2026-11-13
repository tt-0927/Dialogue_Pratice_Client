/*
 * 文件名:    MessageBubble.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMessageBubble 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMessageBubble.cpp */
#include "MessageBubble.h"

#include "Avatar.h"
#include "PlayButton.h"
#include "VoiceBar.h"
#include "Theme.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace {
constexpr int kMaxBubbleWidth = 520;
}

CMessageBubble::CMessageBubble(const QString& senderName,
                             const QString& text,
                             const QString& timeText,
                             bool outgoing,
                             qint64 senderId,
                             bool playable,
                             QWidget* parent)
    : QWidget(parent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* bubble = new QFrame(this);
    m_pBubble = bubble;
    bubble->setObjectName(outgoing ? "BubbleOut" : "BubbleIn");
    bubble->setMaximumWidth(kMaxBubbleWidth);
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

    auto* bubbleLayout = new QVBoxLayout(bubble);
    m_pBubbleLayout = bubbleLayout;
    bubbleLayout->setContentsMargins(12, 8, 12, 6);
    bubbleLayout->setSpacing(2);

    auto* body = new QLabel(text, bubble);
    m_pBody = body;
    body->setObjectName("BubbleText");
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);

    /* QLabel 在 wordWrap=true 时的 sizeHint 会偏窄，短句会被提前折行。 */
    /* 这里用字体度量算出「单行实际需要多宽」，再夹到气泡上限。 */
    body->ensurePolished();
    const int maxTextWidth = kMaxBubbleWidth - 28;
    const QRect needed = body->fontMetrics().boundingRect(
        QRect(0, 0, maxTextWidth, 100000), Qt::TextWordWrap | Qt::AlignLeft, text);
    body->setFixedWidth(qBound(16, needed.width() + 2, maxTextWidth));
    bubbleLayout->addWidget(body);

    /* 语音转写（英文原文），显示在译文上方 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTranscriptLabel = new QLabel(bubble);
    m_pTranscriptLabel->setObjectName("BubbleTranscript");
    m_pTranscriptLabel->setWordWrap(true);
    m_pTranscriptLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pTranscriptLabel->setMaximumWidth(kMaxBubbleWidth - 28);
    m_pTranscriptLabel->setVisible(false);
    bubbleLayout->addWidget(m_pTranscriptLabel);

    /* 译文区：分隔线 + 译文，默认隐藏 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTranslationDivider = new QFrame(bubble);
    m_pTranslationDivider->setObjectName("BubbleDivider");
    m_pTranslationDivider->setFrameShape(QFrame::HLine);
    m_pTranslationDivider->setVisible(false);
    bubbleLayout->addWidget(m_pTranslationDivider);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTranslationLabel = new QLabel(bubble);
    m_pTranslationLabel->setObjectName("BubbleTranslation");
    m_pTranslationLabel->setWordWrap(true);
    m_pTranslationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_pTranslationLabel->setMaximumWidth(kMaxBubbleWidth - 28);
    m_pTranslationLabel->setVisible(false);
    bubbleLayout->addWidget(m_pTranslationLabel);

    /* 底部一行：播放按钮（仅他人英文消息）+ 时间 */
    auto* footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(4);

    if (playable) {
        m_pPlayBtn = new CPlayButton(bubble);
        connect(m_pPlayBtn, &QAbstractButton::clicked, this, [this, text] {
            emit playRequested(text);
        });
        footer->addWidget(m_pPlayBtn, 0, Qt::AlignVCenter);
    }
    footer->addStretch(1);

    auto* timeLabel = new QLabel(timeText, bubble);
    timeLabel->setObjectName("TimeLabel");
    timeLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    footer->addWidget(timeLabel, 0, Qt::AlignBottom);

    bubbleLayout->addLayout(footer);

    if (outgoing) {
        root->addStretch(1);
        root->addWidget(bubble, 0, Qt::AlignRight | Qt::AlignTop);
        return;
    }

    auto* avatar = new CAvatar(this);
    avatar->setFixedSize(32, 32);
    avatar->setFontPx(14);
    avatar->setText(senderName);
    avatar->setColor(CAvatar::colorForId(senderId));

    auto* column = new QVBoxLayout();
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(3);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* nameLabel = new QLabel(senderName, this);
    nameLabel->setObjectName("NameLabel");
    nameLabel->setStyleSheet(QStringLiteral("color: %1;").arg(CAvatar::colorForId(senderId).name()));

    column->addWidget(nameLabel);
    column->addWidget(bubble, 0, Qt::AlignLeft);

    root->addWidget(avatar, 0, Qt::AlignTop);
    root->addLayout(column, 0);
    root->addStretch(1);
}

/*
 * 功能描述:  执行 setTranslation 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CMessageBubble::setTranslation(const QString& text) {
    if (text.isEmpty()) {
        clearTranslation();
        return;
    }
    m_pTranslationDivider->setVisible(true);
    m_pTranslationLabel->setStyleSheet(QString());
    m_pTranslationLabel->setText(text);
    m_pTranslationLabel->setVisible(true);
}

/*
 * 功能描述:  执行 setTranslationPending 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMessageBubble::setTranslationPending() {
    m_pTranslationDivider->setVisible(true);
    m_pTranslationLabel->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:9pt;"));
    m_pTranslationLabel->setText(QStringLiteral("翻译中…"));
    m_pTranslationLabel->setVisible(true);
}

/*
 * 功能描述:  执行 setTranslationFailed 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMessageBubble::setTranslationFailed() {
    m_pTranslationDivider->setVisible(true);
    m_pTranslationLabel->setStyleSheet(QStringLiteral("color:#E5484D; font-size:9pt;"));
    m_pTranslationLabel->setText(QStringLiteral("翻译失败"));
    m_pTranslationLabel->setVisible(true);
}

/*
 * 功能描述:  执行 clearTranslation 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMessageBubble::clearTranslation() {
    m_pTranslationDivider->setVisible(false);
    m_pTranslationLabel->setVisible(false);
    m_pTranslationLabel->clear();
}

/*
 * 功能描述:  执行 setVoice 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  audio  参数。
 * param [in]  durationMs  参数。
 * return:    void
 */
void CMessageBubble::setVoice(const QByteArray& audio, int durationMs) {
    if (audio.isEmpty() || m_pBubbleLayout == nullptr) return;

    m_byVoiceAudio = audio;
    m_pBody->setVisible(false);

    m_pVoiceBar = new CVoiceBar(durationMs, audio, false, m_pBubble);
    /* 插在正文原来的位置（正文仍是索引 0，语音条紧跟其后） */
    m_pBubbleLayout->insertWidget(1, m_pVoiceBar);
    connect(m_pVoiceBar, &CVoiceBar::playRequested, this, [this] {
        emit voicePlayRequested(m_byVoiceAudio);
    });
}

/*
 * 功能描述:  执行 setPlaying 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  playing  参数。
 * return:    void
 */
void CMessageBubble::setPlaying(bool playing) {
    if (m_pVoiceBar != nullptr) {
        m_pVoiceBar->setPlaying(playing);
        return;
    }
    if (m_pPlayBtn != nullptr) m_pPlayBtn->setPlaying(playing);
}

/*
 * 功能描述:  执行 setTranscript 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CMessageBubble::setTranscript(const QString& text) {
    if (text.isEmpty()) {
        m_pTranscriptLabel->setVisible(false);
        m_pTranscriptLabel->clear();
        return;
    }
    m_pTranscriptLabel->setStyleSheet(QString());
    m_pTranscriptLabel->setText(text);
    m_pTranscriptLabel->setVisible(true);
}

/*
 * 功能描述:  执行 setTranscriptPending 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMessageBubble::setTranscriptPending() {
    m_pTranscriptLabel->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:9pt;"));
    m_pTranscriptLabel->setText(QStringLiteral("识别中…"));
    m_pTranscriptLabel->setVisible(true);
}
