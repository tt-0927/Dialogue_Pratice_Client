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

/*
 * 功能描述:  构造 CProfileDialog 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget*。
 * return:    无返回值（构造函数或析构函数）。
 */
CProfileDialog::CProfileDialog(const QString& strName, int nAge, const QString& strDesc, QWidget* pParent)
    : QDialog(pParent) {
    setWindowTitle(QStringLiteral("编辑个人信息"));
    setModal(true);
    setFixedWidth(440);
    setStyleSheet(QStringLiteral("QDialog { background: #FFFFFF; }"));

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(28, 24, 28, 22);
    pRoot->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pTitle = new QLabel(QStringLiteral("编辑个人信息"), this);
    pTitle->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 700;"));
    pRoot->addWidget(pTitle);
    pRoot->addSpacing(6);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pNameLabel = new QLabel(QStringLiteral("姓名"), this);
    pNameLabel->setObjectName("KeyLabel");
    pRoot->addWidget(pNameLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNameEdit = new QLineEdit(strName, this);
    m_pNameEdit->setObjectName("Field");
    m_pNameEdit->setMaxLength(24);
    pRoot->addWidget(m_pNameEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pAgeLabel = new QLabel(QStringLiteral("年龄"), this);
    pAgeLabel->setObjectName("KeyLabel");
    pRoot->addWidget(pAgeLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAgeSpin = new QSpinBox(this);
    m_pAgeSpin->setObjectName("Field");
    m_pAgeSpin->setRange(0, 150);
    m_pAgeSpin->setValue(nAge);
    m_pAgeSpin->setSpecialValueText(QStringLiteral("未填写"));
    m_pAgeSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    pRoot->addWidget(m_pAgeSpin);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pDescLabel = new QLabel(QStringLiteral("个人描述"), this);
    pDescLabel->setObjectName("KeyLabel");
    pRoot->addWidget(pDescLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pDescEdit = new QPlainTextEdit(strDesc, this);
    m_pDescEdit->setObjectName("Field");
    m_pDescEdit->setPlaceholderText(QStringLiteral("一句话介绍自己，最多 200 字节"));
    m_pDescEdit->setFixedHeight(96);
    /* Tab 切换控件而不是插入制表符 */
    m_pDescEdit->setTabChangesFocus(true);
    pRoot->addWidget(m_pDescEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pErrorLabel = new QLabel(this);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    pRoot->addWidget(m_pErrorLabel);

    pRoot->addSpacing(8);

    auto* pButtonRow = new QHBoxLayout();
    pButtonRow->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pCancelBtn = new QPushButton(QStringLiteral("取消"), this);
    pCancelBtn->setObjectName("GhostButton");
    pCancelBtn->setCursor(Qt::PointingHandCursor);
    pCancelBtn->setMinimumWidth(88);
    pButtonRow->addWidget(pCancelBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pSaveBtn = new QPushButton(QStringLiteral("保存"), this);
    pSaveBtn->setObjectName("PrimaryButton");
    pSaveBtn->setCursor(Qt::PointingHandCursor);
    pSaveBtn->setMinimumWidth(96);
    pSaveBtn->setDefault(true);
    pButtonRow->addWidget(pSaveBtn);

    pRoot->addLayout(pButtonRow);

    connect(pCancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(pSaveBtn, &QPushButton::clicked, this, &CProfileDialog::submit);
}

/*
 * 功能描述:  提交submit。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProfileDialog::submit() {
    const QString strName = m_pNameEdit->text().trimmed();
    if (strName.isEmpty()) {
        m_pErrorLabel->setText(QStringLiteral("姓名不能为空"));
        m_pErrorLabel->setVisible(true);
        return;
    }
    if (strName.toUtf8().size() > 24) {
        m_pErrorLabel->setText(QStringLiteral("姓名最多 24 字节（一个汉字约 3 字节）"));
        m_pErrorLabel->setVisible(true);
        return;
    }
    const QString strDesc = m_pDescEdit->toPlainText().trimmed();
    if (strDesc.toUtf8().size() > 200) {
        m_pErrorLabel->setText(QStringLiteral("个人描述最多 200 字节"));
        m_pErrorLabel->setVisible(true);
        return;
    }

    emit saved(strName, m_pAgeSpin->value(), strDesc);
    accept();
}
