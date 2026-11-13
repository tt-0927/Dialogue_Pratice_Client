/*
 * 文件名:    SuggestionBar.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CSuggestionBar 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CSuggestionBar.cpp */
#include "ui/widgets/SuggestionBar.h"

#include "ui/widgets/PlayButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace {

/* 建议卡片：上行英文、下行中文，整块可点击（点击填入输入框）。 */
/* 用 QFrame 而不是 QPushButton，因为按钮只能渲染单一样式的单行文本。 */
class CSuggestionChip : public QFrame {
public:
    CSuggestionChip(const QString& english, const QString& chinese,
                   std::function<void()> onClick, QWidget* parent)
        : QFrame(parent), onClick_(std::move(onClick)) {
        setObjectName(QStringLiteral("CSuggestionChip"));
        setCursor(Qt::PointingHandCursor);
        /* 让 QSS 的 :hover 生效 */
        setAttribute(Qt::WA_Hover, true);
        setToolTip(QStringLiteral("点击填入输入框"));

        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(2);

        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        auto* en = new QLabel(english, this);
        en->setObjectName(QStringLiteral("SuggestionEn"));
        en->setWordWrap(true);
        layout->addWidget(en);

        if (!chinese.isEmpty()) {
            /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
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

/* namespace */
}

/*
 * 功能描述:  执行 CSuggestionBar 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CSuggestionBar::CSuggestionBar(QWidget* parent) : QFrame(parent) {
    setObjectName(QStringLiteral("CSuggestionBar"));
    setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 10, 14, 10);
    root->setSpacing(6);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTitle = new QLabel(QStringLiteral("建议回复"), this);
    m_pTitle->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    root->addWidget(m_pTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pList = new QVBoxLayout();
    m_pList->setContentsMargins(0, 0, 0, 0);
    m_pList->setSpacing(4);
    root->addLayout(m_pList);
}

/*
 * 功能描述:  执行 clearRows 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CSuggestionBar::clearRows() {
    while (m_pList->count() > 0) {
        QLayoutItem* item = m_pList->takeAt(0);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }
}

/*
 * 功能描述:  执行 showPending 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CSuggestionBar::showPending() {
    clearRows();

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* row = new QWidget(this);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* label = new QLabel(QStringLiteral("正在生成建议…"), row);
    label->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:10pt;"));
    layout->addWidget(label);
    layout->addStretch(1);
    m_pList->addWidget(row);

    setVisible(true);
}

/*
 * 功能描述:  执行 showSuggestions 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  items  参数。
 * return:    void
 */
void CSuggestionBar::showSuggestions(const QVector<Suggestion_S>& items) {
    clearRows();
    if (items.isEmpty()) {
        hideBar();
        return;
    }

    for (const Suggestion_S& item : items) {
        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        auto* row = new QWidget(this);
        auto* layout = new QHBoxLayout(row);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(6);

        const QString english = item.english;
        auto* chip = new CSuggestionChip(english, item.chinese, [this, english] {
            emit useRequested(english);
        }, row);

        auto* play = new CPlayButton(row);
        connect(play, &QAbstractButton::clicked, this, [this, english] {
            emit playRequested(english);
        });

        layout->addWidget(chip, 1);
        layout->addWidget(play, 0, Qt::AlignVCenter);
        m_pList->addWidget(row);
    }

    setVisible(true);
}

/*
 * 功能描述:  执行 hideBar 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CSuggestionBar::hideBar() {
    setVisible(false);
    clearRows();
}
