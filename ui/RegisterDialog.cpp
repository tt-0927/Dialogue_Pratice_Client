/*
 * 文件名:    RegisterDialog.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  注册对话框的实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#include "ui/RegisterDialog.h"

#include "core/Logger.h"
#include "net/ProtocolBridge.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QIntValidator>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QString kTag = QStringLiteral("ui");
/* 连接 + 响应总超时（毫秒）。 */
constexpr int kTimeoutMs = 12000;
/* namespace */
}

/*
 * 功能描述:  构造 CRegisterDialog 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   strHost          服务器主机地址，类型 const QString&。
 * param [in]   uPort            服务器端口，类型 quint16。
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget*。
 * return:    无返回值（构造函数或析构函数）。
 */
CRegisterDialog::CRegisterDialog(const QString& strHost, quint16 uPort, QWidget* pParent)
    : QDialog(pParent), m_strHost(strHost), m_uPort(uPort) {
    setWindowTitle(QStringLiteral("注册新账号"));
    setModal(true);
    /* 固定 420 在手机竖屏（360–420dp）会超出屏幕右侧， */
    /* 按父窗口宽度自适应：窄屏取「父宽 - 两侧边距」。 */
    const int nParentWidth = (pParent != nullptr) ? pParent->width() : 420;
    setMinimumWidth(qBound(300, nParentWidth - 48, 420));

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QVBoxLayout(this);
    pRoot->setContentsMargins(24, 20, 24, 20);
    pRoot->setSpacing(12);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pTitle = new QLabel(QStringLiteral("创建账号"), this);
    pTitle->setStyleSheet(QStringLiteral("font-size: 14pt; font-weight: 700;"));
    pRoot->addWidget(pTitle);

    auto* pHint = new QLabel(
        QStringLiteral("登录名 3–24 位，可用字母、数字、下划线和短横线；密码至少 6 位。"),
        this);
    pHint->setObjectName("HintLabel");
    pHint->setWordWrap(true);
    pRoot->addWidget(pHint);

    auto* pForm = new QFormLayout();
    pForm->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    pForm->setHorizontalSpacing(12);
    pForm->setVerticalSpacing(10);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pUserEdit = new QLineEdit(this);
    m_pUserEdit->setObjectName("Field");
    m_pUserEdit->setMaxLength(24);
    m_pUserEdit->setPlaceholderText(QStringLiteral("例如 alice"));
    pForm->addRow(QStringLiteral("登录名 *"), m_pUserEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPassEdit = new QLineEdit(this);
    m_pPassEdit->setObjectName("Field");
    m_pPassEdit->setEchoMode(QLineEdit::Password);
    m_pPassEdit->setPlaceholderText(QStringLiteral("至少 6 位"));
    pForm->addRow(QStringLiteral("密码 *"), m_pPassEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPass2Edit = new QLineEdit(this);
    m_pPass2Edit->setObjectName("Field");
    m_pPass2Edit->setEchoMode(QLineEdit::Password);
    m_pPass2Edit->setPlaceholderText(QStringLiteral("再输一次"));
    pForm->addRow(QStringLiteral("确认密码 *"), m_pPass2Edit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNickEdit = new QLineEdit(this);
    m_pNickEdit->setObjectName("Field");
    m_pNickEdit->setMaxLength(24);
    m_pNickEdit->setPlaceholderText(QStringLiteral("留空则与登录名相同"));
    pForm->addRow(QStringLiteral("昵称"), m_pNickEdit);

    /* 用 QLineEdit + 校验器而不是 QSpinBox：后者套上 QSS padding 后， */
    /* 上下按钮在窄屏会渲染成一条被裁切的残影（端口框踩过同样的坑）。 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAgeEdit = new QLineEdit(this);
    m_pAgeEdit->setObjectName("Field");
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pAgeEdit->setValidator(new QIntValidator(0, 150, m_pAgeEdit));
    m_pAgeEdit->setPlaceholderText(QStringLiteral("不填"));
    pForm->addRow(QStringLiteral("年龄"), m_pAgeEdit);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pDescEdit = new QPlainTextEdit(this);
    m_pDescEdit->setObjectName("Field");
    m_pDescEdit->setFixedHeight(64);
    m_pDescEdit->setPlaceholderText(QStringLiteral("一句话介绍自己（可选，最多 200 字节）"));
    pForm->addRow(QStringLiteral("简介"), m_pDescEdit);

    pRoot->addLayout(pForm);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pStatusLabel = new QLabel(this);
    m_pStatusLabel->setWordWrap(true);
    m_pStatusLabel->setVisible(false);
    pRoot->addWidget(m_pStatusLabel);

    auto* pButtons = new QHBoxLayout();
    pButtons->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pCancelBtn = new QPushButton(QStringLiteral("取消"), this);
    m_pCancelBtn->setObjectName("LinkButton");
    m_pCancelBtn->setCursor(Qt::PointingHandCursor);
    pButtons->addWidget(m_pCancelBtn);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pSubmitBtn = new QPushButton(QStringLiteral("注册"), this);
    m_pSubmitBtn->setDefault(true);
    m_pSubmitBtn->setCursor(Qt::PointingHandCursor);
    pButtons->addWidget(m_pSubmitBtn);

    pRoot->addLayout(pButtons);

    /* ---- 临时连接：注册发生在登录之前，用独立的 bridge，用完即销毁 ---- */
    m_pBridge = new CProtocolBridge(this);

    connect(m_pBridge, &CProtocolBridge::connected, this, [this] {
        m_pBridge->registerAccount(m_pUserEdit->text().trimmed(), m_pPassEdit->text(),
                                 m_pNickEdit->text().trimmed(), m_pAgeEdit->text().toInt(),
                                 m_pDescEdit->toPlainText().trimmed());
    });
    connect(m_pBridge, &CProtocolBridge::registerSucceeded, this,
            [this](qint64 /*userId*/, const QString& username, const QString& nickname) {
                DP_LOG_INFO(kTag, QStringLiteral("注册成功：%1（%2）").arg(username, nickname));
                m_pTimeoutTimer->stop();
                accept();
            });
    connect(m_pBridge, &CProtocolBridge::serverError, this,
            [this](const QString& code, const QString& strText) {
                DP_LOG_WARN(kTag, QStringLiteral("注册失败 %1：%2").arg(code, strText));
                setBusy(false);
                showError(strText);
                teardown();
            });
    connect(m_pBridge, &CProtocolBridge::networkError, this, [this](const QString& message) {
        setBusy(false);
        showError(QStringLiteral("连接失败：%1").arg(message));
        teardown();
    });

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTimeoutTimer = new QTimer(this);
    m_pTimeoutTimer->setSingleShot(true);
    connect(m_pTimeoutTimer, &QTimer::timeout, this, [this] {
        setBusy(false);
        showError(QStringLiteral("连接超时，请检查服务器地址与端口"));
        teardown();
    });

    connect(m_pSubmitBtn, &QPushButton::clicked, this, &CRegisterDialog::submit);
    connect(m_pCancelBtn, &QPushButton::clicked, this, &CRegisterDialog::reject);
    connect(m_pUserEdit, &QLineEdit::returnPressed, this, &CRegisterDialog::submit);
    connect(m_pPassEdit, &QLineEdit::returnPressed, this, &CRegisterDialog::submit);
    connect(m_pPass2Edit, &QLineEdit::returnPressed, this, &CRegisterDialog::submit);

    m_pUserEdit->setFocus();
}

CRegisterDialog::~CRegisterDialog() = default;

/*
 * 功能描述:  获取username。
 * 作者:      JaTin
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString CRegisterDialog::username() const {
    return m_pUserEdit != nullptr ? m_pUserEdit->text().trimmed() : QString();
}

/*
 * 功能描述:  获取密码。
 * 作者:      JaTin
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString CRegisterDialog::password() const {
    return m_pPassEdit != nullptr ? m_pPassEdit->text() : QString();
}

/*
 * 功能描述:  提交submit。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CRegisterDialog::submit() {
    if (!m_pSubmitBtn->isEnabled()) return;

    const QString strUsername = m_pUserEdit->text().trimmed();
    const QString strPassword = m_pPassEdit->text();
    const QString strConfirm = m_pPass2Edit->text();

    /* 本地先挡一遍，省得为了一个笔误跑一趟网络 */
    if (strUsername.size() < 3) {
        showError(QStringLiteral("登录名至少 3 个字符"));
        m_pUserEdit->setFocus();
        return;
    }
    if (strPassword.size() < 6) {
        showError(QStringLiteral("密码至少 6 位"));
        m_pPassEdit->setFocus();
        return;
    }
    if (strPassword != strConfirm) {
        showError(QStringLiteral("两次输入的密码不一致"));
        m_pPass2Edit->setFocus();
        return;
    }

    setBusy(true);
    m_pStatusLabel->setText(QStringLiteral("正在连接服务器…"));
    m_pStatusLabel->setStyleSheet(QStringLiteral("color:#5B6675;"));
    m_pStatusLabel->setVisible(true);

    m_pTimeoutTimer->start(kTimeoutMs);
    m_pBridge->connectToServer(m_strHost, m_uPort);
}

/*
 * 功能描述:  设置busy。
 * 作者:      JaTin
 * param [in]   nBusy            输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CRegisterDialog::setBusy(bool nBusy) {
    m_pUserEdit->setEnabled(!nBusy);
    m_pPassEdit->setEnabled(!nBusy);
    m_pPass2Edit->setEnabled(!nBusy);
    m_pNickEdit->setEnabled(!nBusy);
    m_pAgeEdit->setEnabled(!nBusy);
    m_pDescEdit->setEnabled(!nBusy);
    m_pSubmitBtn->setEnabled(!nBusy);
}

/*
 * 功能描述:  显示错误。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CRegisterDialog::showError(const QString& strText) {
    m_pStatusLabel->setText(strText);
    m_pStatusLabel->setStyleSheet(QStringLiteral("color:#E5484D;"));
    m_pStatusLabel->setVisible(true);
}

/*
 * 功能描述:  拆卸teardown。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CRegisterDialog::teardown() {
    if (m_pTimeoutTimer != nullptr) m_pTimeoutTimer->stop();
    if (m_pBridge != nullptr) m_pBridge->disconnectFromServer();
}
