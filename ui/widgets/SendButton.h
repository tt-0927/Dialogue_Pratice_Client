// SendButton.h
//
// 圆形发送按钮：自绘纸飞机图标，避免依赖字体里的箭头字形。
#pragma once

#include <QAbstractButton>

class SendButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit SendButton(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool hovered_ = false;
};
