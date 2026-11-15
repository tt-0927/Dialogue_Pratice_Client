/**
 * @file       ProfileDialog.cpp
 * @brief      CProfileDialog 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释；
 *             2026-11-15 JaTin 增加头像选择、预览与清除。
 */

/**
 * @brief  CProfileDialog.cpp。
 */
#include "ui/ProfileDialog.h"

#include "ui/widgets/Avatar.h"

#include <QBuffer>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QIntValidator>
#include <QVBoxLayout>

namespace {

/* 头像边长：预览与上传统一尺寸，方形裁剪后压 JPEG。 */
constexpr int nKAvatarEdge = 256;
/* 头像二进制上限：与服务端 kMaxAvatarBytes 一致（32 KiB）。 */
constexpr int nKAvatarMaxBytes = 32 * 1024;

/**
 * @brief      把任意图片缩放成正方形 JPEG，并把体积压到上限内。
 * @param[in] strPath         图片文件路径；类型 const QString&。
 * @return     压缩后的 JPEG 二进制；加载失败或压不到上限内时为空。
 * @author     JaTin
 */
QByteArray compressAvatar(const QString& strPath) {
    QImage stImage;
    if (!stImage.load(strPath)) return QByteArray();

    /* 居中裁剪成正方形再缩放，避免长图被压扁。 */
    const int nSide = qMin(stImage.width(), stImage.height());
    const QRect stSquare((stImage.width() - nSide) / 2, (stImage.height() - nSide) / 2,
                         nSide, nSide);
    const QImage stSquareImg = stImage.copy(stSquare)
                                   .scaled(nKAvatarEdge, nKAvatarEdge,
                                           Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QByteArray byOut;
    for (int nQuality = 85; nQuality >= 30; nQuality -= 10) {
        QBuffer stBuf(&byOut);
        stBuf.open(QIODevice::WriteOnly);
        if (!stSquareImg.save(&stBuf, "JPEG", nQuality)) return QByteArray();
        if (byOut.size() <= nKAvatarMaxBytes) return byOut;
        byOut.clear();
    }
    return QByteArray();
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CProfileDialog 对象，完成成员初始化与依赖注入。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @param[in] byAvatar       当前头像图片二进制，空表示无自定义头像；类型 const QByteArray&。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CProfileDialog::CProfileDialog(const QString& strName, int nAge, const QString& strDesc,
                               const QByteArray& byAvatar, QWidget* pParent)
    : QDialog(pParent), m_byAvatar(byAvatar) {
    setWindowTitle(QStringLiteral("编辑个人信息"));
    setModal(true);
    setMinimumWidth(300);
    setMaximumWidth(440);
    setStyleSheet(QStringLiteral("QDialog { background: #FFFFFF; }"));

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(28, 24, 28, 22);
    pRoot->setSpacing(8);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pTitle = new QLabel(QStringLiteral("编辑个人信息"), this);
    pTitle->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 700;"));
    pRoot->addWidget(pTitle);
    pRoot->addSpacing(6);

    /* 头像行：预览 + 选择/清除按钮。 */
    auto* pAvatarRow = new QHBoxLayout();
    pAvatarRow->setSpacing(12);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAvatarPreview = new CAvatar(this);
    m_pAvatarPreview->setFixedSize(72, 72);
    m_pAvatarPreview->setFontPx(30);
    m_pAvatarPreview->setText(strName);
    m_pAvatarPreview->setImage(m_byAvatar);
    pAvatarRow->addWidget(m_pAvatarPreview, 0, Qt::AlignTop);

    auto* pAvatarBtns = new QVBoxLayout();
    pAvatarBtns->setSpacing(6);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pChooseBtn = new QPushButton(QStringLiteral("选择头像"), this);
    pChooseBtn->setObjectName("GhostButton");
    pChooseBtn->setCursor(Qt::PointingHandCursor);
    pAvatarBtns->addWidget(pChooseBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pClearBtn = new QPushButton(QStringLiteral("恢复默认头像"), this);
    pClearBtn->setObjectName("LinkButton");
    pClearBtn->setCursor(Qt::PointingHandCursor);
    pAvatarBtns->addWidget(pClearBtn);
    pAvatarBtns->addStretch(1);

    pAvatarRow->addLayout(pAvatarBtns, 1);
    pRoot->addLayout(pAvatarRow);

    pRoot->addSpacing(6);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pNameLabel = new QLabel(QStringLiteral("姓名"), this);
    pNameLabel->setObjectName("KeyLabel");
    pRoot->addWidget(pNameLabel);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pNameEdit = new QLineEdit(strName, this);
    m_pNameEdit->setObjectName("Field");
    m_pNameEdit->setMaxLength(24);
    pRoot->addWidget(m_pNameEdit);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pAgeLabel = new QLabel(QStringLiteral("年龄"), this);
    pAgeLabel->setObjectName("KeyLabel");
    pRoot->addWidget(pAgeLabel);

    /**
     * @brief  QLineEdit + 校验器替代 QSpinBox：后者在 QSS padding 下文本会被裁切 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pAgeEdit = new QLineEdit(this);
    m_pAgeEdit->setObjectName("Field");
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pAgeEdit->setValidator(new QIntValidator(0, 150, m_pAgeEdit));
    m_pAgeEdit->setPlaceholderText(QStringLiteral("未填写"));
    if (nAge > 0) m_pAgeEdit->setText(QString::number(nAge));
    pRoot->addWidget(m_pAgeEdit);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pDescLabel = new QLabel(QStringLiteral("个人描述"), this);
    pDescLabel->setObjectName("KeyLabel");
    pRoot->addWidget(pDescLabel);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pDescEdit = new QPlainTextEdit(strDesc, this);
    m_pDescEdit->setObjectName("Field");
    m_pDescEdit->setPlaceholderText(QStringLiteral("一句话介绍自己，最多 200 字节"));
    m_pDescEdit->setFixedHeight(96);
    /**
     * @brief  Tab 切换控件而不是插入制表符。
     */
    m_pDescEdit->setTabChangesFocus(true);
    pRoot->addWidget(m_pDescEdit);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pErrorLabel = new QLabel(this);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    pRoot->addWidget(m_pErrorLabel);

    pRoot->addSpacing(8);

    auto* pButtonRow = new QHBoxLayout();
    pButtonRow->addStretch(1);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pCancelBtn = new QPushButton(QStringLiteral("取消"), this);
    pCancelBtn->setObjectName("GhostButton");
    pCancelBtn->setCursor(Qt::PointingHandCursor);
    pCancelBtn->setMinimumWidth(88);
    pButtonRow->addWidget(pCancelBtn);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pSaveBtn = new QPushButton(QStringLiteral("保存"), this);
    pSaveBtn->setObjectName("PrimaryButton");
    pSaveBtn->setCursor(Qt::PointingHandCursor);
    pSaveBtn->setMinimumWidth(96);
    pSaveBtn->setDefault(true);
    pButtonRow->addWidget(pSaveBtn);

    pRoot->addLayout(pButtonRow);

    connect(pCancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(pSaveBtn, &QPushButton::clicked, this, &CProfileDialog::submit);
    connect(pChooseBtn, &QPushButton::clicked, this, &CProfileDialog::chooseAvatar);
    connect(pClearBtn, &QPushButton::clicked, this, &CProfileDialog::clearAvatar);
}

/**
 * @brief      显示事件。
 * @param[in] pEvent         事件；类型 QShowEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProfileDialog::showEvent(QShowEvent* pEvent) {
    QDialog::showEvent(pEvent);

    /**
     * @brief  手机竖屏放不下 440，按父窗口宽度收敛。
     */
    const int nParentWidth = parentWidget() != nullptr ? parentWidget()->width() : 440;
    const int nTarget = qBound(280, qMin(440, nParentWidth - 48), 440);
    if (width() != nTarget) resize(nTarget, height());
}

/**
 * @brief      提交 submit。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

    emit saved(strName, m_pAgeEdit->text().toInt(), strDesc, m_byAvatar);
    accept();
}

/**
 * @brief      打开文件对话框选择头像，压缩成功后刷新预览。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProfileDialog::chooseAvatar() {
    const QString strPath = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择头像图片"), QString(),
        QStringLiteral("图片 (*.jpg *.jpeg *.png *.bmp *.webp)"));
    if (strPath.isEmpty()) return;

    const QByteArray byCompressed = compressAvatar(strPath);
    if (byCompressed.isEmpty()) {
        m_pErrorLabel->setText(QStringLiteral("图片无法读取或压缩后仍超过 32 KB，请换一张"));
        m_pErrorLabel->setVisible(true);
        return;
    }

    m_pErrorLabel->setVisible(false);
    m_byAvatar = byCompressed;
    m_pAvatarPreview->setImage(m_byAvatar);
}

/**
 * @brief      清除自定义头像并恢复首字默认图。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProfileDialog::clearAvatar() {
    m_byAvatar.clear();
    m_pAvatarPreview->setImage(QByteArray());
}
