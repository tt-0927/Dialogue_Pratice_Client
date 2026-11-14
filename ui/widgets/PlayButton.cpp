/**
 * @file       PlayButton.cpp
 * @brief      CPlayButton 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CPlayButton.cpp。
 */
#include "ui/widgets/PlayButton.h"

#include "core/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

/**
 * @brief      构造 CPlayButton 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QAbstractButton(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CPlayButton::CPlayButton(QWidget* pParent) : QAbstractButton(pParent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(22, 22);
    setToolTip(QStringLiteral("播放发音"));
}

/**
 * @brief      设置 playing相关的数据并返回处理结果。
 * @param[in] nPlaying       输入数据；类型 bool。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CPlayButton::setPlaying(bool nPlaying) {
    if (m_nPlaying == nPlaying) return;
    m_nPlaying = nPlaying;
    setToolTip(nPlaying ? QStringLiteral("停止播放") : QStringLiteral("播放发音"));
    update();
}

/**
 * @brief      进入事件。
 * @param[in] pEvent         事件；类型 QEnterEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CPlayButton::enterEvent(QEnterEvent* pEvent) {
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
void CPlayButton::leaveEvent(QEvent* pEvent) {
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
void CPlayButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter stP(this);
    stP.setRenderHint(QPainter::Antialiasing, true);

    const QRectF stR = rect().adjusted(0.5, 0.5, -0.5, -0.5);
    const QPointF stC = stR.center();
    const QColor stAccent = theme::accent;

    if (m_nHovered && isEnabled()) {
        stP.setPen(Qt::NoPen);
        stP.setBrush(QColor("#E8F0FA"));
        stP.drawEllipse(stR);
    }

    stP.setPen(Qt::NoPen);
    stP.setBrush(isEnabled() ? stAccent : QColor("#B9C2CE"));

    if (m_nPlaying) {
        /**
         * @brief  播放中：两根竖条（暂停）。
         */
        stP.drawRoundedRect(QRectF(stC.x() - 4.0, stC.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        stP.drawRoundedRect(QRectF(stC.x() + 1.4, stC.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        return;
    }

    /**
     * @brief  待播放：三角形。
     */
    QPainterPath stTriangle;
    stTriangle.moveTo(stC.x() - 3.2, stC.y() - 5.0);
    stTriangle.lineTo(stC.x() + 5.2, stC.y());
    stTriangle.lineTo(stC.x() - 3.2, stC.y() + 5.0);
    stTriangle.closeSubpath();
    stP.drawPath(stTriangle);
}
