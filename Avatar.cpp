// Avatar.cpp
#include "Avatar.h"

#include "Theme.h"

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

Avatar::Avatar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

void Avatar::setText(const QString& text) {
    text_ = text.left(1).toUpper();
    update();
}

void Avatar::setColor(const QColor& color) {
    color_ = color;
    update();
}

void Avatar::setFontPx(int px) {
    fontPx_ = px;
    update();
}

QColor Avatar::colorForId(qint64 id) {
    return theme::avatarColorFor(id);
}

void Avatar::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0, 0, -1, -1);
    p.setPen(Qt::NoPen);
    p.setBrush(color_);
    p.drawEllipse(r);

    if (text_.isEmpty()) return;

    QFont f = font();
    f.setPixelSize(fontPx_);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, text_);
}
