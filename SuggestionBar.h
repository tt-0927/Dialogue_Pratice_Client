// SuggestionBar.h
//
// 输入框上方的「建议回复」：3 条可直接使用（或先听发音）的英文短句，
// 每条英文下方跟一行中文译文。
// 与译文卡互斥：输入框里有中文时显示译文卡，为空时显示建议。
#pragma once

#include <QFrame>
#include <QVector>

#include "ChatTypes.h"

class QLabel;
class QVBoxLayout;

class SuggestionBar : public QFrame {
    Q_OBJECT

public:
    explicit SuggestionBar(QWidget* parent = nullptr);

    void showPending();
    void showSuggestions(const QVector<Suggestion>& items);
    void hideBar();

signals:
    void useRequested(const QString& text);
    void playRequested(const QString& text);

private:
    void clearRows();

    QLabel*      title_ = nullptr;
    QVBoxLayout* list_ = nullptr;
};
