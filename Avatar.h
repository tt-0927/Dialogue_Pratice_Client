/**
 * @file       Avatar.h
 * @brief      CAvatar 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CAvatar.h。
 */

/**
 * @brief  圆形文字头像：取名字首字符，背景色按 userId 稳定映射（Telegram 风格）。
 */
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

/**
 * @brief   C头像。
 * @author  JaTin
 */
class CAvatar : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CAvatar(QWidget* parent = nullptr);

    /**
     * @brief      设置文本。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setText(const QString& text);
    /**
     * @brief      设置颜色。
     * @param[in] color          颜色；类型 const QColor&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setColor(const QColor& color);
    /**
     * @brief      设置字体 px。
     * @param[in] px             输入参数；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setFontPx(int px);

    /**
     * @brief      按编号稳定映射出一个头像背景色。
     * @param[in] id             编号；类型 qint64。
     * @return     返回 QColor 类型的处理结果。
     * @author     JaTin
     */
    static QColor colorForId(qint64 id);

protected:
    /**
     * @brief      绘制事件。
     * @param[in] event          事件；类型 QPaintEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void paintEvent(QPaintEvent* event) override;

private:
    /**
     * @brief  消息文本。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strText;
    /**
     * @brief  颜色。 初值 = QColor( )。
     * @note   类型 QColor；类/结构体成员变量。
     */
    QColor  m_stColor = QColor("#3390EC");
    /**
     * @brief  字体Px。 初值 = 16。
     * @note   类型 int；类/结构体成员变量。
     */
    int     m_nFontPx = 16;
};
