/**
 * @file       SendButton.h
 * @brief      CSendButton 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CSendButton.h。
 */

/**
 * @brief  圆形发送按钮：自绘纸飞机图标，避免依赖字体里的箭头字形。
 */
#pragma once

#include <QAbstractButton>

/**
 * @brief   发送按钮。
 * @author  JaTin
 */
class CSendButton : public QAbstractButton {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CSendButton(QWidget* parent = nullptr);

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
     * @brief  Hovered。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool m_nHovered = false;
};
