/**
 * @file       MicButton.cpp
 * @brief      CMicButton 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CMicButton.cpp。
 */
#include "ui/widgets/MicButton.h"

#include "core/Theme.h"

#include <QPainter>
#include <QPaintEvent>

/**
 * @brief      构造 CMicButton 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QAbstractButton(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CMicButton::CMicButton(QWidget* pParent) : QAbstractButton(pParent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(40, 40);
    setToolTip(QStringLiteral("点击开始语音输入"));
}

/**
 * @brief      设置 recording相关的数据并返回处理结果。
 * @param[in] nRecording     输入数据；类型 bool。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMicButton::setRecording(bool nRecording) {
    if (m_nRecording == nRecording) return;
    m_nRecording = nRecording;
    setToolTip(nRecording ? QStringLiteral("点击结束录音并识别")
                         : QStringLiteral("点击开始语音输入"));
    update();
}

/**
 * @brief      进入事件。
 * @param[in] pEvent         事件；类型 QEnterEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMicButton::enterEvent(QEnterEvent* pEvent) {
    m_nHovered = true;
    update();
    QAbstractButton::enterEvent(pEvent);
}

/**
 * @brief      退出事件。
 * @param[in] pEvent         事件；类型 QEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMicButton::leaveEvent(QEvent* pEvent) {
    m_nHovered = false;
    update();
    QAbstractButton::leaveEvent(pEvent);
}

/**
 * @brief      绘制事件。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMicButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter stP(this);
    stP.setRenderHint(QPainter::Antialiasing, true);

    const QRectF stR = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QColor stBg = m_nRecording ? QColor("#FDE7E7") : QColor("#F2F5F9");
    QColor stFg = m_nRecording ? QColor("#E5484D") : theme::accent;

    if (!isEnabled()) {
        stBg = QColor("#F4F6F9");
        stFg = QColor("#B9C2CE");
    } else if (m_nHovered && !m_nRecording) {
        stBg = QColor("#E8EEF6");
    } else if (m_nHovered && m_nRecording) {
        stBg = QColor("#FBD9D9");
    }

    stP.setPen(Qt::NoPen);
    stP.setBrush(stBg);
    stP.drawEllipse(stR);

    const QPointF stC = stR.center();

    /**
     * @brief  麦克风头。
     */
    stP.setBrush(stFg);
    stP.setPen(Qt::NoPen);
    stP.drawRoundedRect(QRectF(stC.x() - 4.0, stC.y() - 12.0, 8.0, 14.0), 4.0, 4.0);

    /**
     * @brief  支架：下半圆弧 + 立杆 + 底座。
     */
    stP.setBrush(Qt::NoBrush);
    stP.setPen(QPen(stFg, 1.8, Qt::SolidLine, Qt::RoundCap));
    stP.drawArc(QRectF(stC.x() - 7.0, stC.y() - 8.0, 14.0, 14.0), 200 * 16, 140 * 16);
    stP.drawLine(QPointF(stC.x(), stC.y() + 6.0), QPointF(stC.x(), stC.y() + 10.5));
    stP.drawLine(QPointF(stC.x() - 4.0, stC.y() + 10.5), QPointF(stC.x() + 4.0, stC.y() + 10.5));

    /**
     * @brief  录音中：右上角红点提示。
     */
    if (m_nRecording) {
        stP.setPen(Qt::NoPen);
        stP.setBrush(QColor("#E5484D"));
        stP.drawEllipse(QRectF(stR.right() - 9.0, stR.top() + 3.0, 6.0, 6.0));
    }
}
