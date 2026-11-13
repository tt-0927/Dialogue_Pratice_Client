/*
 * 文件名:    HomePage.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CHomePage 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CHomePage.cpp */
#include "HomePage.h"

#include "Avatar.h"
#include "Theme.h"

#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QVBoxLayout>

namespace {

/*
 * 功能描述:  执行 makeKeyLabel 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
QLabel* makeKeyLabel(const QString& text, QWidget* parent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* label = new QLabel(text, parent);
    label->setObjectName("KeyLabel");
    label->setFixedWidth(76);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

/*
 * 功能描述:  执行 makeValueLabel 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
QLabel* makeValueLabel(QWidget* parent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* label = new QLabel(parent);
    label->setObjectName("ValueLabel");
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

/* namespace */
}

/*
 * 功能描述:  执行 CHomePage 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CHomePage::CHomePage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* ---- 顶栏 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* topBar = new QFrame(this);
    topBar->setObjectName("TopBar");
    topBar->setFixedHeight(60);
    auto* topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(24, 0, 24, 0);
    topLayout->setSpacing(12);

    auto* appTitle = new QLabel(QStringLiteral("Dialogue Pratice"), topBar);
    appTitle->setObjectName("AppTitle");
    topLayout->addWidget(appTitle);
    topLayout->addStretch(1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pServerLabel = new QLabel(topBar);
    m_pServerLabel->setObjectName("HintLabel");
    topLayout->addWidget(m_pServerLabel);

    auto* logoutBtn = new QPushButton(QStringLiteral("退出登录"), topBar);
    logoutBtn->setObjectName("LinkButton");
    logoutBtn->setCursor(Qt::PointingHandCursor);
    topLayout->addWidget(logoutBtn);

    root->addWidget(topBar);

    /* ---- 主体 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* body = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 28, 24, 28);
    bodyLayout->addStretch(1);

    auto* card = new QFrame(body);
    card->setObjectName("Card");
    card->setFixedWidth(560);

    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(16, 24, 40, 26));
    card->setGraphicsEffect(shadow);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(16);

    /* 头像 + 姓名 */
    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(16);

    m_pAvatar = new CAvatar(card);
    m_pAvatar->setFixedSize(72, 72);
    m_pAvatar->setFontPx(30);
    headerRow->addWidget(m_pAvatar, 0, Qt::AlignTop);

    auto* nameColumn = new QVBoxLayout();
    nameColumn->setSpacing(4);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNameLabel = new QLabel(card);
    m_pNameLabel->setStyleSheet(QStringLiteral("font-size: 18pt; font-weight: 700;"));
    nameColumn->addWidget(m_pNameLabel);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pIdLabel = new QLabel(card);
    m_pIdLabel->setObjectName("SubTitle");
    nameColumn->addWidget(m_pIdLabel);
    nameColumn->addStretch(1);

    headerRow->addLayout(nameColumn, 1);
    cardLayout->addLayout(headerRow);

    /* 资料区 */
    auto* infoBox = new QFrame(card);
    infoBox->setObjectName("InfoBox");
    auto* grid = new QGridLayout(infoBox);
    grid->setContentsMargins(18, 14, 18, 14);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);

    m_pNameValue = makeValueLabel(infoBox);
    m_pAgeValue = makeValueLabel(infoBox);
    m_pDescValue = makeValueLabel(infoBox);

    grid->addWidget(makeKeyLabel(QStringLiteral("姓名"), infoBox), 0, 0);
    grid->addWidget(m_pNameValue, 0, 1);
    grid->addWidget(makeKeyLabel(QStringLiteral("年龄"), infoBox), 1, 0);
    grid->addWidget(m_pAgeValue, 1, 1);
    grid->addWidget(makeKeyLabel(QStringLiteral("个人描述"), infoBox), 2, 0);
    grid->addWidget(m_pDescValue, 2, 1);
    grid->setColumnStretch(1, 1);

    cardLayout->addWidget(infoBox);

    auto* editBtn = new QPushButton(QStringLiteral("编辑个人信息"), card);
    editBtn->setObjectName("GhostButton");
    editBtn->setCursor(Qt::PointingHandCursor);
    editBtn->setMinimumHeight(40);
    cardLayout->addWidget(editBtn);

    auto* divider = new QFrame(card);
    divider->setObjectName("Divider");
    divider->setFrameShape(QFrame::HLine);
    cardLayout->addWidget(divider);

    /* 房间区 */
    auto* sectionTitle = new QLabel(QStringLiteral("加入房间"), card);
    sectionTitle->setObjectName("SectionTitle");
    cardLayout->addWidget(sectionTitle);

    auto* sectionHint = new QLabel(
        QStringLiteral("输入房间号即可加入；房间不存在时会自动创建。"), card);
    sectionHint->setObjectName("HintLabel");
    sectionHint->setWordWrap(true);
    cardLayout->addWidget(sectionHint);

    auto* roomRow = new QHBoxLayout();
    roomRow->setSpacing(10);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomEdit = new QLineEdit(card);
    m_pRoomEdit->setObjectName("Field");
    m_pRoomEdit->setPlaceholderText(QStringLiteral("房间号，例如 1001"));
    m_pRoomEdit->setMaxLength(32);
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9_-]{0,32}")), m_pRoomEdit));
    m_pRoomEdit->setMinimumHeight(42);
    roomRow->addWidget(m_pRoomEdit, 1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pJoinBtn = new QPushButton(QStringLiteral("进入房间"), card);
    m_pJoinBtn->setObjectName("PrimaryButton");
    m_pJoinBtn->setCursor(Qt::PointingHandCursor);
    m_pJoinBtn->setMinimumHeight(42);
    m_pJoinBtn->setMinimumWidth(120);
    roomRow->addWidget(m_pJoinBtn, 0);

    cardLayout->addLayout(roomRow);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pErrorLabel = new QLabel(card);
    m_pErrorLabel->setObjectName("ErrorText");
    m_pErrorLabel->setWordWrap(true);
    m_pErrorLabel->setVisible(false);
    cardLayout->addWidget(m_pErrorLabel);

    bodyLayout->addWidget(card, 0, Qt::AlignHCenter);
    bodyLayout->addStretch(2);
    root->addWidget(body, 1);

    connect(editBtn, &QPushButton::clicked, this, &CHomePage::editProfileRequested);
    connect(m_pJoinBtn, &QPushButton::clicked, this, &CHomePage::submitRoom);
    connect(m_pRoomEdit, &QLineEdit::returnPressed, this, &CHomePage::submitRoom);
    connect(logoutBtn, &QPushButton::clicked, this, &CHomePage::logoutRequested);
}

