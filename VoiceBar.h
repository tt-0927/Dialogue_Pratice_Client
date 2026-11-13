// VoiceBar.h
//
// 气泡里的语音条：播放按钮 + 波形 + 时长。整条可点击。
// 波形是按容器哈希生成的伪波形——真实波形要额外解码一次，
// 而语音条通常只用来「看出这是多长的一段话」，不值得那点开销。
#pragma once

#include <QByteArray>
#include <QVector>
#include <QWidget>

class VoiceBar : public QWidget {
    Q_OBJECT

public:
    VoiceBar(int durationMs, const QByteArray& container, bool outgoing, QWidget* parent = nullptr);

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
    int            durationMs_ = 0;
    bool           outgoing_ = false;
    bool           playing_ = false;
    bool           hovered_ = false;
    QVector<qreal> bars_;      // 0.25~1.0 的归一化柱高
};
