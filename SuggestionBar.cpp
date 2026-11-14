/*
 * 文件名:    SuggestionBar.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CSuggestionBar 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CSuggestionBar.cpp */
#include "SuggestionBar.h"

#include "PlayButton.h"

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
 * 功能描述:  构造 CSuggestionBar 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QFrame(。
 * return:    无返回值（构造函数或析构函数）。
 */
CSuggestionBar::CSuggestionBar(QWidget* pParent) : QFrame(pParent) {
    setObjectName(QStringLiteral("CSuggestionBar"));
    setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(14, 10, 14, 10);
    pRoot->setSpacing(6);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTitle = new QLabel(QStringLiteral("建议回复"), this);
    m_pTitle->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    pRoot->addWidget(m_pTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pList = new QVBoxLayout();
    m_pList->setContentsMargins(0, 0, 0, 0);
    m_pList->setSpacing(4);
    pRoot->addLayout(m_pList);
}

/*
 * 功能描述:  清空rows。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSuggestionBar::clearRows() {
    while (m_pList->count() > 0) {
        QLayoutItem* pItem = m_pList->takeAt(0);
        if (pItem->widget() != nullptr) pItem->widget()->deleteLater();
        delete pItem;
    }
}

/*
 * 功能描述:  显示pending；实现上创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSuggestionBar::showPending() {
    clearRows();

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRow = new QWidget(this);
    auto* pLayout = new QHBoxLayout(pRow);
    pLayout->setContentsMargins(0, 0, 0, 0);
    auto* pLabel = new QLabel(QStringLiteral("正在生成建议…"), pRow);
    pLabel->setStyleSheet(QStringLiteral("color:#A0AAB8; font-size:10pt;"));
    pLayout->addWidget(pLabel);
    pLayout->addStretch(1);
    m_pList->addWidget(pRow);

    setVisible(true);
}

/*
 * 功能描述:  显示智能建议列表；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   aItems           条目列表，类型 const QVector<Suggestion_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSuggestionBar::showSuggestions(const QVector<Suggestion_S>& aItems) {
    clearRows();
    if (aItems.isEmpty()) {
        hideBar();
        return;
    }

    for (const Suggestion_S& stItem : aItems) {
        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        auto* pRow = new QWidget(this);
        auto* pLayout = new QHBoxLayout(pRow);
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setSpacing(6);

        const QString strEnglish = stItem.english;
        auto* pChip = new CSuggestionChip(strEnglish, stItem.chinese, [this, strEnglish] {
            emit useRequested(strEnglish);
        }, pRow);

        auto* pPlay = new CPlayButton(pRow);
        connect(pPlay, &QAbstractButton::clicked, this, [this, strEnglish] {
            emit playRequested(strEnglish);
        });

        pLayout->addWidget(pChip, 1);
        pLayout->addWidget(pPlay, 0, Qt::AlignVCenter);
        m_pList->addWidget(pRow);
    }

    setVisible(true);
}

/*
 * 功能描述:  隐藏工具条。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSuggestionBar::hideBar() {
    setVisible(false);
    clearRows();
}
