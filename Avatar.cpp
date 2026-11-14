/**
 * @file       Avatar.cpp
 * @brief      CAvatar 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CAvatar.cpp。
 */
#include "Avatar.h"

#include "Theme.h"

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

/**
 * @brief      构造 CAvatar 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QWidget(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CAvatar::CAvatar(QWidget* pParent) : QWidget(pParent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

/**
 * @brief      设置文本。
 * @param[in] strText        消息文本；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CAvatar::setText(const QString& strText) {
    m_strText = strText.left(1).toUpper();
    update();
}

/**
 * @brief      设置颜色。
 * @param[in] stColor        颜色；类型 const QColor&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CAvatar::setColor(const QColor& stColor) {
    m_stColor = stColor;
    update();
}

/**
 * @brief      设置字体 px。
 * @param[in] nPx            输入数据；类型 int。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CAvatar::setFontPx(int nPx) {
    m_nFontPx = nPx;
    update();
}

/**
 * @brief      获取颜色 for 编号。
 * @param[in] lId            编号；类型 qint64。
 * @return     返回 QColor CAvatar:: 类型的处理结果。
 * @author     JaTin
 */
QColor CAvatar::colorForId(qint64 lId) {
    return theme::avatarColorFor(lId);
}

/**
 * @brief      绘制事件。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CAvatar::paintEvent(QPaintEvent* /*event*/) {
    QPainter stP(this);
    stP.setRenderHint(QPainter::Antialiasing, true);

    const QRectF stR = rect().adjusted(0, 0, -1, -1);
    stP.setPen(Qt::NoPen);
    stP.setBrush(m_stColor);
    stP.drawEllipse(stR);

    if (m_strText.isEmpty()) return;

    QFont stF = font();
    stF.setPixelSize(m_nFontPx);
    stF.setBold(true);
    stP.setFont(stF);
    stP.setPen(Qt::white);
    stP.drawText(stR, Qt::AlignCenter, m_strText);
}
