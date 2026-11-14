/**
 * @file       TranslationCard.h
 * @brief      CTranslationCard 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CTranslationCard.h。
 */

/**
 * @brief  输入框上方的译文卡：把用户输入的中文翻成英文，可试听、可一键填入输入框。
 */
#pragma once

#include <QFrame>
#include <QString>

class CPlayButton;
class QLabel;
class QPushButton;

/**
 * @brief   C译文卡片。
 * @author  JaTin
 */
class CTranslationCard : public QFrame {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CTranslationCard(QWidget* parent = nullptr);

    /**
     * @brief      显示 pending。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showPending();
    /**
     * @brief      显示译文。
     * @param[in] english        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showTranslation(const QString& english);
    /**
     * @brief      隐藏卡片。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void hideCard();
    /**
     * @brief      设置 playing相关的数据并返回处理结果。
     * @param[in] playing        输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setPlaying(bool playing);

    QString currentText() const { return current_; }

signals:
    /**
     * @brief      播放 requested。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void playRequested(const QString& text);
    /**
     * @brief      执行 userequested相关的数据并返回处理结果。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void useRequested(const QString& text);

private:
    /**
     * @brief  Title。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pTitle = nullptr;
    /**
     * @brief  正文。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pBody = nullptr;
    /**
     * @brief  Play。 初值 = nullptr。
     * @note   类型 CPlayButton*；类/结构体成员变量。
     */
    CPlayButton*  m_pPlay = nullptr;
    /**
     * @brief  Use。 初值 = nullptr。
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton* m_pUse = nullptr;
    /**
     * @brief  current。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString      current_;
};
