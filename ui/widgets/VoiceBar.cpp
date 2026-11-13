// VoiceBar.cpp
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

QString formatDuration(int ms) {
    const int totalSeconds = qMax(1, ms / 1000);
    return QStringLiteral("%1:%2").arg(totalSeconds / 60).arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

}  // namespace

VoiceBar::VoiceBar(int durationMs, const QByteArray& container, bool outgoing, QWidget* parent)
    : QWidget(parent), durationMs_(qMax(0, durationMs)), outgoing_(outgoing) {
    setCursor(Qt::PointingHandCursor);
    setToolTip(QStringLiteral("点击播放语音"));
    setAttribute(Qt::WA_Hover, true);

    // 用容器哈希生成稳定的柱高：同一条语音每次显示一致
    const QByteArray hash = QCryptographicHash::hash(container, QCryptographicHash::Md5);
    bars_.reserve(kBarCount);
    for (int i = 0; i < kBarCount; ++i) {
        const auto byte = static_cast<unsigned char>(hash.at(i % hash.size()));
        // 0.25 ~ 1.0，避免柱子太矮看不出形状
        bars_.append(0.25 + (byte % 100) / 100.0 * 0.75);
    }
}

void VoiceBar::setPlaying(bool playing) {
    if (playing_ == playing) return;
    playing_ = playing;
    setToolTip(playing ? QStringLiteral("点击重新播放") : QStringLiteral("点击播放语音"));
    update();
}

QSize VoiceBar::sizeHint() const {
    // 时长越长条越宽，但限制在 90~230 之间，避免长语音撑爆气泡
    const int waveWidth = kBarCount * (kBarWidth + kBarGap) - kBarGap;
    const int extra = qBound(0, durationMs_ / 120, 120);
    const int width = kSidePadding * 2 + kPlayArea + 8 + waveWidth + extra + 34;
    return QSize(qBound(150, width, 260), 34);
}

void VoiceBar::paintEvent(QPaintEvent* /*event*/) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QColor accent = theme::accent;
    const QColor waveColor = outgoing_ ? QColor("#5F9E3F") : QColor("#7A9BC0");
    const QColor dimColor = outgoing_ ? QColor("#8FBF74") : QColor("#C3D2E2");

    const QRectF r = rect();
    const qreal centerY = r.center().y();

    // 播放/暂停图标
    const QPointF iconCenter(kSidePadding + kPlayArea / 2.0, centerY);
    p.setPen(Qt::NoPen);
    p.setBrush(hovered_ ? accent : (outgoing_ ? waveColor : accent));
    if (playing_) {
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

    // 波形
    const qreal waveLeft = kSidePadding + kPlayArea + 8;
    const qreal waveWidth = r.width() - waveLeft - 40;
    const qreal step = (kBarWidth + kBarGap);
    const int visibleBars = qMax(1, qMin(kBarCount, static_cast<int>(waveWidth / step)));

    for (int i = 0; i < visibleBars; ++i) {
        const qreal h = bars_.at(i) * 16.0;
        const QRectF bar(waveLeft + i * step, centerY - h / 2.0, kBarWidth, h);
        p.setBrush(playing_ ? waveColor : dimColor);
        p.drawRoundedRect(bar, 1.2, 1.2);
    }

    // 时长
    p.setPen(outgoing_ ? QColor("#4F7F35") : QColor("#8A94A6"));
    QFont f = font();
    f.setPointSizeF(8.5);
    p.setFont(f);
    const QString text = formatDuration(durationMs_);
    p.drawText(QRectF(r.right() - 38, r.top(), 34, r.height()),
               Qt::AlignRight | Qt::AlignVCenter, text);
}

void VoiceBar::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) emit playRequested();
    QWidget::mousePressEvent(event);
}

void VoiceBar::enterEvent(QEnterEvent* event) {
    hovered_ = true;
    update();
    QWidget::enterEvent(event);
}

void VoiceBar::leaveEvent(QEvent* event) {
    hovered_ = false;
    update();
    QWidget::leaveEvent(event);
}
