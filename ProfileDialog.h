// ProfileDialog.h
//
// 编辑个人信息：姓名 / 年龄 / 个人描述。保存后由 MainWindow 走 UpdateProfile 上行。
#pragma once

#include <QDialog>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;

class ProfileDialog : public QDialog {
    Q_OBJECT

public:
    ProfileDialog(const QString& name, int age, const QString& desc, QWidget* parent = nullptr);

signals:
    void saved(const QString& name, int age, const QString& desc);

private:
    void submit();

    QLineEdit*      nameEdit_ = nullptr;
    QSpinBox*       ageSpin_ = nullptr;
    QPlainTextEdit* descEdit_ = nullptr;
    QLabel*         errorLabel_ = nullptr;
};
