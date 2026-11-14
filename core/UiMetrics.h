/**
 * @file       UiMetrics.h
 * @brief      界面尺寸策略：桌面与移动端共用一套代码，按可用宽度切换。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

#pragma once

#include <QtGlobal>

namespace core {
namespace ui {

/**
 * @brief  低于该宽度进入紧凑模式（手机竖屏通常 360–420）。
 * @note   取值 600。
 */
inline constexpr int kCompactBreakpoint = 600;

/**
 * @brief  触摸目标的最小边长（Material / HIG 的常见建议值是 44–48）。
 * @note   取值 44。
 */
inline constexpr int kTouchTarget = 44;

/**
 * @brief  卡片类容器的最大宽度（桌面观感上限，窄屏会被压到可用宽度）。
 * @note   取值 420。
 */
inline constexpr int kCardMaxWidth = 420;

/**
 * @brief  页面四周的呼吸边距。
 * @note   取值 24。
 */
inline constexpr int kPageMargin = 24;

/**
 * @brief      当前宽度是否属于紧凑模式。
 * @param[in] width          可用宽度（像素）；类型 int。
 * @return     true 应按移动端布局处理。
 * @author     JaTin
 */
inline bool isCompact(int width) {
    return width < kCompactBreakpoint;
}

/**
 * @brief      计算卡片宽度。
 * @details    两侧各留 `kPageMargin` 的一半，避免卡片贴边。
 * @param[in] availableWidth 可用宽度；类型 int。
 * @param[in] maxWidth       桌面下的宽度上限；类型 int。
 * @return     窄屏时为「可用宽度 - 两侧边距」，宽屏时为 maxWidth。
 * @author     JaTin
 */
inline int cardWidth(int availableWidth, int maxWidth = kCardMaxWidth) {
    const int usable = availableWidth - kPageMargin * 2;
    /**
     * @brief      获取 bound相关的数据并返回处理结果。
     * @param[in] usable         输入参数；类型 qMin(。
     * @param[in] maxWidth       输入参数；类型 auto。
     * @return     返回 return 类型的处理结果。
     * @author     JaTin
     */
    return qBound(240, qMin(usable, maxWidth), maxWidth);
}

/**
 * @brief      计算气泡的最大宽度。
 * @details    桌面 520 是观感上限；窄屏取 78%，给头像与留白留出空间。
 * @param[in] availableWidth 消息区可用宽度；类型 int。
 * @return     窄屏按比例收缩，宽屏保持上限。
 * @author     JaTin
 */
inline int bubbleMaxWidth(int availableWidth) {
    /**
     * @brief      获取 bound相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     返回 return 类型的处理结果。
     * @author     JaTin
     */
    return qBound(180, qMin(520, static_cast<int>(availableWidth * 0.78)), 520);
}

/**
 * @brief  namespace ui。
 */
}
/**
 * @brief  namespace core。
 */
}
