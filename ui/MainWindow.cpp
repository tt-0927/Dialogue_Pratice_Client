/**
 * @file  MainWindow.cpp
 * @brief 主窗口的实现：页面调度、连接状态提示、消息转发。
 */

#include "ui/MainWindow.h"

#include "business/MessageStore.h"
#include "business/SessionContext.h"
#include "core/AppConfig.h"
#include "core/Logger.h"
#include "net/ConnectionManager.h"
#include "net/ProtocolBridge.h"
#include "ui/ChatPage.h"
#include "ui/HomePage.h"
#include "ui/LoginPage.h"
#include "ui/ProfileDialog.h"
#include "ui/RegisterDialog.h"

#include <QDialog>
#include <QGuiApplication>
#include <QScreen>
#include <QLabel>
#include <QResizeEvent>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {
const QString kTag = QStringLiteral("ui");
/// @brief 本地历史缓存使用的 source 标识。
const QString kTransportName = QStringLiteral("server");
}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Dialogue Pratice"));

    // 桌面给一个舒服的默认尺寸；移动端由系统全屏接管。
    // 最小尺寸只保证布局不塌陷，不再卡 940×620——那在手机上根本达不到。
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        resize(qMin(1080, avail.width() * 9 / 10), qMin(720, avail.height() * 9 / 10));
    } else {
        resize(1080, 720);
    }
    setMinimumSize(360, 480);

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

    // ---- 传输层：局域网自研服务端 ----
    const AppConfig::TransportSettings transport = AppConfig::loadTransport();
    if (transport.transport.compare(QLatin1String("server"), Qt::CaseInsensitive) != 0) {
        DP_LOG_WARN(kTag, QStringLiteral("配置里的 transport 不是 server，仍按 server 运行：%1")
                              .arg(transport.transport));
    }
    bridge_ = new ProtocolBridge(this);
    connection_ = new net::ConnectionManager(bridge_, this);

    // ---- 本地消息缓存：兜住服务端重启后丢掉的历史 ----
    store_ = new MessageStore();
    const QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/history.db");
    QString storeError;
    if (!store_->open(dbPath, &storeError)) {
        DP_LOG_WARN(kTag, QStringLiteral("本地消息缓存不可用：%1").arg(storeError));
    }

    // 单一定时器：新横幅要能把旧横幅的隐藏计时重置掉，
    // 否则「登录成功」那条的 3.2s 计时会把刚弹出的错误提示提前关掉。
    bannerTimer_ = new QTimer(this);
    bannerTimer_->setSingleShot(true);
    connect(bannerTimer_, &QTimer::timeout, this, [this] { banner_->hide(); });

    // ---- 页面事件 ----
    connect(loginPage_, &LoginPage::loginRequested, this, &MainWindow::onLoginRequested);
    connect(loginPage_, &LoginPage::registerRequested, this, [this] {
        // 注册用独立连接，不影响主窗口状态；成功后把账号密码回填到登录表单
        RegisterDialog dialog(loginPage_->host(), loginPage_->port(), this);
        if (dialog.exec() == QDialog::Accepted) {
            loginPage_->setCredentials(dialog.username(), dialog.password());
            showBanner(QStringLiteral("注册成功，请用新账号登录"), false);
        }
    });
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

    // ---- 传输层事件 ----
    connect(bridge_, &ProtocolBridge::connected, this, &MainWindow::onBridgeConnected);
    connect(bridge_, &ProtocolBridge::disconnected, this, &MainWindow::onDisconnected);
    connect(bridge_, &ProtocolBridge::llmConfigReceived, this,
            [this](const QByteArray& json) { chatPage_->applyRemoteConfig(json); });
    connect(bridge_, &ProtocolBridge::loginSucceeded, this, &MainWindow::onLoginSucceeded);
    connect(bridge_, &ProtocolBridge::profileUpdated, this, &MainWindow::onProfileUpdated);
    connect(bridge_, &ProtocolBridge::roomJoined, this, &MainWindow::onRoomJoined);
    connect(bridge_, &ProtocolBridge::historyReceived, this, &MainWindow::onHistoryReceived);
    connect(bridge_, &ProtocolBridge::chatReceived, this, &MainWindow::onChatReceived);
    connect(bridge_, &ProtocolBridge::voiceReceived, this, &MainWindow::onVoiceReceived);
    connect(bridge_, &ProtocolBridge::systemMessage, this, &MainWindow::onSystemMessage);
    connect(bridge_, &ProtocolBridge::serverError, this, &MainWindow::onServerError);
    connect(bridge_, &ProtocolBridge::networkError, this, &MainWindow::onNetworkError);

    // ---- 重连事件 ----
    connect(connection_, &net::ConnectionManager::reconnectScheduled, this,
            &MainWindow::onReconnectScheduled);
    connect(connection_, &net::ConnectionManager::reconnectFailed, this,
            &MainWindow::onReconnectFailed);
    connect(connection_, &net::ConnectionManager::restored, this, &MainWindow::onRestored);
}

MainWindow::~MainWindow() = default;

void MainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    positionBanner();
}

// ---------------------------------------------------------------- 登录

void MainWindow::onLoginRequested(const QString& host, quint16 port, const QString& name,
                                 const QString& password) {
    pendingName_ = name;
    pendingPassword_ = password;

    loginPage_->clearError();
    loginPage_->setBusy(true);
    connection_->connectTo(host, port);
}

