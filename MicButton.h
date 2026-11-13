/*
 * 文件名:    MicButton.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMicButton 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMicButton.h */

/* 圆形麦克风按钮：自绘图标，录音时变红，避免依赖字体里的 emoji。 */
#pragma once

#include <QAbstractButton>

class CMicButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit CMicButton(QWidget* parent = nullptr);

    void setRecording(bool recording);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool m_nRecording = false;
    bool m_nHovered = false;
};
