// MainWindow.cpp
#include "MainWindow.h"

#include "ChatPage.h"
#include "HomePage.h"
#include "LoginPage.h"
#include "MessageStore.h"
#include "ProfileDialog.h"
#include "ProtocolBridge.h"
#include "Theme.h"

#include <QLabel>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Dialogue Pratice"));
    resize(1080, 720);
    setMinimumSize(940, 620);

    auto* central = new QWidget(this);
    central->setObjectName("AppRoot");
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    stack_ = new QStackedWidget(central);
    loginPage_ = new LoginPage(stack_);
    homePage_ = new HomePage(stack_);
    chatPage_ = new ChatPage(stack_);
    stack_->addWidget(loginPage_);
    stack_->addWidget(homePage_);
    stack_->addWidget(chatPage_);
    layout->addWidget(stack_);

    setCentralWidget(central);

    banner_ = new QLabel(central);
    banner_->setObjectName("Banner");
    banner_->setAlignment(Qt::AlignCenter);
    banner_->hide();

    // 传输层：局域网自研服务端（声网 RTM 支持已移除，见 docs/plan-agora-rtm.md）
    const AppConfig::TransportSettings transport = AppConfig::loadTransport();
    if (transport.transport.compare(QLatin1String("server"), Qt::CaseInsensitive) != 0) {
        qWarning() << "配置里的 transport 不是 server，但 RTM 支持已移除，仍按 server 运行："
                   << transport.transport;
    }
    bridge_ = new ProtocolBridge(this);
    transportName_ = QStringLiteral("server");

    // 本地消息缓存：兜住服务端重启后丢掉的历史
    store_ = new MessageStore();
    const QString dbPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/history.db");
    QString storeError;
    if (!store_->open(dbPath, &storeError)) {
        qWarning() << "本地消息缓存不可用：" << storeError;
    }

    reconnectTimer_ = new QTimer(this);
    reconnectTimer_->setSingleShot(true);
    connect(reconnectTimer_, &QTimer::timeout, this, &MainWindow::onReconnectTimeout);

    // 单一定时器：新横幅要能把旧横幅的隐藏计时重置掉，
    // 否则「登录成功」那条的 3.2s 计时会把刚弹出的错误提示提前关掉。
    bannerTimer_ = new QTimer(this);
    bannerTimer_->setSingleShot(true);
    connect(bannerTimer_, &QTimer::timeout, this, [this] { banner_->hide(); });

    connect(loginPage_, &LoginPage::loginRequested, this, &MainWindow::onLoginRequested);
    connect(homePage_, &HomePage::joinRoomRequested, this, &MainWindow::onJoinRoom);
    connect(homePage_, &HomePage::editProfileRequested, this, &MainWindow::onEditProfile);
    connect(homePage_, &HomePage::logoutRequested, this, &MainWindow::onLogout);
    connect(chatPage_, &ChatPage::backRequested, this, &MainWindow::onLeaveRoom);
    connect(chatPage_, &ChatPage::sendRequested, this, &MainWindow::onSendMessage);
    connect(chatPage_, &ChatPage::sendVoiceRequested, this,
            [this](const QByteArray& audio, int durationMs) {
                bridge_->sendVoice(audio, durationMs);
            });
    connect(chatPage_, &ChatPage::notice, this, &MainWindow::showBanner);

    connect(bridge_, &ProtocolBridge::connected, this, &MainWindow::onBridgeConnected);
    connect(bridge_, &ProtocolBridge::disconnected, this, &MainWindow::onDisconnected);
    connect(bridge_, &ProtocolBridge::loginSucceeded, this, &MainWindow::onLoginSucceeded);
    connect(bridge_, &ProtocolBridge::profileUpdated, this, &MainWindow::onProfileUpdated);
    connect(bridge_, &ProtocolBridge::roomJoined, this, &MainWindow::onRoomJoined);
    connect(bridge_, &ProtocolBridge::historyReceived, this, &MainWindow::onHistoryReceived);
    connect(bridge_, &ProtocolBridge::chatReceived, this, &MainWindow::onChatReceived);
    connect(bridge_, &ProtocolBridge::voiceReceived, this, &MainWindow::onVoiceReceived);
    connect(bridge_, &ProtocolBridge::systemMessage, this, &MainWindow::onSystemMessage);
    connect(bridge_, &ProtocolBridge::serverError, this, &MainWindow::onServerError);
    connect(bridge_, &ProtocolBridge::networkError, this, &MainWindow::onNetworkError);
}

MainWindow::~MainWindow() = default;

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    positionBanner();
}

// ---------------------------------------------------------------- 登录

void MainWindow::onLoginRequested(const QString& host, quint16 port, const QString& name) {
    lastHost_ = host;
    lastPort_ = port;
    pendingHost_ = host;
    pendingPort_ = port;
    pendingName_ = name;

    resetReconnectState();
    loginPage_->clearError();
    loginPage_->setBusy(true);
    bridge_->connectToServer(host, port);
}

