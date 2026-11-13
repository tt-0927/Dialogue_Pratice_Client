/*
 * 文件名:    MainWindow.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMainWindow 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMainWindow.cpp */
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

/*
 * 功能描述:  执行 CMainWindow 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CMainWindow::CMainWindow(QWidget* parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("Dialogue Pratice"));
    resize(1080, 720);
    setMinimumSize(940, 620);

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

    /* 传输层：局域网自研服务端（声网 RTM 支持已移除，见 docs/plan-agora-rtm.md） */
    const AppConfig::TransportSettings_S transport = AppConfig::loadTransport();
    if (transport.transport.compare(QLatin1String("server"), Qt::CaseInsensitive) != 0) {
        qWarning() << "配置里的 transport 不是 server，但 RTM 支持已移除，仍按 server 运行："
                   << transport.transport;
    }
    m_pBridge = new CProtocolBridge(this);
    m_strTransportName = QStringLiteral("server");

    /* 本地消息缓存：兜住服务端重启后丢掉的历史 */
    m_pStore = new CMessageStore();
    const QString dbPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/history.db");
    QString storeError;
    if (!m_pStore->open(dbPath, &storeError)) {
        qWarning() << "本地消息缓存不可用：" << storeError;
    }

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pReconnectTimer = new QTimer(this);
    m_pReconnectTimer->setSingleShot(true);
    connect(m_pReconnectTimer, &QTimer::timeout, this, &CMainWindow::onReconnectTimeout);

    /* 单一定时器：新横幅要能把旧横幅的隐藏计时重置掉， */
    /* 否则「登录成功」那条的 3.2s 计时会把刚弹出的错误提示提前关掉。 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pBannerTimer = new QTimer(this);
    m_pBannerTimer->setSingleShot(true);
    connect(m_pBannerTimer, &QTimer::timeout, this, [this] { m_pBanner->hide(); });

    connect(m_pLoginPage, &CLoginPage::loginRequested, this, &CMainWindow::onLoginRequested);
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

    connect(m_pBridge, &CProtocolBridge::connected, this, &CMainWindow::onBridgeConnected);
    connect(m_pBridge, &CProtocolBridge::disconnected, this, &CMainWindow::onDisconnected);
    connect(m_pBridge, &CProtocolBridge::loginSucceeded, this, &CMainWindow::onLoginSucceeded);
    connect(m_pBridge, &CProtocolBridge::profileUpdated, this, &CMainWindow::onProfileUpdated);
    connect(m_pBridge, &CProtocolBridge::roomJoined, this, &CMainWindow::onRoomJoined);
    connect(m_pBridge, &CProtocolBridge::historyReceived, this, &CMainWindow::onHistoryReceived);
    connect(m_pBridge, &CProtocolBridge::chatReceived, this, &CMainWindow::onChatReceived);
    connect(m_pBridge, &CProtocolBridge::voiceReceived, this, &CMainWindow::onVoiceReceived);
    connect(m_pBridge, &CProtocolBridge::systemMessage, this, &CMainWindow::onSystemMessage);
    connect(m_pBridge, &CProtocolBridge::serverError, this, &CMainWindow::onServerError);
    connect(m_pBridge, &CProtocolBridge::networkError, this, &CMainWindow::onNetworkError);
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

/*
 * 功能描述:  执行 onLoginRequested 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  host  参数。
 * param [in]  port  参数。
 * param [in]  name  参数。
 * return:    void
 */
void CMainWindow::onLoginRequested(const QString& host, quint16 port, const QString& name) {
    m_strLastHost = host;
    m_uLastPort = port;
    m_strPendingHost = host;
    m_uPendingPort = port;
    m_strPendingName = name;

    resetReconnectState();
    m_pLoginPage->clearError();
    m_pLoginPage->setBusy(true);
    m_pBridge->connectToServer(host, port);
}

/*
 * 功能描述:  执行 onBridgeConnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onBridgeConnected() {
    if (m_nReconnecting) {
        /* 重连：用已保存的身份直接重登，不再走登录页 */
        m_pBridge->login(m_strSelfName, m_nSelfAge, m_strSelfDesc);
        return;
    }
    m_pBridge->login(m_strPendingName, 0, QString());
}

/*
 * 功能描述:  执行 onLoginSucceeded 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  userId  参数。
 * param [in]  name  参数。
 * param [in]  age  参数。
 * param [in]  desc  参数。
 * return:    void
 */
