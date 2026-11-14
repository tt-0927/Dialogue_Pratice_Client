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

class QResizeEvent;

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;

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

    /**
     * @brief      当前填写的服务器地址（供注册对话框使用）。
     * @note       本函数无输入参数。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    QString host() const;

    /**
     * @brief      当前填写的端口。
     * @note       本函数无输入参数。
     * @return     返回计数或长度；无有效数据时返回 0。
     * @author     JaTin
     */
    quint16 port() const;

    /**
     * @brief      回填账号与密码。
     * @details    注册成功后调用，省得用户再敲一遍。
     * @param[in] name           登录名；类型 const QString&。
     * @param[in] password       密码；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setCredentials(const QString& name, const QString& password);

signals:
    /**
     * @brief  用户点击登录。 host 服务器地址。 port 端口。 name 账号名（或昵称）。 password 密码；为空表示按昵称匿名登录。
     */
    void loginRequested(const QString& host, quint16 port, const QString& name,
                        const QString& password);

    /**
     * @brief      用户点击「注册新账号」。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void registerRequested();

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
     * @brief      提交 submit。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit();

    /**
     * @brief  卡片。 初值 = nullptr。
     * @note   类型 QFrame*；类/结构体成员变量。
     */
    QFrame*      m_pCard = nullptr;
    /**
     * @brief  主机地址Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*   m_pHostEdit = nullptr;
    /**
     * @brief  端口输入。用 QLineEdit + QIntValidator 而非 QSpinBox： QSpinBox 套上 QSS 的 padding 后，内部 lineEdit 的几何计算会出错，。
     * @details 窄屏（尤其 Android）下文本被裁成只剩一个字符，且 setFixedWidth 失效。
     */
    QLineEdit*   m_pPortEdit = nullptr;
    /**
     * @brief  名称Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*   m_pNameEdit = nullptr;
    /**
     * @brief  密码Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*   m_pPasswordEdit = nullptr;
    /**
     * @brief  Login按钮。 初值 = nullptr。
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton* m_pLoginBtn = nullptr;
    /**
     * @brief  Register按钮。 初值 = nullptr。
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton* m_pRegisterBtn = nullptr;
    /**
     * @brief  错误标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pErrorLabel = nullptr;
};
