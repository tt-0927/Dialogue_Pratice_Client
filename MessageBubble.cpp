// MessageBubble.cpp
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

MessageBubble::MessageBubble(const QString& senderName,
                             const QString& text,
                             const QString& timeText,
                             bool outgoing,
                             qint64 senderId,
                             bool playable,
                             QWidget* parent)
    : QWidget(parent) {
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(8);

    auto* bubble = new QFrame(this);
    bubble_ = bubble;
    bubble->setObjectName(outgoing ? "BubbleOut" : "BubbleIn");
    bubble->setMaximumWidth(kMaxBubbleWidth);
    bubble->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

    auto* bubbleLayout = new QVBoxLayout(bubble);
    bubbleLayout_ = bubbleLayout;
    bubbleLayout->setContentsMargins(12, 8, 12, 6);
    bubbleLayout->setSpacing(2);

    auto* body = new QLabel(text, bubble);
    body_ = body;
    body->setObjectName("BubbleText");
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);

    // QLabel 在 wordWrap=true 时的 sizeHint 会偏窄，短句会被提前折行。
    // 这里用字体度量算出「单行实际需要多宽」，再夹到气泡上限。
    body->ensurePolished();
    const int maxTextWidth = kMaxBubbleWidth - 28;
    const QRect needed = body->fontMetrics().boundingRect(
        QRect(0, 0, maxTextWidth, 100000), Qt::TextWordWrap | Qt::AlignLeft, text);
    body->setFixedWidth(qBound(16, needed.width() + 2, maxTextWidth));
    bubbleLayout->addWidget(body);

    // 语音转写（英文原文），显示在译文上方
    transcriptLabel_ = new QLabel(bubble);
    transcriptLabel_->setObjectName("BubbleTranscript");
    transcriptLabel_->setWordWrap(true);
    transcriptLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    transcriptLabel_->setMaximumWidth(kMaxBubbleWidth - 28);
    transcriptLabel_->setVisible(false);
    bubbleLayout->addWidget(transcriptLabel_);

    // 译文区：分隔线 + 译文，默认隐藏
    translationDivider_ = new QFrame(bubble);
    translationDivider_->setObjectName("BubbleDivider");
    translationDivider_->setFrameShape(QFrame::HLine);
    translationDivider_->setVisible(false);
    bubbleLayout->addWidget(translationDivider_);

    translationLabel_ = new QLabel(bubble);
    translationLabel_->setObjectName("BubbleTranslation");
    translationLabel_->setWordWrap(true);
    translationLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    translationLabel_->setMaximumWidth(kMaxBubbleWidth - 28);
    translationLabel_->setVisible(false);
    bubbleLayout->addWidget(translationLabel_);

    // 底部一行：播放按钮（仅他人英文消息）+ 时间
    auto* footer = new QHBoxLayout();
    footer->setContentsMargins(0, 0, 0, 0);
    footer->setSpacing(4);

    if (playable) {
        playBtn_ = new PlayButton(bubble);
        connect(playBtn_, &QAbstractButton::clicked, this, [this, text] {
            emit playRequested(text);
        });
        footer->addWidget(playBtn_, 0, Qt::AlignVCenter);
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

    auto* avatar = new Avatar(this);
    avatar->setFixedSize(32, 32);
    avatar->setFontPx(14);
    avatar->setText(senderName);
    avatar->setColor(Avatar::colorForId(senderId));

    auto* column = new QVBoxLayout();
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(3);

    auto* nameLabel = new QLabel(senderName, this);
    nameLabel->setObjectName("NameLabel");
    nameLabel->setStyleSheet(QStringLiteral("color: %1;").arg(Avatar::colorForId(senderId).name()));

    column->addWidget(nameLabel);
    column->addWidget(bubble, 0, Qt::AlignLeft);

    root->addWidget(avatar, 0, Qt::AlignTop);
    root->addLayout(column, 0);
    root->addStretch(1);
}

void MessageBubble::setTranslation(const QString& text) {
    if (text.isEmpty()) {
        clearTranslation();
        return;
    }
    translationDivider_->setVisible(true);
    translationLabel_->setStyleSheet(QString());
    translationLabel_->setText(text);
    translationLabel_->setVisible(true);
}

void MessageBubble::setTranslationPending() {
    translationDivider_->setVisible(true);
    translationLabel_->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:9pt;"));
    translationLabel_->setText(QStringLiteral("翻译中…"));
    translationLabel_->setVisible(true);
}

void MessageBubble::setTranslationFailed() {
    translationDivider_->setVisible(true);
    translationLabel_->setStyleSheet(QStringLiteral("color:#E5484D; font-size:9pt;"));
    translationLabel_->setText(QStringLiteral("翻译失败"));
    translationLabel_->setVisible(true);
}

void MessageBubble::clearTranslation() {
    translationDivider_->setVisible(false);
    translationLabel_->setVisible(false);
    translationLabel_->clear();
}

void MessageBubble::setVoice(const QByteArray& audio, int durationMs) {
    if (audio.isEmpty() || bubbleLayout_ == nullptr) return;

    voiceAudio_ = audio;
    body_->setVisible(false);

    voiceBar_ = new VoiceBar(durationMs, audio, false, bubble_);
    // 插在正文原来的位置（正文仍是索引 0，语音条紧跟其后）
    bubbleLayout_->insertWidget(1, voiceBar_);
    connect(voiceBar_, &VoiceBar::playRequested, this, [this] {
        emit voicePlayRequested(voiceAudio_);
    });
}

void MessageBubble::setPlaying(bool playing) {
    if (voiceBar_ != nullptr) {
        voiceBar_->setPlaying(playing);
        return;
    }
    if (playBtn_ != nullptr) playBtn_->setPlaying(playing);
}

void MessageBubble::setTranscript(const QString& text) {
    if (text.isEmpty()) {
        transcriptLabel_->setVisible(false);
        transcriptLabel_->clear();
        return;
    }
    transcriptLabel_->setStyleSheet(QString());
    transcriptLabel_->setText(text);
    transcriptLabel_->setVisible(true);
}

void MessageBubble::setTranscriptPending() {
    transcriptLabel_->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:9pt;"));
    transcriptLabel_->setText(QStringLiteral("识别中…"));
    transcriptLabel_->setVisible(true);
}
