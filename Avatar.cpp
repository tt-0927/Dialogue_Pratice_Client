/*
 * 文件名:    Avatar.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CAvatar 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CAvatar.cpp */
#include "Avatar.h"

#include "Theme.h"

#include <QFont>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>

/*
 * 功能描述:  执行 CAvatar 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CAvatar::CAvatar(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
}

/*
 * 功能描述:  执行 setText 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CAvatar::setText(const QString& text) {
    m_strText = text.left(1).toUpper();
    update();
}

/*
 * 功能描述:  执行 setColor 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  color  参数。
 * return:    void
 */
void CAvatar::setColor(const QColor& color) {
    m_stColor = color;
    update();
}

/*
 * 功能描述:  执行 setFontPx 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  px  参数。
 * return:    void
 */
void CAvatar::setFontPx(int px) {
    m_nFontPx = px;
    update();
}

/*
 * 功能描述:  执行 colorForId 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  id  参数。
 * return:    返回值，含义见功能描述。
 */
QColor CAvatar::colorForId(qint64 id) {
    return theme::avatarColorFor(id);
}

/*
 * 功能描述:  执行 paintEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CAvatar::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QRectF r = rect().adjusted(0, 0, -1, -1);
    p.setPen(Qt::NoPen);
    p.setBrush(m_stColor);
    p.drawEllipse(r);

    if (m_strText.isEmpty()) return;

    QFont f = font();
    f.setPixelSize(m_nFontPx);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(r, Qt::AlignCenter, m_strText);
}
