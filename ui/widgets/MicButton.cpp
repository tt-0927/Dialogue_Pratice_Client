/*
 * 文件名:    MicButton.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMicButton 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMicButton.cpp */
#include "ui/widgets/MicButton.h"

#include "core/Theme.h"

#include <QPainter>
#include <QPaintEvent>

/*
 * 功能描述:  执行 CMicButton 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CMicButton::CMicButton(QWidget* parent) : QAbstractButton(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(40, 40);
    setToolTip(QStringLiteral("点击开始语音输入"));
}

/*
 * 功能描述:  执行 setRecording 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  recording  参数。
 * return:    void
 */
void CMicButton::setRecording(bool recording) {
    if (m_nRecording == recording) return;
    m_nRecording = recording;
    setToolTip(recording ? QStringLiteral("点击结束录音并识别")
                         : QStringLiteral("点击开始语音输入"));
    update();
}

/*
 * 功能描述:  执行 enterEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CMicButton::enterEvent(QEnterEvent* event) {
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
void CMicButton::leaveEvent(QEvent* event) {
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
void CMicButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QColor bg = m_nRecording ? QColor("#FDE7E7") : QColor("#F2F5F9");
    QColor fg = m_nRecording ? QColor("#E5484D") : theme::accent;

    if (!isEnabled()) {
        bg = QColor("#F4F6F9");
        fg = QColor("#B9C2CE");
    } else if (m_nHovered && !m_nRecording) {
        bg = QColor("#E8EEF6");
    } else if (m_nHovered && m_nRecording) {
        bg = QColor("#FBD9D9");
    }

    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawEllipse(r);

    const QPointF c = r.center();

    /* 麦克风头 */
    p.setBrush(fg);
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(QRectF(c.x() - 4.0, c.y() - 12.0, 8.0, 14.0), 4.0, 4.0);

    /* 支架：下半圆弧 + 立杆 + 底座 */
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(fg, 1.8, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(QRectF(c.x() - 7.0, c.y() - 8.0, 14.0, 14.0), 200 * 16, 140 * 16);
    p.drawLine(QPointF(c.x(), c.y() + 6.0), QPointF(c.x(), c.y() + 10.5));
    p.drawLine(QPointF(c.x() - 4.0, c.y() + 10.5), QPointF(c.x() + 4.0, c.y() + 10.5));

    /* 录音中：右上角红点提示 */
    if (m_nRecording) {
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#E5484D"));
        p.drawEllipse(QRectF(r.right() - 9.0, r.top() + 3.0, 6.0, 6.0));
    }
}
