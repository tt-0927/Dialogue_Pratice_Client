/*
 * 文件名:    Avatar.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CAvatar 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CAvatar.cpp */
#include "ui/widgets/Avatar.h"

#include "core/Theme.h"

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

/*
 * 功能描述:  构造 CAvatar 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QWidget(。
 * return:    无返回值（构造函数或析构函数）。
 */
CAvatar::CAvatar(QWidget* pParent) : QWidget(pParent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

/*
 * 功能描述:  设置文本。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CAvatar::setText(const QString& strText) {
    m_strText = strText.left(1).toUpper();
    update();
}

/*
 * 功能描述:  设置颜色。
 * 作者:      JaTin
 * param [in]   stColor          颜色，类型 const QColor&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CAvatar::setColor(const QColor& stColor) {
    m_stColor = stColor;
    update();
}

/*
 * 功能描述:  设置字体 px。
 * 作者:      JaTin
 * param [in]   nPx              输入数据，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CAvatar::setFontPx(int nPx) {
    m_nFontPx = nPx;
    update();
}

/*
 * 功能描述:  获取颜色 for 编号。
 * 作者:      JaTin
 * param [in]   lId              编号，类型 qint64。
 * return:    返回 QColor 类型的处理结果，具体含义见功能描述。
 */
QColor CAvatar::colorForId(qint64 lId) {
    return theme::avatarColorFor(lId);
}

/*
 * 功能描述:  绘制事件。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