void MainWindow::onBridgeConnected() {
    if (reconnecting_) {
        // 重连：用已保存的身份直接重登，不再走登录页
        bridge_->login(selfName_, selfAge_, selfDesc_);
        return;
    }
    bridge_->login(pendingName_, 0, QString());
}

void MainWindow::onLoginSucceeded(qint64 userId, const QString& name, int age, const QString& desc) {
    selfId_ = userId;
    selfName_ = name;
    selfAge_ = age;
    selfDesc_ = desc;

    loginPage_->setBusy(false);
    homePage_->setProfile(selfId_, selfName_, selfAge_, selfDesc_);
    homePage_->setServerText(QStringLiteral("已连接 %1:%2").arg(pendingHost_).arg(pendingPort_));

    if (reconnecting_) {
        if (!pendingRoom_.isEmpty()) {
            bridge_->joinRoom(pendingRoom_);   // 回到断线前所在的房间
        } else {
            finishReconnect();
        }
        return;
    }

    homePage_->clearError();
    stack_->setCurrentWidget(homePage_);
    homePage_->focusRoomInput();

    showBanner(QStringLiteral("登录成功，账号 ID %1").arg(userId), false);
}

// ---------------------------------------------------------------- 房间

void MainWindow::onJoinRoom(const QString& room) {
    pendingRoom_ = room;
    homePage_->clearError();
    homePage_->setBusy(true);
    bridge_->joinRoom(room);
}

void MainWindow::onRoomJoined(const QString& room, bool created, const QVector<MemberInfo>& members) {
    homePage_->setBusy(false);

    if (reconnecting_) {
        chatPage_->resumeRoom(room, members, selfId_, selfName_);
        stack_->setCurrentWidget(chatPage_);
        finishReconnect();
        return;
    }

    chatPage_->enterRoom(room, created, members, selfId_, selfName_);
    stack_->setCurrentWidget(chatPage_);

    // 服务端/声网的历史之外，再用本地缓存补齐（按 seq 去重，不会重复）
    if (store_ != nullptr && store_->isOpen()) {
        store_->prune(transportName_, room, 500);
        chatPage_->mergeHistory(store_->loadRecent(transportName_, room, 200));
    }

    showBanner(created ? QStringLiteral("房间 %1 不存在，已自动创建").arg(room)
                       : QStringLiteral("已加入房间 %1").arg(room),
               false);
}

void MainWindow::onLeaveRoom() {
    if (!chatPage_->room().isEmpty()) {
        bridge_->leaveRoom();
    }
    pendingRoom_.clear();
    chatPage_->clearRoom();
    stack_->setCurrentWidget(homePage_);
    homePage_->focusRoomInput();
}

// ---------------------------------------------------------------- 消息

void MainWindow::onSendMessage(const QString& text) {
    bridge_->sendChat(text);
}

void MainWindow::onHistoryReceived(const QString& room, const QVector<ChatMessage>& messages) {
    if (stack_->currentWidget() != chatPage_) return;
    if (!room.isEmpty() && room != chatPage_->room()) return;
    chatPage_->mergeHistory(messages);
}

void MainWindow::onVoiceReceived(qint64 seq, qint64 from, const QString& name,
                                 const QString& room, const QByteArray& audio,
                                 int durationMs, qint64 ts) {
    if (stack_->currentWidget() != chatPage_) return;
    if (room != chatPage_->room()) return;

    chatPage_->appendVoice(seq, from, name, audio, durationMs, ts);
}

void MainWindow::onChatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                                const QString& text, qint64 ts) {
    if (stack_->currentWidget() != chatPage_) return;
    if (room != chatPage_->room()) return;

    if (store_ != nullptr && store_->isOpen()) {
        ChatMessage message;
        message.seq = seq;
        message.from = from;
        message.name = name;
        message.text = text;
        message.ts = ts;
        store_->save(transportName_, room, message);
    }

    chatPage_->appendChat(seq, from, name, text, ts);
}

void MainWindow::onSystemMessage(const QString& event, const QString& room, qint64 userId,
                                 const QString& name, const QString& oldName,
                                 const QString& text, qint64 ts) {
    if (event == QStringLiteral("welcome")) return;
    if (stack_->currentWidget() != chatPage_) return;
    if (!room.isEmpty() && room != chatPage_->room()) return;
    chatPage_->onSystemEvent(event, userId, name, oldName, text, ts);
}

// ---------------------------------------------------------------- 资料

void MainWindow::onEditProfile() {
    auto* dialog = new ProfileDialog(selfName_, selfAge_, selfDesc_, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &ProfileDialog::saved, this,
            [this](const QString& name, int age, const QString& desc) {
                bridge_->updateProfile(name, age, desc);
            });

    dialog->exec();
}

