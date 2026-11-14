/**
 * @file       ProfileDialog.h
 * @brief      CProfileDialog 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CProfileDialog.h。
 */

/**
 * @brief  编辑个人信息：姓名 / 年龄 / 个人描述。保存后由 CMainWindow 走 UpdateProfile 上行。
 */
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

/**
 * @brief   C个人资料对话框。
 * @author  JaTin
 */
class CProfileDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    CProfileDialog(const QString& name, int age, const QString& desc, QWidget* parent = nullptr);

signals:
    /**
     * @brief      执行 saved相关的数据并返回处理结果。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void saved(const QString& name, int age, const QString& desc);

private:
    /**
     * @brief      提交 submit。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit();

    /**
     * @brief  名称Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pNameEdit = nullptr;
    /**
     * @brief  年龄Spin。 初值 = nullptr。
     * @note   类型 QSpinBox*；类/结构体成员变量。
     */
    QSpinBox*       m_pAgeSpin = nullptr;
    /**
     * @brief  个人简介Edit。 初值 = nullptr。
     * @note   类型 QPlainTextEdit*；类/结构体成员变量。
     */
    QPlainTextEdit* m_pDescEdit = nullptr;
    /**
     * @brief  错误标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*         m_pErrorLabel = nullptr;
};
