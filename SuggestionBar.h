/**
 * @file       SuggestionBar.h
 * @brief      CSuggestionBar 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CSuggestionBar.h。
 */

/**
 * @brief  输入框上方的「建议回复」：3 条可直接使用（或先听发音）的英文短句， 每条英文下方跟一行中文译文。 与译文卡互斥：输入框里有中文时显示译文卡，为空时显示建议。
 */
#pragma once

#include <QFrame>
#include <QVector>

#include "ChatTypes.h"

class QLabel;
class QVBoxLayout;

/**
 * @brief   C智能建议工具条。
 * @author  JaTin
 */
class CSuggestionBar : public QFrame {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CSuggestionBar(QWidget* parent = nullptr);

    /**
     * @brief      显示 pending。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showPending();
    /**
     * @brief      显示智能建议列表。
     * @param[in] items          条目列表；类型 const QVector<Suggestion_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showSuggestions(const QVector<Suggestion_S>& items);
    /**
     * @brief      隐藏工具条。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void hideBar();

signals:
    /**
     * @brief      执行 userequested相关的数据并返回处理结果。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void useRequested(const QString& text);
    /**
     * @brief      播放 requested。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void playRequested(const QString& text);

private:
    /**
     * @brief      清空 rows。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearRows();

    /**
     * @brief  Title。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pTitle = nullptr;
    /**
     * @brief  列表。 初值 = nullptr。
     * @note   类型 QVBoxLayout*；类/结构体成员变量。
     */
    QVBoxLayout* m_pList = nullptr;
};
