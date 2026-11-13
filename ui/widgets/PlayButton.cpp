// PlayButton.cpp
#include "ui/widgets/PlayButton.h"

#include "core/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

PlayButton::PlayButton(QWidget* parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(22, 22);
    setToolTip(QStringLiteral("播放发音"));
}

void PlayButton::setPlaying(bool playing) {
    if (playing_ == playing) return;
    playing_ = playing;
    setToolTip(playing ? QStringLiteral("停止播放") : QStringLiteral("播放发音"));
    update();
}

void PlayButton::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    update();
    QAbstractButton::enterEvent(event);
}

void PlayButton::leaveEvent(QEvent* event) {
    hovered_ = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void PlayButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    const QPointF c = r.center();
    const QColor accent = theme::accent;

    if (hovered_ && isEnabled()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#E8F0FA"));
        p.drawEllipse(r);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(isEnabled() ? accent : QColor("#B9C2CE"));

    if (playing_) {
        // 播放中：两根竖条（暂停）
        p.drawRoundedRect(QRectF(c.x() - 4.0, c.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        p.drawRoundedRect(QRectF(c.x() + 1.4, c.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        return;
    }

    // 待播放：三角形
    QPainterPath triangle;
    triangle.moveTo(c.x() - 3.2, c.y() - 5.0);
    triangle.lineTo(c.x() + 5.2, c.y());
    triangle.lineTo(c.x() - 3.2, c.y() + 5.0);
    triangle.closeSubpath();
    p.drawPath(triangle);
}
