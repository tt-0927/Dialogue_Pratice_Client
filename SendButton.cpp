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
 * 功能描述:  构造 CSendButton 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QAbstractButton(。
 * return:    无返回值（构造函数或析构函数）。
 */
CSendButton::CSendButton(QWidget* pParent) : QAbstractButton(pParent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(40, 40);
    setToolTip(QStringLiteral("发送 (Enter)"));
}

/*
 * 功能描述:  进入事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 QEnterEvent*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSendButton::enterEvent(QEnterEvent* pEvent) {
    m_nHovered = true;
    update();
    QAbstractButton::enterEvent(pEvent);
}

/*
 * 功能描述:  退出事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 QEvent*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSendButton::leaveEvent(QEvent* pEvent) {
    m_nHovered = false;
    update();
    QAbstractButton::leaveEvent(pEvent);
}

/*
 * 功能描述:  绘制事件。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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

    /* 纸飞机：以 40x40 为基准绘制 */
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
