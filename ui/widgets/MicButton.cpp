// MicButton.cpp
#include "ui/widgets/MicButton.h"

#include "core/Theme.h"

#include <QPainter>
#include <QPaintEvent>

MicButton::MicButton(QWidget* parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(40, 40);
    setToolTip(QStringLiteral("点击开始语音输入"));
}

void MicButton::setRecording(bool recording) {
    if (recording_ == recording) return;
    recording_ = recording;
    setToolTip(recording ? QStringLiteral("点击结束录音并识别")
                         : QStringLiteral("点击开始语音输入"));
    update();
}

void MicButton::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    update();
    QAbstractButton::enterEvent(event);
}

void MicButton::leaveEvent(QEvent* event) {
    hovered_ = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void MicButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bg = recording_ ? QColor("#FDE7E7") : QColor("#F2F5F9");
    QColor fg = recording_ ? QColor("#E5484D") : theme::accent;

    if (!isEnabled()) {
        bg = QColor("#F4F6F9");
        fg = QColor("#B9C2CE");
    } else if (hovered_ && !recording_) {
        bg = QColor("#E8EEF6");
    } else if (hovered_ && recording_) {
        bg = QColor("#FBD9D9");
    }

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(r);

    const QPointF c = r.center();

    // 麦克风头
    p.setBrush(fg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(c.x() - 4.0, c.y() - 12.0, 8.0, 14.0), 4.0, 4.0);

    // 支架：下半圆弧 + 立杆 + 底座
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(fg, 1.8, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(c.x() - 7.0, c.y() - 8.0, 14.0, 14.0), 200 * 16, 140 * 16);
    p.drawLine(QPointF(c.x(), c.y() + 6.0), QPointF(c.x(), c.y() + 10.5));
    p.drawLine(QPointF(c.x() - 4.0, c.y() + 10.5), QPointF(c.x() + 4.0, c.y() + 10.5));

    // 录音中：右上角红点提示
    if (recording_) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#E5484D"));
        p.drawEllipse(QRectF(r.right() - 9.0, r.top() + 3.0, 6.0, 6.0));
    }
}
