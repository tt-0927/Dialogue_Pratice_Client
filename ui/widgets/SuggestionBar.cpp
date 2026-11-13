// SuggestionBar.cpp
#include "ui/widgets/SuggestionBar.h"

#include "ui/widgets/PlayButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace {

/// 建议卡片：上行英文、下行中文，整块可点击（点击填入输入框）。
/// 用 QFrame 而不是 QPushButton，因为按钮只能渲染单一样式的单行文本。
class SuggestionChip : public QFrame {
public:
    SuggestionChip(const QString& english, const QString& chinese,
                   std::function<void()> onClick, QWidget* parent)
        : QFrame(parent), onClick_(std::move(onClick)) {
        setObjectName(QStringLiteral("SuggestionChip"));
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_Hover, true);   // 让 QSS 的 :hover 生效
        setToolTip(QStringLiteral("点击填入输入框"));

        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(2);

        auto* en = new QLabel(english, this);
        en->setObjectName(QStringLiteral("SuggestionEn"));
        en->setWordWrap(true);
        layout->addWidget(en);

        if (!chinese.isEmpty()) {
            auto* zh = new QLabel(chinese, this);
            zh->setObjectName(QStringLiteral("SuggestionZh"));
            zh->setWordWrap(true);
            layout->addWidget(zh);
        }
    }

protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton && onClick_) onClick_();
        QFrame::mousePressEvent(event);
    }

private:
    std::function<void()> onClick_;
};

}  // namespace

SuggestionBar::SuggestionBar(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("SuggestionBar"));
    setVisible(false);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(6);

    title_ = new QLabel(QStringLiteral("建议回复"), this);
    title_->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    root->addWidget(title_);

    list_ = new QVBoxLayout();
    list_->setContentsMargins(0, 0, 0, 0);
    list_->setSpacing(4);
    root->addLayout(list_);
}

void SuggestionBar::clearRows() {
    while (list_->count() > 0) {
        QLayoutItem* item = list_->takeAt(0);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }
}

void SuggestionBar::showPending() {
    clearRows();

    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(QStringLiteral("正在生成建议…"), row);
    label->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:10pt;"));
    layout->addWidget(label);
    layout->addStretch(1);
    list_->addWidget(row);

    setVisible(true);
}

void SuggestionBar::showSuggestions(const QVector<Suggestion>& items) {
    clearRows();
    if (items.isEmpty()) {
        hideBar();
        return;
    }

    for (const Suggestion& item : items) {
        auto* row = new QWidget(this);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        const QString english = item.english;
        auto* chip = new SuggestionChip(english, item.chinese, [this, english] {
            emit useRequested(english);
        }, row);

        auto* play = new PlayButton(row);
        connect(play, &QAbstractButton::clicked, this, [this, english] {
            emit playRequested(english);
        });

        layout->addWidget(chip, 1);
        layout->addWidget(play, 0, Qt::AlignVCenter);
        list_->addWidget(row);
    }

    setVisible(true);
}

void SuggestionBar::hideBar() {
    setVisible(false);
    clearRows();
}
