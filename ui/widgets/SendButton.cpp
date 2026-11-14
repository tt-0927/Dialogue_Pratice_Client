/**
 * @file       SendButton.cpp
 * @brief      CSendButton 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CSendButton.cpp。
 */
#include "ui/widgets/SendButton.h"

#include "core/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>

/**
 * @brief      构造 CSendButton 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QAbstractButton(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CSendButton::CSendButton(QWidget* pParent) : QAbstractButton(pParent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(40, 40);
    setToolTip(QStringLiteral("发送 (Enter)"));
}

/**
 * @brief      进入事件。
 * @param[in] pEvent         事件；类型 QEnterEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSendButton::enterEvent(QEnterEvent* pEvent) {
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
void CSendButton::leaveEvent(QEvent* pEvent) {
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
void CSendButton::paintEvent(QPaintEvent* /*event*/) {
    QPainter stP(this);
    stP.setRenderHint(QPainter::Antialiasing, true);

    const QRectF stR = rect().adjusted(0.5, 0.5, -0.5, -0.5);

    QColor stBg = theme::accent;
    if (!isEnabled()) {
        stBg = QColor("#B9D4F2");
    } else if (isDown()) {
        stBg = theme::accentPressed;
    } else if (m_nHovered) {
        stBg = theme::accentHover;
    }

    stP.setPen(Qt::NoPen);
    stP.setBrush(stBg);
    stP.drawEllipse(stR);

    /**
     * @brief  纸飞机：以 40x40 为基准绘制。
     */
    const qreal stS = stR.width() / 40.0;
    stP.translate(stR.topLeft());
    stP.scale(stS, stS);

    QPainterPath stPlane;
    stPlane.moveTo(10.0, 19.6);
    stPlane.lineTo(30.0, 10.8);
    stPlane.lineTo(23.2, 29.2);
    stPlane.lineTo(19.4, 21.3);
    stPlane.closeSubpath();

    stP.setBrush(Qt::white);
    stP.setPen(Qt::NoPen);
    stP.drawPath(stPlane);
}
