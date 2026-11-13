/*
 * 文件名:    VoiceBar.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CVoiceBar 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CVoiceBar.cpp */
#include "ui/widgets/VoiceBar.h"

#include "core/Theme.h"

#include <QCryptographicHash>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kBarCount = 26;
constexpr int kBarGap = 2;
constexpr int kBarWidth = 3;
constexpr int kSidePadding = 10;
constexpr int kPlayArea = 22;

/*
 * 功能描述:  执行 formatDuration 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  ms  参数。
 * return:    返回值，含义见功能描述。
 */
QString formatDuration(int ms) {
    const int totalSeconds = qMax(1, ms / 1000);
    return QStringLiteral("%1:%2").arg(totalSeconds / 60).arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

/* namespace */
}

CVoiceBar::CVoiceBar(int durationMs, const QByteArray& container, bool outgoing, QWidget* parent)
    : QWidget(parent), m_nDurationMs(qMax(0, durationMs)), m_nOutgoing(outgoing) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("点击播放语音"));
    setAttribute(Qt::WA_Hover, true);

    /* 用容器哈希生成稳定的柱高：同一条语音每次显示一致 */
    const QByteArray hash = QCryptographicHash::hash(container, QCryptographicHash::Md5);
    m_aBars.reserve(kBarCount);
    for (int i = 0; i < kBarCount; ++i) {
        const auto byte = static_cast<unsigned char>(hash.at(i % hash.size()));
        /* 0.25 ~ 1.0，避免柱子太矮看不出形状 */
        m_aBars.append(0.25 + (byte % 100) / 100.0 * 0.75);
    }
}

/*
 * 功能描述:  执行 setPlaying 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  playing  参数。
 * return:    void
 */
void CVoiceBar::setPlaying(bool playing) {
    if (m_nPlaying == playing) return;
    m_nPlaying = playing;
    setToolTip(playing ? QStringLiteral("点击重新播放") : QStringLiteral("点击播放语音"));
    update();
}

/*
 * 功能描述:  执行 sizeHint 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
QSize CVoiceBar::sizeHint() const {
    /* 时长越长条越宽，但限制在 90~230 之间，避免长语音撑爆气泡 */
    const int waveWidth = kBarCount * (kBarWidth + kBarGap) - kBarGap;
    const int extra = qBound(0, m_nDurationMs / 120, 120);
    const int width = kSidePadding * 2 + kPlayArea + 8 + waveWidth + extra + 34;
    return QSize(qBound(150, width, 260), 34);
}

/*
 * 功能描述:  执行 paintEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CVoiceBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor accent = theme::accent;
    const QColor waveColor = m_nOutgoing ? QColor("#5F9E3F") : QColor("#7A9BC0");
    const QColor dimColor = m_nOutgoing ? QColor("#8FBF74") : QColor("#C3D2E2");

    const QRectF r = rect();
    const qreal centerY = r.center().y();

    /* 播放/暂停图标 */
    const QPointF iconCenter(kSidePadding + kPlayArea / 2.0, centerY);
    p.setPen(Qt::NoPen);
    p.setBrush(m_nHovered ? accent : (m_nOutgoing ? waveColor : accent));
    if (m_nPlaying) {
        p.drawRoundedRect(QRectF(iconCenter.x() - 4.0, iconCenter.y() - 5.0, 2.8, 10.0), 1.0, 1.0);
        p.drawRoundedRect(QRectF(iconCenter.x() + 1.2, iconCenter.y() - 5.0, 2.8, 10.0), 1.0, 1.0);
    } else {
        QPainterPath triangle;
        triangle.moveTo(iconCenter.x() - 3.5, iconCenter.y() - 5.5);
        triangle.lineTo(iconCenter.x() + 5.5, iconCenter.y());
        triangle.lineTo(iconCenter.x() - 3.5, iconCenter.y() + 5.5);
        triangle.closeSubpath();
        p.drawPath(triangle);
    }

    /* 波形 */
    const qreal waveLeft = kSidePadding + kPlayArea + 8;
    const qreal waveWidth = r.width() - waveLeft - 40;
    const qreal step = (kBarWidth + kBarGap);
    const int visibleBars = qMax(1, qMin(kBarCount, static_cast<int>(waveWidth / step)));

    for (int i = 0; i < visibleBars; ++i) {
        const qreal h = m_aBars.at(i) * 16.0;
        const QRectF bar(waveLeft + i * step, centerY - h / 2.0, kBarWidth, h);
        p.setBrush(m_nPlaying ? waveColor : dimColor);
        p.drawRoundedRect(bar, 1.2, 1.2);
    }

    /* 时长 */
    p.setPen(m_nOutgoing ? QColor("#4F7F35") : QColor("#8A94A6"));
    QFont f = font();
    f.setPointSizeF(8.5);
    p.setFont(f);
    const QString text = formatDuration(m_nDurationMs);
    p.drawText(QRectF(r.right() - 38, r.top(), 34, r.height()),
               Qt::AlignRight | Qt::AlignVCenter, text);
}

/*
 * 功能描述:  执行 mousePressEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CVoiceBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) emit playRequested();
    QWidget::mousePressEvent(event);
}

/*
 * 功能描述:  执行 enterEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CVoiceBar::enterEvent(QEnterEvent* event) {
    m_nHovered = true;
    update();
    QWidget::enterEvent(event);
}

/*
 * 功能描述:  执行 leaveEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CVoiceBar::leaveEvent(QEvent* event) {
    m_nHovered = false;
    update();
    QWidget::leaveEvent(event);
}