void MainWindow::onBridgeConnected() {
    // 首次登录与重连走同一套：都用登录时保存的账号密码。
    // 重连不经过登录页，所以必须把账号密码记在内存里。
    bridge_->login(pendingName_, pendingPassword_, 0, QString());
}

void MainWindow::onLoginSucceeded(qint64 userId, const QString& name, int age,
                                  const QString& desc) {
    business::SessionContext& ctx = business::SessionContext::instance();
    ctx.setProfile(userId, name, age, desc);

    loginPage_->setBusy(false);
    homePage_->setProfile(ctx.userId(), ctx.name(), ctx.age(), ctx.desc());
    homePage_->setServerText(
        QStringLiteral("已连接 %1:%2").arg(connection_->host()).arg(connection_->port()));

    if (connection_->isReconnecting()) {
        if (!pendingRoom_.isEmpty()) {
            bridge_->joinRoom(pendingRoom_);   // 回到断线前所在的房间
        } else {
            connection_->markRestored();
        }
        return;
    }

    homePage_->clearError();
    stack_->setCurrentWidget(homePage_);
    homePage_->focusRoomInput();

    DP_LOG_INFO(kTag, QStringLiteral("login ok, userId=%1").arg(userId));
    showBanner(QStringLiteral("登录成功，账号 ID %1").arg(userId), false);
}

// ---------------------------------------------------------------- 房间

void MainWindow::onJoinRoom(const QString& room) {
    pendingRoom_ = room;
    homePage_->clearError();
    homePage_->setBusy(true);
    bridge_->joinRoom(room);
}

void MainWindow::onRoomJoined(const QString& room, bool created,
                              const QVector<MemberInfo>& members) {
    business::SessionContext& ctx = business::SessionContext::instance();
    homePage_->setBusy(false);

    if (connection_->isReconnecting()) {
        chatPage_->resumeRoom(room, members, ctx.userId(), ctx.name());
        stack_->setCurrentWidget(chatPage_);
        connection_->markRestored();
        return;
    }

    chatPage_->enterRoom(room, created, members, ctx.userId(), ctx.name());
    stack_->setCurrentWidget(chatPage_);

    // 服务端的历史之外，再用本地缓存补齐（按 seq 去重，不会重复）
    if (store_ != nullptr && store_->isOpen()) {
        store_->prune(kTransportName, room, 500);
        chatPage_->mergeHistory(store_->loadRecent(kTransportName, room, 200));
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
        store_->save(kTransportName, room, message);
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
    business::SessionContext& ctx = business::SessionContext::instance();
    auto* dialog = new ProfileDialog(ctx.name(), ctx.age(), ctx.desc(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &ProfileDialog::saved, this,
            [this](const QString& name, int age, const QString& desc) {
                bridge_->updateProfile(name, age, desc);
            });

    dialog->exec();
}

void MainWindow::onProfileUpdated(qint64 userId, const QString& name, int age,
                                  const QString& desc) {
    Q_UNUSED(userId)
    business::SessionContext& ctx = business::SessionContext::instance();
    ctx.setProfile(ctx.userId(), name, age, desc);
    homePage_->setProfile(ctx.userId(), ctx.name(), ctx.age(), ctx.desc());
    showBanner(QStringLiteral("个人信息已更新"), false);
}

// ---------------------------------------------------------------- 断线重连

void MainWindow::onDisconnected() {
    if (suppressDisconnectNotice_) {
        suppressDisconnectNotice_ = false;
        return;
    }
    if (stack_->currentWidget() == loginPage_) return;

    connection_->notifyDisconnected(QStringLiteral("连接已断开"));
}

void MainWindow::onReconnectScheduled(int attempt, int maxAttempts, int delayMs) {
    chatPage_->setOnline(false);
    showBanner(QStringLiteral("连接已断开，%1 秒后第 %2/%3 次重连…")
                   .arg(delayMs / 1000)
                   .arg(attempt)
                   .arg(maxAttempts),
               true);
}

void MainWindow::onReconnectFailed(const QString& reason) {
    resetToLogin(QStringLiteral("重连失败（%1），请检查服务端后重新登录").arg(reason));
}

void MainWindow::onRestored() {
    chatPage_->setOnline(true);
    showBanner(QStringLiteral("已重新连接"), false);
}

// ---------------------------------------------------------------- 错误

void MainWindow::onServerError(const QString& code, const QString& text) {
    Q_UNUSED(code)

    if (connection_->isReconnecting()) {
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
        connection_->setAutoReconnect(false);
        return;
    }

    // 已登录状态下出现任何网络错误（RST、超时、断网）都走重连，而不是踢回登录页。
    const business::SessionContext& ctx = business::SessionContext::instance();
    if (connection_->isReconnecting() || ctx.userId() != 0) {
        connection_->notifyDisconnected(message);
        return;
    }

    showBanner(message, true);
    resetToLogin(message);
}

void MainWindow::resetToLogin(const QString& reason) {
    // 清理过程中会触发 disconnect，避免被当成「意外断线」再走一次重连。
    connection_->setAutoReconnect(false);

    chatPage_->clearRoom();
    homePage_->setBusy(false);
    homePage_->clearError();
    loginPage_->setBusy(false);
    stack_->setCurrentWidget(loginPage_);

    business::SessionContext::instance().clearProfile();
    pendingRoom_.clear();

    if (!reason.isEmpty()) {
        loginPage_->showError(reason);
    }

    connection_->setAutoReconnect(true);
}

void MainWindow::onLogout() {
    suppressDisconnectNotice_ = true;
    connection_->disconnectFrom();
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
