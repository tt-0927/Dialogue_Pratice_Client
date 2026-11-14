/**
 * @file       VoiceBar.h
 * @brief      CVoiceBar 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoiceBar.h。
 */

/**
 * @brief  气泡里的语音条：播放按钮 + 波形 + 时长。整条可点击。 波形是按容器哈希生成的伪波形——真实波形要额外解码一次， 而语音条通常只用来「看出这是多长的一段话」，不值得那点开销。
 */
#pragma once

#include <QByteArray>
#include <QVector>
#include <QWidget>

/**
 * @brief   C语音消息工具条。
 * @author  JaTin
 */
class CVoiceBar : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @param[in] container      容器；类型 const QByteArray&。
     * @param[in] outgoing       输入参数；类型 bool。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    CVoiceBar(int durationMs, const QByteArray& container, bool outgoing, QWidget* parent = nullptr);

    /**
     * @brief      设置 playing相关的数据并返回处理结果。
     * @param[in] playing        输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setPlaying(bool playing);
    /**
     * @brief      获取长度 hint。
     * @note       本函数无输入参数。
     * @return     返回 QSize 类型的处理结果。
     * @author     JaTin
     */
    QSize sizeHint() const override;

signals:
    /**
     * @brief      播放 requested。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void playRequested();

protected:
    /**
     * @brief      绘制事件。
     * @param[in] event          事件；类型 QPaintEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void paintEvent(QPaintEvent* event) override;
    /**
     * @brief      执行 mousepress 事件。
     * @param[in] event          事件；类型 QMouseEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void mousePressEvent(QMouseEvent* event) override;
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
     * @brief  语音时长，单位毫秒。 初值 = 0。
     * @note   类型 int；类/结构体成员变量。
     */
    int            m_nDurationMs = 0;
    /**
     * @brief  Outgoing。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool           m_nOutgoing = false;
    /**
     * @brief  Playing。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool           m_nPlaying = false;
    /**
     * @brief  Hovered。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool           m_nHovered = false;
    /**
     * @brief  0.25~1.0 的归一化柱高
     * @note   类型 QVector<qreal>；类/结构体成员变量。
     */
    QVector<qreal> m_aBars;
};
