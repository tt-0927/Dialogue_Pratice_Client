/*
 * 文件名:    MainWindow.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  主窗口的实现：页面调度、连接状态提示、消息转发。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
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
/* 本地历史缓存使用的 source 标识。 */
const QString kTransportName = QStringLiteral("server");
/* namespace */
}

/*
 * 功能描述:  执行 CMainWindow 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CMainWindow::CMainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Dialogue Pratice"));

    /* 桌面给一个舒服的默认尺寸；移动端由系统全屏接管。 */
    /* 最小尺寸只保证布局不塌陷，不再卡 940×620——那在手机上根本达不到。 */
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        resize(qMin(1080, avail.width() * 9 / 10), qMin(720, avail.height() * 9 / 10));
    } else {
        resize(1080, 720);
    }
    setMinimumSize(360, 480);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* central = new QWidget(this);
    central->setObjectName("AppRoot");
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pStack = new QStackedWidget(central);
    m_pLoginPage = new CLoginPage(m_pStack);
    m_pHomePage = new CHomePage(m_pStack);
    m_pChatPage = new ChatPage(m_pStack);
    m_pStack->addWidget(m_pLoginPage);
    m_pStack->addWidget(m_pHomePage);
    m_pStack->addWidget(m_pChatPage);
    layout->addWidget(m_pStack);

    setCentralWidget(central);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pBanner = new QLabel(central);
    m_pBanner->setObjectName("Banner");
    m_pBanner->setAlignment(Qt::AlignCenter);
    m_pBanner->hide();

    /* ---- 传输层：局域网自研服务端 ---- */
    const AppConfig::TransportSettings_S transport = AppConfig::loadTransport();
    if (transport.transport.compare(QLatin1String("server"), Qt::CaseInsensitive) != 0) {
        DP_LOG_WARN(kTag, QStringLiteral("配置里的 transport 不是 server，仍按 server 运行：%1")
                              .arg(transport.transport));
    }
    m_pBridge = new CProtocolBridge(this);
    m_pConnection = new net::ConnectionManager(m_pBridge, this);

    /* ---- 本地消息缓存：兜住服务端重启后丢掉的历史 ---- */
    m_pStore = new CMessageStore();
    const QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/history.db");
    QString storeError;
    if (!m_pStore->open(dbPath, &storeError)) {
        DP_LOG_WARN(kTag, QStringLiteral("本地消息缓存不可用：%1").arg(storeError));
    }

    /* 单一定时器：新横幅要能把旧横幅的隐藏计时重置掉， */
    /* 否则「登录成功」那条的 3.2s 计时会把刚弹出的错误提示提前关掉。 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pBannerTimer = new QTimer(this);
    m_pBannerTimer->setSingleShot(true);
    connect(m_pBannerTimer, &QTimer::timeout, this, [this] { m_pBanner->hide(); });

    /* ---- 页面事件 ---- */
    connect(m_pLoginPage, &CLoginPage::loginRequested, this, &CMainWindow::onLoginRequested);
    connect(m_pLoginPage, &CLoginPage::registerRequested, this, [this] {
        /* 注册用独立连接，不影响主窗口状态；成功后把账号密码回填到登录表单 */
        CRegisterDialog dialog(m_pLoginPage->host(), m_pLoginPage->port(), this);
        if (dialog.exec() == QDialog::Accepted) {
            m_pLoginPage->setCredentials(dialog.username(), dialog.password());
            showBanner(QStringLiteral("注册成功，请用新账号登录"), false);
        }
    });
    connect(m_pHomePage, &CHomePage::joinRoomRequested, this, &CMainWindow::onJoinRoom);
    connect(m_pHomePage, &CHomePage::editProfileRequested, this, &CMainWindow::onEditProfile);
    connect(m_pHomePage, &CHomePage::logoutRequested, this, &CMainWindow::onLogout);
    connect(m_pChatPage, &ChatPage::backRequested, this, &CMainWindow::onLeaveRoom);
    connect(m_pChatPage, &ChatPage::sendRequested, this, &CMainWindow::onSendMessage);
    connect(m_pChatPage, &ChatPage::sendVoiceRequested, this,
            [this](const QByteArray& audio, int durationMs) {
                m_pBridge->sendVoice(audio, durationMs);
            });
    connect(m_pChatPage, &ChatPage::notice, this, &CMainWindow::showBanner);

    /* ---- 传输层事件 ---- */
    connect(m_pBridge, &CProtocolBridge::connected, this, &CMainWindow::onBridgeConnected);
    connect(m_pBridge, &CProtocolBridge::disconnected, this, &CMainWindow::onDisconnected);
    connect(m_pBridge, &CProtocolBridge::llmConfigReceived, this,
            [this](const QByteArray& json) { m_pChatPage->applyRemoteConfig(json); });
    connect(m_pBridge, &CProtocolBridge::loginSucceeded, this, &CMainWindow::onLoginSucceeded);
    connect(m_pBridge, &CProtocolBridge::profileUpdated, this, &CMainWindow::onProfileUpdated);
    connect(m_pBridge, &CProtocolBridge::roomJoined, this, &CMainWindow::onRoomJoined);
    connect(m_pBridge, &CProtocolBridge::historyReceived, this, &CMainWindow::onHistoryReceived);
    connect(m_pBridge, &CProtocolBridge::chatReceived, this, &CMainWindow::onChatReceived);
    connect(m_pBridge, &CProtocolBridge::voiceReceived, this, &CMainWindow::onVoiceReceived);
    connect(m_pBridge, &CProtocolBridge::systemMessage, this, &CMainWindow::onSystemMessage);
    connect(m_pBridge, &CProtocolBridge::serverError, this, &CMainWindow::onServerError);
    connect(m_pBridge, &CProtocolBridge::networkError, this, &CMainWindow::onNetworkError);

    /* ---- 重连事件 ---- */
    connect(m_pConnection, &net::ConnectionManager::reconnectScheduled, this,
            &CMainWindow::onReconnectScheduled);
    connect(m_pConnection, &net::ConnectionManager::reconnectFailed, this,
            &CMainWindow::onReconnectFailed);
    connect(m_pConnection, &net::ConnectionManager::restored, this, &CMainWindow::onRestored);
}

