/*
 * 文件名:    PlayButton.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CPlayButton 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CPlayButton.cpp */
#include "PlayButton.h"

#include "Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

/*
 * 功能描述:  执行 CPlayButton 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CPlayButton::CPlayButton(QWidget* parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(22, 22);
    setToolTip(QStringLiteral("播放发音"));
}

/*
 * 功能描述:  执行 setPlaying 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  playing  参数。
 * return:    void
 */
void CPlayButton::setPlaying(bool playing) {
    if (m_nPlaying == playing) return;
    m_nPlaying = playing;
    setToolTip(playing ? QStringLiteral("停止播放") : QStringLiteral("播放发音"));
    update();
}

/*
 * 功能描述:  执行 enterEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CPlayButton::enterEvent(QEnterEvent* event) {
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
void CPlayButton::leaveEvent(QEvent* event) {
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
void CPlayButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    const QPointF c = r.center();
    const QColor accent = theme::accent;

    if (m_nHovered && isEnabled()) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#E8F0FA"));
        p.drawEllipse(r);
    }

    p.setPen(Qt::NoPen);
    p.setBrush(isEnabled() ? accent : QColor("#B9C2CE"));

    if (m_nPlaying) {
        /* 播放中：两根竖条（暂停） */
        p.drawRoundedRect(QRectF(c.x() - 4.0, c.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        p.drawRoundedRect(QRectF(c.x() + 1.4, c.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        return;
    }

    /* 待播放：三角形 */
    QPainterPath triangle;
    triangle.moveTo(c.x() - 3.2, c.y() - 5.0);
    triangle.lineTo(c.x() + 5.2, c.y());
    triangle.lineTo(c.x() - 3.2, c.y() + 5.0);
    triangle.closeSubpath();
    p.drawPath(triangle);
}