void MainWindow::onProfileUpdated(qint64 userId, const QString& name, int age, const QString& desc) {
    Q_UNUSED(userId)
    selfName_ = name;
    selfAge_ = age;
    selfDesc_ = desc;
    homePage_->setProfile(selfId_, selfName_, selfAge_, selfDesc_);
    showBanner(QStringLiteral("个人信息已更新"), false);
}

// ---------------------------------------------------------------- 断线重连

void MainWindow::onDisconnected() {
    if (suppressDisconnectNotice_) {
        suppressDisconnectNotice_ = false;
        return;
    }
    if (stack_->currentWidget() == loginPage_) return;
    if (reconnecting_) return;   // 已经进入重连流程（socket error 可能先于 disconnected 到达）

    scheduleReconnect(QStringLiteral("连接已断开"));
}

void MainWindow::onReconnectTimeout() {
    if (lastHost_.isEmpty()) {
        resetToLogin(QStringLiteral("无法重连：缺少服务器地址"));
        return;
    }
    bridge_->connectToServer(lastHost_, lastPort_);
}

void MainWindow::scheduleReconnect(const QString& reason) {
    if (reconnectAttempt_ >= kMaxReconnectAttempts) {
        resetToLogin(QStringLiteral("重连失败（%1），请检查服务端后重新登录").arg(reason));
        return;
    }

    reconnecting_ = true;
    ++reconnectAttempt_;
    chatPage_->setOnline(false);

    showBanner(QStringLiteral("连接已断开，%1 秒后第 %2/%3 次重连…")
                   .arg(kReconnectIntervalMs / 1000)
                   .arg(reconnectAttempt_)
                   .arg(kMaxReconnectAttempts),
               true);
    reconnectTimer_->start(kReconnectIntervalMs);
}

void MainWindow::finishReconnect() {
    reconnecting_ = false;
    reconnectAttempt_ = 0;
    chatPage_->setOnline(true);
    showBanner(QStringLiteral("已重新连接"), false);
}

void MainWindow::resetReconnectState() {
    reconnectTimer_->stop();
    reconnecting_ = false;
    reconnectAttempt_ = 0;
}

// ---------------------------------------------------------------- 错误

void MainWindow::onServerError(const QString& code, const QString& text) {
    Q_UNUSED(code)

    if (reconnecting_) {
        resetToLogin(QStringLiteral("重连被服务端拒绝：%1").arg(text));
        return;
    }

    if (stack_->currentWidget() == loginPage_) {
        loginPage_->setBusy(false);
        loginPage_->showError(text.isEmpty() ? QStringLiteral("登录失败") : text);
        suppressDisconnectNotice_ = true;
        bridge_->disconnectFromServer();
        return;
    }
    if (stack_->currentWidget() == homePage_) {
        homePage_->setBusy(false);
        homePage_->showError(text);
        return;
    }
    showBanner(text, true);
}

void MainWindow::onNetworkError(const QString& message) {
    if (stack_->currentWidget() == loginPage_) {
        loginPage_->setBusy(false);
        loginPage_->showError(message);
        resetReconnectState();
        return;
    }

    // 已登录状态下出现任何网络错误（RST、超时、断网）都走重连，而不是踢回登录页。
    if (reconnecting_ || selfId_ != 0) {
        scheduleReconnect(message);
        return;
    }

    showBanner(message, true);
    resetToLogin(message);
}

void MainWindow::resetToLogin(const QString& reason) {
    resetReconnectState();
    chatPage_->clearRoom();
    homePage_->setBusy(false);
    homePage_->clearError();
    loginPage_->setBusy(false);
    stack_->setCurrentWidget(loginPage_);

    selfId_ = 0;
    selfName_.clear();
    selfAge_ = 0;
    selfDesc_.clear();
    pendingRoom_.clear();

    if (!reason.isEmpty()) {
        loginPage_->showError(reason);
    }
}

void MainWindow::onLogout() {
    suppressDisconnectNotice_ = true;
    bridge_->disconnectFromServer();
    resetToLogin(QString());
}

// ---------------------------------------------------------------- 顶部提示条

void MainWindow::showBanner(const QString& text, bool error) {
    banner_->setObjectName(error ? QStringLiteral("BannerError") : QStringLiteral("Banner"));
    banner_->style()->unpolish(banner_);
    banner_->style()->polish(banner_);

    banner_->setText(text);
    banner_->adjustSize();
    banner_->show();
    banner_->raise();
    positionBanner();

    // 错误信息留久一点，免得用户没看清就消失
    bannerTimer_->start(error ? 6000 : 3200);
}

void MainWindow::positionBanner() {
    if (banner_ == nullptr || !banner_->isVisible()) return;
    banner_->adjustSize();
    const int x = (width() - banner_->width()) / 2;
    banner_->move(qMax(0, x), 76);
}
