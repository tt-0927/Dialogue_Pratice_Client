/*
 * 文件名:    SendButton.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CSendButton 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CSendButton.cpp */
#include "SendButton.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

/*
 * 功能描述:  执行 CSendButton 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CSendButton::CSendButton(QWidget* parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(40, 40);
    setToolTip(QStringLiteral("发送 (Enter)"));
}

/*
 * 功能描述:  执行 enterEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CSendButton::enterEvent(QEnterEvent* event) {
    m_nHovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

/*
 * 功能描述:  执行 leaveEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CSendButton::leaveEvent(QEvent* event) {
    m_nHovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

/*
 * 功能描述:  执行 paintEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CSendButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bg = theme::accent;
    if (!isEnabled()) {
        bg = QColor("#B9D4F2");
    } else if (isDown()) {
        bg = theme::accentPressed;
    } else if (m_nHovered) {
        bg = theme::accentHover;
    }

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(r);

    /* 纸飞机：以 40x40 为基准绘制 */
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
