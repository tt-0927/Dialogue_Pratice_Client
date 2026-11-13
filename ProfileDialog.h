/*
 * 文件名:    ProfileDialog.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CProfileDialog 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CProfileDialog.h */

/* 编辑个人信息：姓名 / 年龄 / 个人描述。保存后由 CMainWindow 走 UpdateProfile 上行。 */
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

class CProfileDialog : public QDialog {
    Q_OBJECT

public:
    CProfileDialog(const QString& name, int age, const QString& desc, QWidget* parent = nullptr);

signals:
    void saved(const QString& name, int age, const QString& desc);

private:
    void submit();

    QLineEdit*      m_pNameEdit = nullptr;
    QSpinBox*       m_pAgeSpin = nullptr;
    QPlainTextEdit* m_pDescEdit = nullptr;
    QLabel*         m_pErrorLabel = nullptr;
};