/*
 * 功能描述:  执行 setProfile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  id  参数。
 * param [in]  name  参数。
 * param [in]  age  参数。
 * param [in]  desc  参数。
 * return:    void
 */
void CHomePage::setProfile(qint64 id, const QString& name, int age, const QString& desc) {
    m_pAvatar->setText(name);
    m_pAvatar->setColor(CAvatar::colorForId(id));
    m_pNameLabel->setText(name);
    m_pIdLabel->setText(QStringLiteral("账号 ID %1").arg(id));
    m_pNameValue->setText(name);
    m_pAgeValue->setText(age > 0 ? QStringLiteral("%1 岁").arg(age)
                               : QStringLiteral("未填写"));
    m_pDescValue->setText(desc.isEmpty() ? QStringLiteral("还没有填写个人描述")
                                       : desc);
}

/*
 * 功能描述:  执行 setServerText 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CHomePage::setServerText(const QString& text) {
    m_pServerLabel->setText(text);
}

/*
 * 功能描述:  执行 submitRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CHomePage::submitRoom() {
    clearError();
    const QString room = m_pRoomEdit->text().trimmed();
    if (room.isEmpty()) {
        showError(QStringLiteral("请先输入房间号"));
        m_pRoomEdit->setFocus();
        return;
    }
    emit joinRoomRequested(room);
}

/*
 * 功能描述:  执行 showError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  message  参数。
 * return:    void
 */
void CHomePage::showError(const QString& message) {
    m_pErrorLabel->setText(message);
    m_pErrorLabel->setVisible(true);
}

/*
 * 功能描述:  执行 clearError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CHomePage::clearError() {
    m_pErrorLabel->clear();
    m_pErrorLabel->setVisible(false);
}

/*
 * 功能描述:  执行 setBusy 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  busy  参数。
 * return:    void
 */
void CHomePage::setBusy(bool busy) {
    m_pJoinBtn->setEnabled(!busy);
    m_pJoinBtn->setText(busy ? QStringLiteral("进入中…") : QStringLiteral("进入房间"));
    m_pRoomEdit->setEnabled(!busy);
}

/*
 * 功能描述:  执行 focusRoomInput 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CHomePage::focusRoomInput() {
    m_pRoomEdit->setFocus();
    m_pRoomEdit->selectAll();
}
