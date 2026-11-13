// SendButton.cpp
#include "ui/widgets/SendButton.h"

#include "core/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

SendButton::SendButton(QWidget* parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(40, 40);
    setToolTip(QStringLiteral("发送 (Enter)"));
}

void SendButton::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    update();
    QAbstractButton::enterEvent(event);
}

void SendButton::leaveEvent(QEvent* event) {
    hovered_ = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void SendButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bg = theme::accent;
    if (!isEnabled()) {
        bg = QColor("#B9D4F2");
    } else if (isDown()) {
        bg = theme::accentPressed;
    } else if (hovered_) {
        bg = theme::accentHover;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(r);

    // 纸飞机：以 40x40 为基准绘制
    const qreal s = r.width() / 40.0;
    p.translate(r.topLeft());
    p.scale(s, s);

    QPainterPath plane;
    plane.moveTo(10.0, 19.6);
    plane.lineTo(30.0, 10.8);
    plane.lineTo(23.2, 29.2);
    plane.lineTo(19.4, 21.3);
    plane.closeSubpath();

    p.setBrush(Qt::white);
    p.setPen(Qt::NoPen);
    p.drawPath(plane);
}
