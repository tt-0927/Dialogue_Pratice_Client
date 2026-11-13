/*
 * 文件名:    SuggestionBar.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CSuggestionBar 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CSuggestionBar.h */

/* 输入框上方的「建议回复」：3 条可直接使用（或先听发音）的英文短句， */
/* 每条英文下方跟一行中文译文。 */
/* 与译文卡互斥：输入框里有中文时显示译文卡，为空时显示建议。 */
#pragma once

#include <QFrame>
#include <QVector>

#include "business/ChatTypes.h"

class QLabel;
class QVBoxLayout;

class CSuggestionBar : public QFrame {
    Q_OBJECT

public:
    explicit CSuggestionBar(QWidget* parent = nullptr);

    void showPending();
    void showSuggestions(const QVector<Suggestion_S>& items);
    void hideBar();

signals:
    void useRequested(const QString& text);
    void playRequested(const QString& text);

private:
    void clearRows();

    QLabel*      m_pTitle = nullptr;
    QVBoxLayout* m_pList = nullptr;
};
