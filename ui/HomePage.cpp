// HomePage.cpp
#include "ui/HomePage.h"

#include "core/UiMetrics.h"

#include "ui/widgets/Avatar.h"
#include "core/Theme.h"

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

QLabel* makeKeyLabel(const QString& text, QWidget* parent) {
    auto* label = new QLabel(text, parent);
    label->setObjectName("KeyLabel");
    label->setFixedWidth(76);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    return label;
}

QLabel* makeValueLabel(QWidget* parent) {
    auto* label = new QLabel(parent);
    label->setObjectName("ValueLabel");
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

}  // namespace

HomePage::HomePage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 顶栏 ----
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

    serverLabel_ = new QLabel(topBar);
    serverLabel_->setObjectName("HintLabel");
    topLayout->addWidget(serverLabel_);

    auto* logoutBtn = new QPushButton(QStringLiteral("退出登录"), topBar);
    logoutBtn->setObjectName("LinkButton");
    logoutBtn->setCursor(Qt::PointingHandCursor);
    topLayout->addWidget(logoutBtn);

    root->addWidget(topBar);

    // ---- 主体 ----
    auto* body = new QWidget(this);
    auto* bodyLayout = new QVBoxLayout(body);
    bodyLayout->setContentsMargins(24, 28, 24, 28);
    bodyLayout->addStretch(1);

    auto* card = new QFrame(body);
    card_ = card;
    card->setObjectName("Card");
    card->setFixedWidth(core::ui::cardWidth(width(), 560));   // 初始值，resizeEvent 会再校准

    auto* shadow = new QGraphicsDropShadowEffect(card);
    shadow->setBlurRadius(36);
    shadow->setOffset(0, 10);
    shadow->setColor(QColor(16, 24, 40, 26));
    card->setGraphicsEffect(shadow);

    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(32, 28, 32, 28);
    cardLayout->setSpacing(16);

    // 头像 + 姓名
    auto* headerRow = new QHBoxLayout();
    headerRow->setSpacing(16);

    avatar_ = new Avatar(card);
    avatar_->setFixedSize(72, 72);
    avatar_->setFontPx(30);
    headerRow->addWidget(avatar_, 0, Qt::AlignTop);

    auto* nameColumn = new QVBoxLayout();
    nameColumn->setSpacing(4);

    nameLabel_ = new QLabel(card);
    nameLabel_->setStyleSheet(QStringLiteral("font-size: 18pt; font-weight: 700;"));
    nameColumn->addWidget(nameLabel_);

    idLabel_ = new QLabel(card);
    idLabel_->setObjectName("SubTitle");
    nameColumn->addWidget(idLabel_);
    nameColumn->addStretch(1);

    headerRow->addLayout(nameColumn, 1);
    cardLayout->addLayout(headerRow);

    // 资料区
    auto* infoBox = new QFrame(card);
    infoBox->setObjectName("InfoBox");
    auto* grid = new QGridLayout(infoBox);
    grid->setContentsMargins(18, 14, 18, 14);
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);

    nameValue_ = makeValueLabel(infoBox);
    ageValue_ = makeValueLabel(infoBox);
    descValue_ = makeValueLabel(infoBox);

    grid->addWidget(makeKeyLabel(QStringLiteral("姓名"), infoBox), 0, 0);
    grid->addWidget(nameValue_, 0, 1);
    grid->addWidget(makeKeyLabel(QStringLiteral("年龄"), infoBox), 1, 0);
    grid->addWidget(ageValue_, 1, 1);
    grid->addWidget(makeKeyLabel(QStringLiteral("个人描述"), infoBox), 2, 0);
    grid->addWidget(descValue_, 2, 1);
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

    // 房间区
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

    roomEdit_ = new QLineEdit(card);
    roomEdit_->setObjectName("Field");
    roomEdit_->setPlaceholderText(QStringLiteral("房间号，例如 1001"));
    roomEdit_->setMaxLength(32);
    roomEdit_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9_-]{0,32}")), roomEdit_));
    roomEdit_->setMinimumHeight(42);
    roomRow->addWidget(roomEdit_, 1);

    joinBtn_ = new QPushButton(QStringLiteral("进入房间"), card);
    joinBtn_->setObjectName("PrimaryButton");
    joinBtn_->setCursor(Qt::PointingHandCursor);
    joinBtn_->setMinimumHeight(42);
    joinBtn_->setMinimumWidth(120);
    roomRow->addWidget(joinBtn_, 0);

    cardLayout->addLayout(roomRow);

    errorLabel_ = new QLabel(card);
    errorLabel_->setObjectName("ErrorText");
    errorLabel_->setWordWrap(true);
    errorLabel_->setVisible(false);
    cardLayout->addWidget(errorLabel_);

    bodyLayout->addWidget(card, 0, Qt::AlignHCenter);
    bodyLayout->addStretch(2);
    root->addWidget(body, 1);

    connect(editBtn, &QPushButton::clicked, this, &HomePage::editProfileRequested);
    connect(joinBtn_, &QPushButton::clicked, this, &HomePage::submitRoom);
    connect(roomEdit_, &QLineEdit::returnPressed, this, &HomePage::submitRoom);
    connect(logoutBtn, &QPushButton::clicked, this, &HomePage::logoutRequested);
}

void HomePage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (card_ == nullptr) return;

    const int target = core::ui::cardWidth(width(), 560);
    if (card_->width() != target) card_->setFixedWidth(target);

    // 窄屏顶栏塞不下「标题 + 连接状态 + 退出登录」三样，
    // 不处理的话标题会被裁成「Dialogue Pr」这种半截文本。
    // 手机端用户本来就不关心服务器地址，直接隐藏连接状态。
    if (serverLabel_ != nullptr) {
        serverLabel_->setVisible(!core::ui::isCompact(width()));
    }
}

void HomePage::setProfile(qint64 id, const QString& name, int age, const QString& desc) {
    avatar_->setText(name);
    avatar_->setColor(Avatar::colorForId(id));
    nameLabel_->setText(name);
    idLabel_->setText(QStringLiteral("账号 ID %1").arg(id));
    nameValue_->setText(name);
    ageValue_->setText(age > 0 ? QStringLiteral("%1 岁").arg(age)
                               : QStringLiteral("未填写"));
    descValue_->setText(desc.isEmpty() ? QStringLiteral("还没有填写个人描述")
                                       : desc);
}

void HomePage::setServerText(const QString& text) {
    serverLabel_->setText(text);
}

void HomePage::submitRoom() {
    clearError();
    const QString room = roomEdit_->text().trimmed();
    if (room.isEmpty()) {
        showError(QStringLiteral("请先输入房间号"));
        roomEdit_->setFocus();
        return;
    }
    emit joinRoomRequested(room);
}

void HomePage::showError(const QString& message) {
    errorLabel_->setText(message);
    errorLabel_->setVisible(true);
}

void HomePage::clearError() {
    errorLabel_->clear();
    errorLabel_->setVisible(false);
}

void HomePage::setBusy(bool busy) {
    joinBtn_->setEnabled(!busy);
    joinBtn_->setText(busy ? QStringLiteral("进入中…") : QStringLiteral("进入房间"));
    roomEdit_->setEnabled(!busy);
}

void HomePage::focusRoomInput() {
    roomEdit_->setFocus();
    roomEdit_->selectAll();
}
