/**
 * @file       ProfileDialog.h
 * @brief      CProfileDialog 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释；
 *             2026-11-15 JaTin 增加头像选择、预览与清除。
 */

/**
 * @brief  CProfileDialog.h。
 */

/**
 * @brief  编辑个人信息：头像 / 姓名 / 年龄 / 个人描述。保存后由 CMainWindow 走 UpdateProfile 上行。
 */
#pragma once

#include <QByteArray>
#include <QDialog>

class CAvatar;
class QLabel;
class QLineEdit;
class QShowEvent;
class QPlainTextEdit;

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
     * @param[in] avatar         当前头像图片二进制，空表示无自定义头像；类型 const QByteArray&。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    CProfileDialog(const QString& name, int age, const QString& desc, const QByteArray& avatar,
                   QWidget* parent = nullptr);

signals:
    /**
     * @brief      保存请求；avatar 为空表示清除头像。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @param[in] avatar         头像图片二进制，空表示清除；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void saved(const QString& name, int age, const QString& desc, const QByteArray& avatar);

protected:
    /**
     * @brief      首次显示时按父窗口宽度收敛对话框宽度。
     * @param[in] event          事件；类型 QShowEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showEvent(QShowEvent* event) override;

private:
    /**
     * @brief      提交 submit。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit();

    /**
     * @brief      打开文件对话框选择头像图片并压缩到上限内。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void chooseAvatar();

    /**
     * @brief      清除自定义头像，恢复首字默认图。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearAvatar();

    /**
     * @brief  头像预览。
     * 初值 = nullptr。
     * @note   类型 CAvatar*；类/结构体成员变量。
     */
    CAvatar*        m_pAvatarPreview = nullptr;
    /**
     * @brief  选中的头像二进制
     * @note   类型 QByteArray；类/结构体成员变量。
     */
    QByteArray      m_byAvatar;

    /**
     * @brief  名称Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pNameEdit = nullptr;
    /**
     * @brief  年龄Edit。 初值 = nullptr。
     * @note   类型 QLineEdit*；类/结构体成员变量。
     */
    QLineEdit*      m_pAgeEdit = nullptr;
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
