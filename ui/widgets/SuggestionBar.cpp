/**
 * @file       SuggestionBar.cpp
 * @brief      CSuggestionBar 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CSuggestionBar.cpp。
 */
#include "ui/widgets/SuggestionBar.h"

#include "ui/widgets/PlayButton.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace {

/**
 * @brief  建议卡片：上行英文、下行中文，整块可点击（点击填入输入框）。
 * @details 用 QFrame 而不是 QPushButton，因为按钮只能渲染单一样式的单行文本。
 */
class CSuggestionChip : public QFrame {
public:
    CSuggestionChip(const QString& english, const QString& chinese,
                   std::function<void()> onClick, QWidget* parent)
        : QFrame(parent), onClick_(std::move(onClick)) {
        setObjectName(QStringLiteral("CSuggestionChip"));
        setCursor(Qt::PointingHandCursor);
        /**
         * @brief  让 QSS 的 :hover 生效。
         */
        setAttribute(Qt::WA_Hover, true);
        setToolTip(QStringLiteral("点击填入输入框"));

        /**
         * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
         */
        auto* layout = new QVBoxLayout(this);
        layout->setContentsMargins(10, 6, 10, 6);
        layout->setSpacing(2);

        /**
         * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
         */
        auto* en = new QLabel(english, this);
        en->setObjectName(QStringLiteral("SuggestionEn"));
        en->setWordWrap(true);
        layout->addWidget(en);

        if (!chinese.isEmpty()) {
            /**
             * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
             */
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

/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CSuggestionBar 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QFrame(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CSuggestionBar::CSuggestionBar(QWidget* pParent) : QFrame(pParent) {
    setObjectName(QStringLiteral("CSuggestionBar"));
    setVisible(false);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(14, 10, 14, 10);
    pRoot->setSpacing(6);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pTitle = new QLabel(QStringLiteral("建议回复"), this);
    m_pTitle->setStyleSheet(QStringLiteral("color:#7A8797; font-size:8.5pt;"));
    pRoot->addWidget(m_pTitle);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pList = new QVBoxLayout();
    m_pList->setContentsMargins(0, 0, 0, 0);
    m_pList->setSpacing(4);
    pRoot->addLayout(m_pList);
}

/**
 * @brief      清空 rows。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSuggestionBar::clearRows() {
    while (m_pList->count() > 0) {
        QLayoutItem* pItem = m_pList->takeAt(0);
        if (pItem->widget() != nullptr) pItem->widget()->deleteLater();
        delete pItem;
    }
}

/**
 * @brief      显示 pending；实现上创建 Qt 控件并纳入父子对象树。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSuggestionBar::showPending() {
    clearRows();

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
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

/**
 * @brief      显示智能建议列表；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * @param[in] aItems         条目列表；类型 const QVector<Suggestion_S>&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSuggestionBar::showSuggestions(const QVector<Suggestion_S>& aItems) {
    clearRows();
    if (aItems.isEmpty()) {
        hideBar();
        return;
    }

    for (const Suggestion_S& stItem : aItems) {
        /**
         * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
         */
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

/**
 * @brief      隐藏工具条。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSuggestionBar::hideBar() {
    setVisible(false);
    clearRows();
}
