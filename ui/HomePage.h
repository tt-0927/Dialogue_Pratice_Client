/**
 * @file       HomePage.h
 * @brief      CHomePage 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CHomePage.h。
 */

/**
 * @brief  登录后的主页：个人信息卡 + 编辑资料 + 房间号输入 + 进入房间。
 */
#pragma once

#include <QWidget>

class CAvatar;
class QFrame;
class QLabel;
class QResizeEvent;
class QLineEdit;
class QPushButton;

/**
 * @brief   CHome页面。
 * @author  JaTin
 */
class CHomePage : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CHomePage(QWidget* parent = nullptr);

    /**
     * @brief      设置个人资料。
     * @param[in] id             编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setProfile(qint64 id, const QString& name, int age, const QString& desc);
    /**
     * @brief      设置服务端文本。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setServerText(const QString& text);
    /**
     * @brief      显示错误。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showError(const QString& message);
    /**
     * @brief      清空错误。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearError();
    /**
     * @brief      设置 busy相关的数据并返回处理结果。
     * @param[in] busy           输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setBusy(bool busy);
    /**
     * @brief      聚焦房间输入。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void focusRoomInput();

signals:
    /**
     * @brief      执行 edit 个人资料 requested。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void editProfileRequested();
    /**
     * @brief      加入房间 requested。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void joinRoomRequested(const QString& room);
    /**
     * @brief      退出登录 requested。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void logoutRequested();

protected:
    /**
     * @brief      窗口尺寸变化时按可用宽度调整卡片宽度。
     * @param[in] event          事件；类型 QResizeEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief      提交房间。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submitRoom();

    /**
     * @brief  卡片。 初值 = nullptr。
     * @note   类型 QFrame*；类/结构体成员变量。
     */
    QFrame*      m_pCard = nullptr;
    /**
     * @brief  头像。 初值 = nullptr。
     * @note   类型 CAvatar*；类/结构体成员变量。
     */
    CAvatar*      m_pAvatar = nullptr;
    /**
     * @brief  名称标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pNameLabel = nullptr;
    /**
     * @brief  编号标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pIdLabel = nullptr;
    /**
     * @brief  名称数值。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pNameValue = nullptr;
    /**
     * @brief  年龄数值。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pAgeValue = nullptr;
    /**
     * @brief  个人简介数值。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pDescValue = nullptr;
    /**
     * @brief  房间Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*   m_pRoomEdit = nullptr;
    /**
     * @brief  Join按钮。 初值 = nullptr。
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton* m_pJoinBtn = nullptr;
    /**
     * @brief  错误标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pErrorLabel = nullptr;
    /**
     * @brief  服务端标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pServerLabel = nullptr;
};
