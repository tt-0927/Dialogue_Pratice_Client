// TranslationCard.cpp
#include "TranslationCard.h"

#include "PlayButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

TranslationCard::TranslationCard(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("TranslationCard"));
    setVisible(false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(4);

    title_ = new QLabel(QStringLiteral("译文"), this);
    title_->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    root->addWidget(title_);

    body_ = new QLabel(this);
    body_->setWordWrap(true);
    body_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    body_->setStyleSheet(QStringLiteral("color:#1F2A37; font-size:11pt;"));
    root->addWidget(body_);

    auto* buttons = new QHBoxLayout();
    buttons->setContentsMargins(0, 2, 0, 0);
    buttons->setSpacing(6);
    buttons->addStretch(1);

    play_ = new PlayButton(this);
    buttons->addWidget(play_);

    use_ = new QPushButton(QStringLiteral("填入输入框"), this);
    use_->setObjectName("LinkButton");
    use_->setCursor(Qt::PointingHandCursor);
    buttons->addWidget(use_);

    root->addLayout(buttons);

    connect(play_, &QAbstractButton::clicked, this, [this] {
        if (!current_.isEmpty()) emit playRequested(current_);
    });
    connect(use_, &QPushButton::clicked, this, [this] {
        if (!current_.isEmpty()) emit useRequested(current_);
    });
}

void TranslationCard::showPending() {
    current_.clear();
    body_->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:10pt;"));
    body_->setText(QStringLiteral("翻译中…"));
    play_->setEnabled(false);
    use_->setEnabled(false);
    setVisible(true);
}

void TranslationCard::showTranslation(const QString& english) {
    current_ = english;
    body_->setStyleSheet(QStringLiteral("color:#1F2A37; font-size:11pt;"));
    body_->setText(english);
    play_->setEnabled(true);
    use_->setEnabled(true);
    setVisible(true);
}

void TranslationCard::setPlaying(bool playing) {
    play_->setPlaying(playing);
}

void TranslationCard::hideCard() {
    current_.clear();
    setVisible(false);
}