void CMainWindow::onLoginSucceeded(qint64 userId, const QString& name, int age, const QString& desc) {
    m_uSelfId = userId;
    m_strSelfName = name;
    m_nSelfAge = age;
    m_strSelfDesc = desc;

    m_pLoginPage->setBusy(false);
    m_pHomePage->setProfile(m_uSelfId, m_strSelfName, m_nSelfAge, m_strSelfDesc);
    m_pHomePage->setServerText(QStringLiteral("已连接 %1:%2").arg(m_strPendingHost).arg(m_uPendingPort));

    if (m_nReconnecting) {
        if (!m_strPendingRoom.isEmpty()) {
            /* 回到断线前所在的房间 */
            m_pBridge->joinRoom(m_strPendingRoom);
        } else {
            finishReconnect();
        }
        return;
    }

    m_pHomePage->clearError();
    m_pStack->setCurrentWidget(m_pHomePage);
    m_pHomePage->focusRoomInput();

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

/*
 * 功能描述:  执行 onRoomJoined 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  room  参数。
 * param [in]  created  参数。
 * param [in]  members  参数。
 * return:    void
 */
void CMainWindow::onRoomJoined(const QString& room, bool created, const QVector<MemberInfo_S>& members) {
    m_pHomePage->setBusy(false);

    if (m_nReconnecting) {
        m_pChatPage->resumeRoom(room, members, m_uSelfId, m_strSelfName);
        m_pStack->setCurrentWidget(m_pChatPage);
        finishReconnect();
        return;
    }

    m_pChatPage->enterRoom(room, created, members, m_uSelfId, m_strSelfName);
    m_pStack->setCurrentWidget(m_pChatPage);

    /* 服务端/声网的历史之外，再用本地缓存补齐（按 seq 去重，不会重复） */
    if (m_pStore != nullptr && m_pStore->isOpen()) {
        m_pStore->prune(m_strTransportName, room, 500);
        m_pChatPage->mergeHistory(m_pStore->loadRecent(m_strTransportName, room, 200));
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
        m_pStore->save(m_strTransportName, room, message);
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
    auto* dialog = new CProfileDialog(m_strSelfName, m_nSelfAge, m_strSelfDesc, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &CProfileDialog::saved, this,
            [this](const QString& name, int age, const QString& desc) {
                m_pBridge->updateProfile(name, age, desc);
            });

    dialog->exec();
}

/*
 * 功能描述:  执行 onProfileUpdated 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  userId  参数。
 * param [in]  name  参数。
 * param [in]  age  参数。
 * param [in]  desc  参数。
 * return:    void
 */
void CMainWindow::onProfileUpdated(qint64 userId, const QString& name, int age, const QString& desc) {
    Q_UNUSED(userId)
    m_strSelfName = name;
    m_nSelfAge = age;
    m_strSelfDesc = desc;
    m_pHomePage->setProfile(m_uSelfId, m_strSelfName, m_nSelfAge, m_strSelfDesc);
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
    /* 已经进入重连流程（socket error 可能先于 disconnected 到达） */
    if (m_nReconnecting) return;

    scheduleReconnect(QStringLiteral("连接已断开"));
}

/*
 * 功能描述:  执行 onReconnectTimeout 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onReconnectTimeout() {
    if (m_strLastHost.isEmpty()) {
        resetToLogin(QStringLiteral("无法重连：缺少服务器地址"));
        return;
    }
    m_pBridge->connectToServer(m_strLastHost, m_uLastPort);
}

/*
 * 功能描述:  执行 scheduleReconnect 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  reason  参数。
 * return:    void
 */
void CMainWindow::scheduleReconnect(const QString& reason) {
    if (m_nReconnectAttempt >= kMaxReconnectAttempts) {
        resetToLogin(QStringLiteral("重连失败（%1），请检查服务端后重新登录").arg(reason));
        return;
    }

    m_nReconnecting = true;
    ++m_nReconnectAttempt;
    m_pChatPage->setOnline(false);

    showBanner(QStringLiteral("连接已断开，%1 秒后第 %2/%3 次重连…")
                   .arg(kReconnectIntervalMs / 1000)
                   .arg(m_nReconnectAttempt)
                   .arg(kMaxReconnectAttempts),
               true);
    m_pReconnectTimer->start(kReconnectIntervalMs);
}

/*
 * 功能描述:  执行 finishReconnect 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::finishReconnect() {
    m_nReconnecting = false;
    m_nReconnectAttempt = 0;
    m_pChatPage->setOnline(true);
    showBanner(QStringLiteral("已重新连接"), false);
}

/*
 * 功能描述:  执行 resetReconnectState 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::resetReconnectState() {
    m_pReconnectTimer->stop();
    m_nReconnecting = false;
    m_nReconnectAttempt = 0;
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

    if (m_nReconnecting) {
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
        resetReconnectState();
        return;
    }

    /* 已登录状态下出现任何网络错误（RST、超时、断网）都走重连，而不是踢回登录页。 */
    if (m_nReconnecting || m_uSelfId != 0) {
        scheduleReconnect(message);
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
    resetReconnectState();
    m_pChatPage->clearRoom();
    m_pHomePage->setBusy(false);
    m_pHomePage->clearError();
    m_pLoginPage->setBusy(false);
    m_pStack->setCurrentWidget(m_pLoginPage);

    m_uSelfId = 0;
    m_strSelfName.clear();
    m_nSelfAge = 0;
    m_strSelfDesc.clear();
    m_strPendingRoom.clear();

    if (!reason.isEmpty()) {
        m_pLoginPage->showError(reason);
    }
}

/*
 * 功能描述:  执行 onLogout 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CMainWindow::onLogout() {
    m_nSuppressDisconnectNotice = true;
    m_pBridge->disconnectFromServer();
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
