/*
 * 文件名:    VoiceBar.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CVoiceBar 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CVoiceBar.h */

/* 气泡里的语音条：播放按钮 + 波形 + 时长。整条可点击。 */
/* 波形是按容器哈希生成的伪波形——真实波形要额外解码一次， */
/* 而语音条通常只用来「看出这是多长的一段话」，不值得那点开销。 */
#pragma once

#include <QByteArray>
#include <QVector>
#include <QWidget>

class CVoiceBar : public QWidget {
    Q_OBJECT

public:
    CVoiceBar(int durationMs, const QByteArray& container, bool outgoing, QWidget* parent = nullptr);

    void setPlaying(bool playing);
    QSize sizeHint() const override;

signals:
    void playRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int            m_nDurationMs = 0;
    bool           m_nOutgoing = false;
    bool           m_nPlaying = false;
    bool           m_nHovered = false;
    /* 0.25~1.0 的归一化柱高 */
    QVector<qreal> m_aBars;
};
