/**
 * @file       LoginPage.h
 * @brief      CLoginPage 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CLoginPage.h。
 */
#pragma once

#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

/**
 * @brief   完成登录页面。
 * @author  JaTin
 */
class CLoginPage : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CLoginPage(QWidget* parent = nullptr);

    /**
     * @brief      设置 busy相关的数据并返回处理结果。
     * @param[in] busy           输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setBusy(bool busy);
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

signals:
    /**
     * @brief      完成登录 requested。
     * @param[in] host           服务器主机地址；类型 const QString&。
     * @param[in] port           服务器端口；类型 quint16。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void loginRequested(const QString& host, quint16 port, const QString& name);

private:
    /**
     * @brief      提交 submit。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit();

    /**
     * @brief  主机地址Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*   m_pHostEdit = nullptr;
    /**
     * @brief  端口Spin。 初值 = nullptr。
     * @note   类型 QSpinBox*；类/结构体成员变量。
     */
    QSpinBox*    m_pPortSpin = nullptr;
    /**
     * @brief  名称Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*   m_pNameEdit = nullptr;
    /**
     * @brief  Login按钮。 初值 = nullptr。
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton* m_pLoginBtn = nullptr;
    /**
     * @brief  错误标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pErrorLabel = nullptr;
};
