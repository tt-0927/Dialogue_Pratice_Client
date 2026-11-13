/*
 * 文件名:    ProfileDialog.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CProfileDialog 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CProfileDialog.cpp */
#include "ProfileDialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

CProfileDialog::CProfileDialog(const QString& name, int age, const QString& desc, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(QStringLiteral("编辑个人信息"));
    setModal(true);
    setFixedWidth(440);
    setStyleSheet(QStringLiteral("QDialog { background: #FFFFFF; }"));

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(28, 24, 28, 22);
    root->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* title = new QLabel(QStringLiteral("编辑个人信息"), this);
    title->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 700;"));
    root->addWidget(title);
    root->addSpacing(6);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* nameLabel = new QLabel(QStringLiteral("姓名"), this);
    nameLabel->setObjectName("KeyLabel");
    root->addWidget(nameLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNameEdit = new QLineEdit(name, this);
    m_pNameEdit->setObjectName("Field");
    m_pNameEdit->setMaxLength(24);
    root->addWidget(m_pNameEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* ageLabel = new QLabel(QStringLiteral("年龄"), this);
    ageLabel->setObjectName("KeyLabel");
    root->addWidget(ageLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAgeSpin = new QSpinBox(this);
    m_pAgeSpin->setObjectName("Field");
    m_pAgeSpin->setRange(0, 150);
    m_pAgeSpin->setValue(age);
    m_pAgeSpin->setSpecialValueText(QStringLiteral("未填写"));
    m_pAgeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    root->addWidget(m_pAgeSpin);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* descLabel = new QLabel(QStringLiteral("个人描述"), this);
    descLabel->setObjectName("KeyLabel");
    root->addWidget(descLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pDescEdit = new QPlainTextEdit(desc, this);
    m_pDescEdit->setObjectName("Field");
    m_pDescEdit->setPlaceholderText(QStringLiteral("一句话介绍自己，最多 200 字节"));
    m_pDescEdit->setFixedHeight(96);
    /* Tab 切换控件而不是插入制表符 */
    m_pDescEdit->setTabChangesFocus(true);
    root->addWidget(m_pDescEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pErrorLabel = new QLabel(this);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    root->addWidget(m_pErrorLabel);

    root->addSpacing(8);

    auto* buttonRow = new QHBoxLayout();
    buttonRow->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    cancelBtn->setObjectName("GhostButton");
    cancelBtn->setCursor(Qt::PointingHandCursor);
    cancelBtn->setMinimumWidth(88);
    buttonRow->addWidget(cancelBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* saveBtn = new QPushButton(QStringLiteral("保存"), this);
    saveBtn->setObjectName("PrimaryButton");
    saveBtn->setCursor(Qt::PointingHandCursor);
    saveBtn->setMinimumWidth(96);
    saveBtn->setDefault(true);
    buttonRow->addWidget(saveBtn);

    root->addLayout(buttonRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, this, &CProfileDialog::submit);
}

/*
 * 功能描述:  执行 submit 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CProfileDialog::submit() {
    const QString name = m_pNameEdit->text().trimmed();
    if (name.isEmpty()) {
        m_pErrorLabel->setText(QStringLiteral("姓名不能为空"));
        m_pErrorLabel->setVisible(true);
        return;
    }
    if (name.toUtf8().size() > 24) {
        m_pErrorLabel->setText(QStringLiteral("姓名最多 24 字节（一个汉字约 3 字节）"));
        m_pErrorLabel->setVisible(true);
        return;
    }
    const QString desc = m_pDescEdit->toPlainText().trimmed();
    if (desc.toUtf8().size() > 200) {
        m_pErrorLabel->setText(QStringLiteral("个人描述最多 200 字节"));
        m_pErrorLabel->setVisible(true);
        return;
    }

    emit saved(name, m_pAgeSpin->value(), desc);
    accept();
}
