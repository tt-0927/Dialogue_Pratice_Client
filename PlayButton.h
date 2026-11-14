/**
 * @file       PlayButton.h
 * @brief      CPlayButton 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CPlayButton.h。
 */

/**
 * @brief  小号播放/停止按钮：气泡里听发音、译文卡上听译文都用它。
 */
#pragma once

#include <QAbstractButton>

/**
 * @brief   播放按钮。
 * @author  JaTin
 */
class CPlayButton : public QAbstractButton {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CPlayButton(QWidget* parent = nullptr);

    /**
     * @brief      设置 playing相关的数据并返回处理结果。
     * @param[in] playing        输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setPlaying(bool playing);

protected:
    /**
     * @brief      绘制事件。
     * @param[in] event          事件；类型 QPaintEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void paintEvent(QPaintEvent* event) override;
    /**
     * @brief      进入事件。
     * @param[in] event          事件；类型 QEnterEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void enterEvent(QEnterEvent* event) override;
    /**
     * @brief      退出事件。
     * @param[in] event          事件；类型 QEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void leaveEvent(QEvent* event) override;

private:
    /**
     * @brief  Playing。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool m_nPlaying = false;
    /**
     * @brief  Hovered。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool m_nHovered = false;
};
