/*
 * 文件名:    UiMetrics.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  界面尺寸策略：桌面与移动端共用一套代码，按可用宽度切换。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#pragma once

#include <QtGlobal>

namespace core {
namespace ui {

/* 低于该宽度进入紧凑模式（手机竖屏通常 360–420）。 */
inline constexpr int kCompactBreakpoint = 600;

/* 触摸目标的最小边长（Material / HIG 的常见建议值是 44–48）。 */
inline constexpr int kTouchTarget = 44;

/* 卡片类容器的最大宽度（桌面观感上限，窄屏会被压到可用宽度）。 */
inline constexpr int kCardMaxWidth = 420;

/* 页面四周的呼吸边距。 */
inline constexpr int kPageMargin = 24;

/**
 * @brief 当前宽度是否属于紧凑模式。
 * @param width 可用宽度（像素）。
 * @retval true 应按移动端布局处理。
 */
inline bool isCompact(int width) {
    return width < kCompactBreakpoint;
}

/**
 * @brief 计算卡片宽度。
 * @param availableWidth 可用宽度。
 * @param maxWidth       桌面下的宽度上限。
 * @retval 窄屏时为「可用宽度 - 两侧边距」，宽屏时为 maxWidth。
 * @details 两侧各留 `kPageMargin` 的一半，避免卡片贴边。
 */
inline int cardWidth(int availableWidth, int maxWidth = kCardMaxWidth) {
    const int usable = availableWidth - kPageMargin * 2;
    return qBound(240, qMin(usable, maxWidth), maxWidth);
}

/**
 * @brief 计算气泡的最大宽度。
 * @param availableWidth 消息区可用宽度。
 * @retval 窄屏按比例收缩，宽屏保持上限。
 * @details 桌面 520 是观感上限；窄屏取 78%，给头像与留白留出空间。
 */
inline int bubbleMaxWidth(int availableWidth) {
    return qBound(180, qMin(520, static_cast<int>(availableWidth * 0.78)), 520);
}

/* namespace ui */
}
/* namespace core */
}
