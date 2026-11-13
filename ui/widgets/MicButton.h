// MicButton.h
//
// 圆形麦克风按钮：自绘图标，录音时变红，避免依赖字体里的 emoji。
#pragma once

#include <QAbstractButton>

class MicButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit MicButton(QWidget* parent = nullptr);

    void setRecording(bool recording);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool recording_ = false;
    bool hovered_ = false;
};
