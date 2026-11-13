/*
 * 文件名:    SendButton.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CSendButton 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CSendButton.h */

/* 圆形发送按钮：自绘纸飞机图标，避免依赖字体里的箭头字形。 */
#pragma once

#include <QAbstractButton>

class CSendButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit CSendButton(QWidget* parent = nullptr);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool m_nHovered = false;
};
