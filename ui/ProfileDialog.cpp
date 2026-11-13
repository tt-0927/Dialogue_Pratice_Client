// ProfileDialog.cpp
#include "ui/ProfileDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QIntValidator>
#include <QVBoxLayout>

ProfileDialog::ProfileDialog(const QString& name, int age, const QString& desc, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("编辑个人信息"));
    setModal(true);
    setMinimumWidth(300);
    setMaximumWidth(440);
    setStyleSheet(QStringLiteral("QDialog { background: #FFFFFF; }"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("编辑个人信息"), this);
    title->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 700;"));
    root->addWidget(title);
    root->addSpacing(6);

    auto* nameLabel = new QLabel(QStringLiteral("姓名"), this);
    nameLabel->setObjectName("KeyLabel");
    root->addWidget(nameLabel);

    nameEdit_ = new QLineEdit(name, this);
    nameEdit_->setObjectName("Field");
    nameEdit_->setMaxLength(24);
    root->addWidget(nameEdit_);

    auto* ageLabel = new QLabel(QStringLiteral("年龄"), this);
    ageLabel->setObjectName("KeyLabel");
    root->addWidget(ageLabel);

    // QLineEdit + 校验器替代 QSpinBox：后者在 QSS padding 下文本会被裁切
    ageEdit_ = new QLineEdit(this);
    ageEdit_->setObjectName("Field");
    ageEdit_->setValidator(new QIntValidator(0, 150, ageEdit_));
    ageEdit_->setPlaceholderText(QStringLiteral("未填写"));
    if (age > 0) ageEdit_->setText(QString::number(age));
    root->addWidget(ageEdit_);

    auto* descLabel = new QLabel(QStringLiteral("个人描述"), this);
    descLabel->setObjectName("KeyLabel");
    root->addWidget(descLabel);

    descEdit_ = new QPlainTextEdit(desc, this);
    descEdit_->setObjectName("Field");
    descEdit_->setPlaceholderText(QStringLiteral("一句话介绍自己，最多 200 字节"));
    descEdit_->setFixedHeight(96);
    descEdit_->setTabChangesFocus(true);   // Tab 切换控件而不是插入制表符
    root->addWidget(descEdit_);

    errorLabel_ = new QLabel(this);
    errorLabel_->setObjectName("ErrorText");
    errorLabel_->setWordWrap(true);
    errorLabel_->setVisible(false);
    root->addWidget(errorLabel_);

    root->addSpacing(8);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);

    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    cancelBtn->setObjectName("GhostButton");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setMinimumWidth(88);
    buttonRow->addWidget(cancelBtn);

    auto* saveBtn = new QPushButton(QStringLiteral("保存"), this);
    saveBtn->setObjectName("PrimaryButton");
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setMinimumWidth(96);
    saveBtn->setDefault(true);
    buttonRow->addWidget(saveBtn);

    root->addLayout(buttonRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &ProfileDialog::submit);
}

void ProfileDialog::showEvent(QShowEvent* event) {
    QDialog::showEvent(event);

    // 手机竖屏放不下 440，按父窗口宽度收敛
    const int parentWidth = parentWidget() != nullptr ? parentWidget()->width() : 440;
    const int target = qBound(280, qMin(440, parentWidth - 48), 440);
    if (width() != target) resize(target, height());
}

void ProfileDialog::submit() {
    const QString name = nameEdit_->text().trimmed();
    if (name.isEmpty()) {
        errorLabel_->setText(QStringLiteral("姓名不能为空"));
        errorLabel_->setVisible(true);
        return;
    }
    if (name.toUtf8().size() > 24) {
        errorLabel_->setText(QStringLiteral("姓名最多 24 字节（一个汉字约 3 字节）"));
        errorLabel_->setVisible(true);
        return;
    }
    const QString desc = descEdit_->toPlainText().trimmed();
    if (desc.toUtf8().size() > 200) {
        errorLabel_->setText(QStringLiteral("个人描述最多 200 字节"));
        errorLabel_->setVisible(true);
        return;
    }

    emit saved(name, ageEdit_->text().toInt(), desc);
    accept();
}