CMainWindow::~CMainWindow() = default;

/*
 * 功能描述:  执行 resizeEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void CMainWindow::resizeEvent(QResizeEvent* event) {
    QMainWindow::resizeEvent(event);
    positionBanner();
}

/* ---------------------------------------------------------------- 登录 */

void CMainWindow::onLoginRequested(const QString& host, quint16 port, const QString& name,
                                 const QString& password) {
    m_strPendingName = name;
    m_strPendingPassword = password;

    m_pLoginPage->clearError();
    m_pLoginPage->setBusy(true);
    m_pConnection->connectTo(host, port);
}

/*
 * 功能描述:  执行 onBridgeConnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onBridgeConnected() {
    /* 首次登录与重连走同一套：都用登录时保存的账号密码。 */
    /* 重连不经过登录页，所以必须把账号密码记在内存里。 */
    m_pBridge->login(m_strPendingName, m_strPendingPassword, 0, QString());
}

void CMainWindow::onLoginSucceeded(qint64 userId, const QString& name, int age,
                                  const QString& desc) {
    business::CSessionContext& ctx = business::CSessionContext::instance();
    ctx.setProfile(userId, name, age, desc);

    m_pLoginPage->setBusy(false);
    m_pHomePage->setProfile(ctx.userId(), ctx.name(), ctx.age(), ctx.desc());
    m_pHomePage->setServerText(
        QStringLiteral("已连接 %1:%2").arg(m_pConnection->host()).arg(m_pConnection->port()));

    if (m_pConnection->isReconnecting()) {
        if (!m_strPendingRoom.isEmpty()) {
            /* 回到断线前所在的房间 */
            m_pBridge->joinRoom(m_strPendingRoom);
        } else {
            m_pConnection->markRestored();
        }
        return;
    }

    m_pHomePage->clearError();
    m_pStack->setCurrentWidget(m_pHomePage);
    m_pHomePage->focusRoomInput();

    DP_LOG_INFO(kTag, QStringLiteral("login ok, userId=%1").arg(userId));
    showBanner(QStringLiteral("登录成功，账号 ID %1").arg(userId), false);
}

/* ---------------------------------------------------------------- 房间 */

/*
 * 功能描述:  执行 onJoinRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  room  参数。
 * return:    void
 */
void CMainWindow::onJoinRoom(const QString& room) {
    m_strPendingRoom = room;
    m_pHomePage->clearError();
    m_pHomePage->setBusy(true);
    m_pBridge->joinRoom(room);
}

