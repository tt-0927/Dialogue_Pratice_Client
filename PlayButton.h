// PlayButton.h
//
// 小号播放/停止按钮：气泡里听发音、译文卡上听译文都用它。
#pragma once

#include <QAbstractButton>

class PlayButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit PlayButton(QWidget* parent = nullptr);

    void setPlaying(bool playing);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool playing_ = false;
    bool hovered_ = false;
};
