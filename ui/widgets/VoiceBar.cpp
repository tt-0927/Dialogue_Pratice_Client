/**
 * @file       VoiceBar.cpp
 * @brief      CVoiceBar 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoiceBar.cpp。
 */
#include "ui/widgets/VoiceBar.h"

#include "core/Theme.h"

#include <QCryptographicHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

/**
 * @brief  统计工具条。
 * @note   取值 26。
 */
constexpr int kBarCount = 26;
/**
 * @brief  k工具条Gap。
 * @note   取值 2。
 */
constexpr int kBarGap = 2;
/**
 * @brief  k工具条Width。
 * @note   取值 3。
 */
constexpr int kBarWidth = 3;
/**
 * @brief  kSidePadding。
 * @note   取值 10。
 */
constexpr int kSidePadding = 10;
/**
 * @brief  播放area。
 * @note   取值 22。
 */
constexpr int kPlayArea = 22;

/**
 * @brief      把毫秒时长格式化为分秒字符串。
 * @param[in] nMs            输入数据；类型 int。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString formatDuration(int nMs) {
    const int nTotalSeconds = qMax(1, nMs / 1000);
    return QStringLiteral("%1:%2").arg(nTotalSeconds / 60).arg(nTotalSeconds % 60, 2, 10, QLatin1Char('0'));
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CVoiceBar 对象，完成成员初始化与依赖注入。
 * @param[in] nDurationMs    语音时长，单位毫秒；类型 int。
 * @param[in] byContainer    容器；类型 const QByteArray&。
 * @param[in] nOutgoing      输入数据；类型 bool。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CVoiceBar::CVoiceBar(int nDurationMs, const QByteArray& byContainer, bool nOutgoing, QWidget* pParent)
    : QWidget(pParent), m_nDurationMs(qMax(0, nDurationMs)), m_nOutgoing(nOutgoing) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("点击播放语音"));
    setAttribute(Qt::WA_Hover, true);

    /**
     * @brief  用容器哈希生成稳定的柱高：同一条语音每次显示一致。
     */
    const QByteArray byHash = QCryptographicHash::hash(byContainer, QCryptographicHash::Md5);
    m_aBars.reserve(kBarCount);
    for (int i = 0; i < kBarCount; ++i) {
        const auto stByte = static_cast<unsigned char>(byHash.at(i % byHash.size()));
        /**
         * @brief  0.25 ~ 1.0，避免柱子太矮看不出形状。
         */
        m_aBars.append(0.25 + (stByte % 100) / 100.0 * 0.75);
    }
}

/**
 * @brief      设置 playing相关的数据并返回处理结果。
 * @param[in] nPlaying       输入数据；类型 bool。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceBar::setPlaying(bool nPlaying) {
    if (m_nPlaying == nPlaying) return;
    m_nPlaying = nPlaying;
    setToolTip(nPlaying ? QStringLiteral("点击重新播放") : QStringLiteral("点击播放语音"));
    update();
}

/**
 * @brief      获取长度 hint。
 * @note       本函数无输入参数。
 * @return     返回 QSize CVoiceBar:: 类型的处理结果。
 * @author     JaTin
 */
QSize CVoiceBar::sizeHint() const {
    /**
     * @brief  时长越长条越宽，但限制在 90~230 之间，避免长语音撑爆气泡。
     */
    const int nWaveWidth = kBarCount * (kBarWidth + kBarGap) - kBarGap;
    const int nExtra = qBound(0, m_nDurationMs / 120, 120);
    const int nWidth = kSidePadding * 2 + kPlayArea + 8 + nWaveWidth + nExtra + 34;
    return QSize(qBound(150, nWidth, 260), 34);
}

/**
 * @brief      绘制事件。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter stP(this);
    stP.setRenderHint(QPainter::Antialiasing, true);

    const QColor stAccent = theme::accent;
    const QColor stWaveColor = m_nOutgoing ? QColor("#5F9E3F") : QColor("#7A9BC0");
    const QColor stDimColor = m_nOutgoing ? QColor("#8FBF74") : QColor("#C3D2E2");

    const QRectF stR = rect();
    const qreal stCenterY = stR.center().y();

    /**
     * @brief  播放/暂停图标。
     */
    const QPointF stIconCenter(kSidePadding + kPlayArea / 2.0, stCenterY);
    stP.setPen(Qt::NoPen);
    stP.setBrush(m_nHovered ? stAccent : (m_nOutgoing ? stWaveColor : stAccent));
    if (m_nPlaying) {
        stP.drawRoundedRect(QRectF(stIconCenter.x() - 4.0, stIconCenter.y() - 5.0, 2.8, 10.0), 1.0, 1.0);
        stP.drawRoundedRect(QRectF(stIconCenter.x() + 1.2, stIconCenter.y() - 5.0, 2.8, 10.0), 1.0, 1.0);
    } else {
        QPainterPath stTriangle;
        stTriangle.moveTo(stIconCenter.x() - 3.5, stIconCenter.y() - 5.5);
        stTriangle.lineTo(stIconCenter.x() + 5.5, stIconCenter.y());
        stTriangle.lineTo(stIconCenter.x() - 3.5, stIconCenter.y() + 5.5);
        stTriangle.closeSubpath();
        stP.drawPath(stTriangle);
    }

    /**
     * @brief  波形。
     */
    const qreal stWaveLeft = kSidePadding + kPlayArea + 8;
    const qreal stWaveWidth = stR.width() - stWaveLeft - 40;
    const qreal stStep = (kBarWidth + kBarGap);
    const int nVisibleBars = qMax(1, qMin(kBarCount, static_cast<int>(stWaveWidth / stStep)));

    for (int i = 0; i < nVisibleBars; ++i) {
        const qreal stH = m_aBars.at(i) * 16.0;
        const QRectF stBar(stWaveLeft + i * stStep, stCenterY - stH / 2.0, kBarWidth, stH);
        stP.setBrush(m_nPlaying ? stWaveColor : stDimColor);
        stP.drawRoundedRect(stBar, 1.2, 1.2);
    }

    /**
     * @brief  时长。
     */
    stP.setPen(m_nOutgoing ? QColor("#4F7F35") : QColor("#8A94A6"));
    QFont stF = font();
    stF.setPointSizeF(8.5);
    stP.setFont(stF);
    const QString strText = formatDuration(m_nDurationMs);
    stP.drawText(QRectF(stR.right() - 38, stR.top(), 34, stR.height()),
               Qt::AlignRight | Qt::AlignVCenter, strText);
}

/**
 * @brief      执行 mousepress 事件。
 * @param[in] pEvent         事件；类型 QMouseEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceBar::mousePressEvent(QMouseEvent* pEvent) {
    if (pEvent->button() == Qt::LeftButton) emit playRequested();
    QWidget::mousePressEvent(pEvent);
}

/**
 * @brief      进入事件。
 * @param[in] pEvent         事件；类型 QEnterEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceBar::enterEvent(QEnterEvent* pEvent) {
    m_nHovered = true;
    update();
    QWidget::enterEvent(pEvent);
}

/**
 * @brief      退出事件。
 * @param[in] pEvent         事件；类型 QEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceBar::leaveEvent(QEvent* pEvent) {
    m_nHovered = false;
    update();
    QWidget::leaveEvent(pEvent);
}
