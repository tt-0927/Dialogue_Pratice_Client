/*
 * 文件名:    PlayButton.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CPlayButton 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CPlayButton.h */

/* 小号播放/停止按钮：气泡里听发音、译文卡上听译文都用它。 */
#pragma once

#include <QAbstractButton>

class CPlayButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit CPlayButton(QWidget* parent = nullptr);

    void setPlaying(bool playing);

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    bool m_nPlaying = false;
    bool m_nHovered = false;
};
