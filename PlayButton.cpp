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
 * 功能描述:  构造 CPlayButton 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QAbstractButton(。
 * return:    无返回值（构造函数或析构函数）。
 */
CPlayButton::CPlayButton(QWidget* pParent) : QAbstractButton(pParent) {
    setCursor(Qt::PointingHandCursor);
    setFixedSize(22, 22);
    setToolTip(QStringLiteral("播放发音"));
}

/*
 * 功能描述:  设置playing。
 * 作者:      JaTin
 * param [in]   nPlaying         输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CPlayButton::setPlaying(bool nPlaying) {
    if (m_nPlaying == nPlaying) return;
    m_nPlaying = nPlaying;
    setToolTip(nPlaying ? QStringLiteral("停止播放") : QStringLiteral("播放发音"));
    update();
}

/*
 * 功能描述:  进入事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 QEnterEvent*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CPlayButton::enterEvent(QEnterEvent* pEvent) {
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
void CPlayButton::leaveEvent(QEvent* pEvent) {
    m_nHovered = false;
    update();
    QAbstractButton::leaveEvent(pEvent);
}

/*
 * 功能描述:  绘制事件。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
        /* 播放中：两根竖条（暂停） */
        stP.drawRoundedRect(QRectF(stC.x() - 4.0, stC.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        stP.drawRoundedRect(QRectF(stC.x() + 1.4, stC.y() - 4.5, 2.6, 9.0), 1.0, 1.0);
        return;
    }

    /* 待播放：三角形 */
    QPainterPath stTriangle;
    stTriangle.moveTo(stC.x() - 3.2, stC.y() - 5.0);
    stTriangle.lineTo(stC.x() + 5.2, stC.y());
    stTriangle.lineTo(stC.x() - 3.2, stC.y() + 5.0);
    stTriangle.closeSubpath();
    stP.drawPath(stTriangle);
}