void CMainWindow::onRoomJoined(const QString& room, bool created,
                              const QVector<MemberInfo_S>& members) {
    business::CSessionContext& ctx = business::CSessionContext::instance();
    m_pHomePage->setBusy(false);

    if (m_pConnection->isReconnecting()) {
        m_pChatPage->resumeRoom(room, members, ctx.userId(), ctx.name());
        m_pStack->setCurrentWidget(m_pChatPage);
        m_pConnection->markRestored();
        return;
    }

    m_pChatPage->enterRoom(room, created, members, ctx.userId(), ctx.name());
    m_pStack->setCurrentWidget(m_pChatPage);

    /* 服务端的历史之外，再用本地缓存补齐（按 seq 去重，不会重复） */
    if (m_pStore != nullptr && m_pStore->isOpen()) {
        m_pStore->prune(kTransportName, room, 500);
        m_pChatPage->mergeHistory(m_pStore->loadRecent(kTransportName, room, 200));
    }

    showBanner(created ? QStringLiteral("房间 %1 不存在，已自动创建").arg(room)
                       : QStringLiteral("已加入房间 %1").arg(room),
               false);
}

/*
 * 功能描述:  执行 onLeaveRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onLeaveRoom() {
    if (!m_pChatPage->room().isEmpty()) {
        m_pBridge->leaveRoom();
    }
    m_strPendingRoom.clear();
    m_pChatPage->clearRoom();
    m_pStack->setCurrentWidget(m_pHomePage);
    m_pHomePage->focusRoomInput();
}

/* ---------------------------------------------------------------- 消息 */

/*
 * 功能描述:  执行 onSendMessage 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CMainWindow::onSendMessage(const QString& text) {
    m_pBridge->sendChat(text);
}

/*
 * 功能描述:  执行 onHistoryReceived 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  room  参数。
 * param [in]  messages  参数。
 * return:    void
 */
void CMainWindow::onHistoryReceived(const QString& room, const QVector<ChatMessage_S>& messages) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (!room.isEmpty() && room != m_pChatPage->room()) return;
    m_pChatPage->mergeHistory(messages);
}

void CMainWindow::onVoiceReceived(qint64 seq, qint64 from, const QString& name,
                                 const QString& room, const QByteArray& audio,
                                 int durationMs, qint64 ts) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (room != m_pChatPage->room()) return;

    m_pChatPage->appendVoice(seq, from, name, audio, durationMs, ts);
}

void CMainWindow::onChatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                                const QString& text, qint64 ts) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (room != m_pChatPage->room()) return;

    if (m_pStore != nullptr && m_pStore->isOpen()) {
        ChatMessage_S message;
        message.seq = seq;
        message.from = from;
        message.name = name;
        message.text = text;
        message.ts = ts;
        m_pStore->save(kTransportName, room, message);
    }

    m_pChatPage->appendChat(seq, from, name, text, ts);
}

void CMainWindow::onSystemMessage(const QString& event, const QString& room, qint64 userId,
                                 const QString& name, const QString& oldName,
                                 const QString& text, qint64 ts) {
    if (event == QStringLiteral("welcome")) return;
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (!room.isEmpty() && room != m_pChatPage->room()) return;
    m_pChatPage->onSystemEvent(event, userId, name, oldName, text, ts);
}

/* ---------------------------------------------------------------- 资料 */

/*
 * 功能描述:  执行 onEditProfile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onEditProfile() {
    business::CSessionContext& ctx = business::CSessionContext::instance();
    auto* dialog = new CProfileDialog(ctx.name(), ctx.age(), ctx.desc(), this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &CProfileDialog::saved, this,
            [this](const QString& name, int age, const QString& desc) {
                m_pBridge->updateProfile(name, age, desc);
            });

    dialog->exec();
}

void CMainWindow::onProfileUpdated(qint64 userId, const QString& name, int age,
                                  const QString& desc) {
    Q_UNUSED(userId)
    business::CSessionContext& ctx = business::CSessionContext::instance();
    ctx.setProfile(ctx.userId(), name, age, desc);
    m_pHomePage->setProfile(ctx.userId(), ctx.name(), ctx.age(), ctx.desc());
    showBanner(QStringLiteral("个人信息已更新"), false);
}

/* ---------------------------------------------------------------- 断线重连 */

/*
 * 功能描述:  执行 onDisconnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onDisconnected() {
    if (m_nSuppressDisconnectNotice) {
        m_nSuppressDisconnectNotice = false;
        return;
    }
    if (m_pStack->currentWidget() == m_pLoginPage) return;

    m_pConnection->notifyDisconnected(QStringLiteral("连接已断开"));
}

/*
 * 功能描述:  执行 onReconnectScheduled 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  attempt  参数。
 * param [in]  maxAttempts  参数。
 * param [in]  delayMs  参数。
 * return:    void
 */
void CMainWindow::onReconnectScheduled(int attempt, int maxAttempts, int delayMs) {
    m_pChatPage->setOnline(false);
    showBanner(QStringLiteral("连接已断开，%1 秒后第 %2/%3 次重连…")
                   .arg(delayMs / 1000)
                   .arg(attempt)
                   .arg(maxAttempts),
               true);
}

/*
 * 功能描述:  执行 onReconnectFailed 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  reason  参数。
 * return:    void
 */
void CMainWindow::onReconnectFailed(const QString& reason) {
    resetToLogin(QStringLiteral("重连失败（%1），请检查服务端后重新登录").arg(reason));
}

/*
 * 功能描述:  执行 onRestored 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onRestored() {
    m_pChatPage->setOnline(true);
    showBanner(QStringLiteral("已重新连接"), false);
}

/* ---------------------------------------------------------------- 错误 */

/*
 * 功能描述:  执行 onServerError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  code  参数。
 * param [in]  text  参数。
 * return:    void
 */
void CMainWindow::onServerError(const QString& code, const QString& text) {
    Q_UNUSED(code)

    if (m_pConnection->isReconnecting()) {
        resetToLogin(QStringLiteral("重连被服务端拒绝：%1").arg(text));
        return;
    }

    if (m_pStack->currentWidget() == m_pLoginPage) {
        m_pLoginPage->setBusy(false);
        m_pLoginPage->showError(text.isEmpty() ? QStringLiteral("登录失败") : text);
        m_nSuppressDisconnectNotice = true;
        m_pBridge->disconnectFromServer();
        return;
    }
    if (m_pStack->currentWidget() == m_pHomePage) {
        m_pHomePage->setBusy(false);
        m_pHomePage->showError(text);
        return;
    }
    showBanner(text, true);
}

/*
 * 功能描述:  执行 onNetworkError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  message  参数。
 * return:    void
 */
void CMainWindow::onNetworkError(const QString& message) {
    if (m_pStack->currentWidget() == m_pLoginPage) {
        m_pLoginPage->setBusy(false);
        m_pLoginPage->showError(message);
        m_pConnection->setAutoReconnect(false);
        return;
    }

    /* 已登录状态下出现任何网络错误（RST、超时、断网）都走重连，而不是踢回登录页。 */
    const business::CSessionContext& ctx = business::CSessionContext::instance();
    if (m_pConnection->isReconnecting() || ctx.userId() != 0) {
        m_pConnection->notifyDisconnected(message);
        return;
    }

    showBanner(message, true);
    resetToLogin(message);
}

/*
 * 功能描述:  执行 resetToLogin 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  reason  参数。
 * return:    void
 */
void CMainWindow::resetToLogin(const QString& reason) {
    /* 清理过程中会触发 disconnect，避免被当成「意外断线」再走一次重连。 */
    m_pConnection->setAutoReconnect(false);

    m_pChatPage->clearRoom();
    m_pHomePage->setBusy(false);
    m_pHomePage->clearError();
    m_pLoginPage->setBusy(false);
    m_pStack->setCurrentWidget(m_pLoginPage);

    business::CSessionContext::instance().clearProfile();
    m_strPendingRoom.clear();

    if (!reason.isEmpty()) {
        m_pLoginPage->showError(reason);
    }

    m_pConnection->setAutoReconnect(true);
}

/*
 * 功能描述:  执行 onLogout 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onLogout() {
    m_nSuppressDisconnectNotice = true;
    m_pConnection->disconnectFrom();
    resetToLogin(QString());
}

/* ---------------------------------------------------------------- 顶部提示条 */

/*
 * 功能描述:  执行 showBanner 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * param [in]  error  参数。
 * return:    void
 */
void CMainWindow::showBanner(const QString& text, bool error) {
    m_pBanner->setObjectName(error ? QStringLiteral("BannerError") : QStringLiteral("Banner"));
    m_pBanner->style()->unpolish(m_pBanner);
    m_pBanner->style()->polish(m_pBanner);

    m_pBanner->setText(text);
    m_pBanner->adjustSize();
    m_pBanner->show();
    m_pBanner->raise();
    positionBanner();

    /* 错误信息留久一点，免得用户没看清就消失 */
    m_pBannerTimer->start(error ? 6000 : 3200);
}

/*
 * 功能描述:  执行 positionBanner 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::positionBanner() {
    if (m_pBanner == nullptr || !m_pBanner->isVisible()) return;
    m_pBanner->adjustSize();
    const int x = (width() - m_pBanner->width()) / 2;
    m_pBanner->move(qMax(0, x), 76);
}
