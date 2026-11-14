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
 * 功能描述:  构造 CMainWindow 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QMainWindow(。
 * return:    无返回值（构造函数或析构函数）。
 */
CMainWindow::CMainWindow(QWidget* pParent) : QMainWindow(pParent) {
    setWindowTitle(QStringLiteral("Dialogue Pratice"));
    resize(1080, 720);
    setMinimumSize(940, 620);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pCentral = new QWidget(this);
    pCentral->setObjectName("AppRoot");
    auto* pLayout = new QVBoxLayout(pCentral);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pStack = new QStackedWidget(pCentral);
    m_pLoginPage = new CLoginPage(m_pStack);
    m_pHomePage = new CHomePage(m_pStack);
    m_pChatPage = new ChatPage(m_pStack);
    m_pStack->addWidget(m_pLoginPage);
    m_pStack->addWidget(m_pHomePage);
    m_pStack->addWidget(m_pChatPage);
    pLayout->addWidget(m_pStack);

    setCentralWidget(pCentral);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pBanner = new QLabel(pCentral);
    m_pBanner->setObjectName("Banner");
    m_pBanner->setAlignment(Qt::AlignCenter);
    m_pBanner->hide();

    /* 传输层：局域网自研服务端（声网 RTM 支持已移除，见 docs/plan-agora-rtm.md） */
    const AppConfig::TransportSettings_S stTransport = AppConfig::loadTransport();
    if (stTransport.transport.compare(QLatin1String("server"), Qt::CaseInsensitive) != 0) {
        qWarning() << "配置里的 transport 不是 server，但 RTM 支持已移除，仍按 server 运行："
                   << stTransport.transport;
    }
    m_pBridge = new CProtocolBridge(this);
    m_strTransportName = QStringLiteral("server");

    /* 本地消息缓存：兜住服务端重启后丢掉的历史 */
    m_pStore = new CMessageStore();
    const QString strDbPath =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/history.db");
    QString strStoreError;
    if (!m_pStore->open(strDbPath, &strStoreError)) {
        qWarning() << "本地消息缓存不可用：" << strStoreError;
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
            [this](const QByteArray& byAudio, int nDurationMs) {
                m_pBridge->sendVoice(byAudio, nDurationMs);
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
 * 功能描述:  重新计算尺寸事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 QResizeEvent*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::resizeEvent(QResizeEvent* pEvent) {
    QMainWindow::resizeEvent(pEvent);
    positionBanner();
}

/* ---------------------------------------------------------------- 登录 */

/*
 * 功能描述:  完成登录onrequested。
 * 作者:      JaTin
 * param [in]   strHost          服务器主机地址，类型 const QString&。
 * param [in]   uPort            服务器端口，类型 quint16。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onLoginRequested(const QString& strHost, quint16 uPort, const QString& strName) {
    m_strLastHost = strHost;
    m_uLastPort = uPort;
    m_strPendingHost = strHost;
    m_uPendingPort = uPort;
    m_strPendingName = strName;

    resetReconnectState();
    m_pLoginPage->clearError();
    m_pLoginPage->setBusy(true);
    m_pBridge->connectToServer(strHost, uPort);
}

/*
 * 功能描述:  执行on 协议桥接器 connected。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
 * 功能描述:  登录成功后切换到主页并初始化会话上下文。
 * 作者:      JaTin
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onLoginSucceeded(qint64 lUserId, const QString& strName, int nAge, const QString& strDesc) {
    m_uSelfId = lUserId;
    m_strSelfName = strName;
    m_nSelfAge = nAge;
    m_strSelfDesc = strDesc;

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

    showBanner(QStringLiteral("登录成功，账号 ID %1").arg(lUserId), false);
}

/* ---------------------------------------------------------------- 房间 */

/*
 * 功能描述:  加入on 房间。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onJoinRoom(const QString& strRoom) {
    m_strPendingRoom = strRoom;
    m_pHomePage->clearError();
    m_pHomePage->setBusy(true);
    m_pBridge->joinRoom(strRoom);
}

/*
 * 功能描述:  执行on 房间 joined。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   nCreated         输入数据，类型 bool。
 * param [in]   aMembers         初始成员列表，类型 const QVector<MemberInfo_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onRoomJoined(const QString& strRoom, bool nCreated, const QVector<MemberInfo_S>& aMembers) {
    m_pHomePage->setBusy(false);

    if (m_nReconnecting) {
        m_pChatPage->resumeRoom(strRoom, aMembers, m_uSelfId, m_strSelfName);
        m_pStack->setCurrentWidget(m_pChatPage);
        finishReconnect();
        return;
    }

    m_pChatPage->enterRoom(strRoom, nCreated, aMembers, m_uSelfId, m_strSelfName);
    m_pStack->setCurrentWidget(m_pChatPage);

    /* 服务端/声网的历史之外，再用本地缓存补齐（按 seq 去重，不会重复） */
    if (m_pStore != nullptr && m_pStore->isOpen()) {
        m_pStore->prune(m_strTransportName, strRoom, 500);
        m_pChatPage->mergeHistory(m_pStore->loadRecent(m_strTransportName, strRoom, 200));
    }

    showBanner(nCreated ? QStringLiteral("房间 %1 不存在，已自动创建").arg(strRoom)
                       : QStringLiteral("已加入房间 %1").arg(strRoom),
               false);
}

/*
 * 功能描述:  退出on 房间。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
 * 功能描述:  发送on 消息内容。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onSendMessage(const QString& strText) {
    m_pBridge->sendChat(strText);
}

/*
 * 功能描述:  执行on 历史消息 received。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   aMessages        消息记录，类型 const QVector<ChatMessage_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onHistoryReceived(const QString& strRoom, const QVector<ChatMessage_S>& aMessages) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (!strRoom.isEmpty() && strRoom != m_pChatPage->room()) return;
    m_pChatPage->mergeHistory(aMessages);
}

/*
 * 功能描述:  执行on 语音消息 received。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   lFrom            输入数据，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   byAudio          Opus 编码的音频数据，类型 const QByteArray&。
 * param [in]   nDurationMs      语音时长，单位毫秒，类型 int。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onVoiceReceived(qint64 lSeq, qint64 lFrom, const QString& strName,
                                 const QString& strRoom, const QByteArray& byAudio,
                                 int nDurationMs, qint64 lTs) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (strRoom != m_pChatPage->room()) return;

    m_pChatPage->appendVoice(lSeq, lFrom, strName, byAudio, nDurationMs, lTs);
}

/*
 * 功能描述:  收到聊天消息时更新数据模型并刷新消息视图。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   lFrom            输入数据，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onChatReceived(qint64 lSeq, qint64 lFrom, const QString& strName, const QString& strRoom,
                                const QString& strText, qint64 lTs) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (strRoom != m_pChatPage->room()) return;

    if (m_pStore != nullptr && m_pStore->isOpen()) {
        ChatMessage_S strMessage;
        strMessage.seq = lSeq;
        strMessage.from = lFrom;
        strMessage.name = strName;
        strMessage.text = strText;
        strMessage.ts = lTs;
        m_pStore->save(m_strTransportName, strRoom, strMessage);
    }

    m_pChatPage->appendChat(lSeq, lFrom, strName, strText, lTs);
}

/*
 * 功能描述:  执行on 系统提示词消息内容。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 const QString&。
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strOldName       输入数据，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onSystemMessage(const QString& pEvent, const QString& strRoom, qint64 lUserId,
                                 const QString& strName, const QString& strOldName,
                                 const QString& strText, qint64 lTs) {
    if (pEvent == QStringLiteral("welcome")) return;
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (!strRoom.isEmpty() && strRoom != m_pChatPage->room()) return;
    m_pChatPage->onSystemEvent(pEvent, lUserId, strName, strOldName, strText, lTs);
}

/* ---------------------------------------------------------------- 资料 */

/*
 * 功能描述:  执行onedit 个人资料；实现上连接 Qt 信号与槽。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onEditProfile() {
    auto* pDialog = new CProfileDialog(m_strSelfName, m_nSelfAge, m_strSelfDesc, this);
    pDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(pDialog, &CProfileDialog::saved, this,
            [this](const QString& strName, int nAge, const QString& strDesc) {
                m_pBridge->updateProfile(strName, nAge, strDesc);
            });

    pDialog->exec();
}

/*
 * 功能描述:  执行on 个人资料 updated。
 * 作者:      JaTin
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onProfileUpdated(qint64 lUserId, const QString& strName, int nAge, const QString& strDesc) {
    Q_UNUSED(lUserId)
    m_strSelfName = strName;
    m_nSelfAge = nAge;
    m_strSelfDesc = strDesc;
    m_pHomePage->setProfile(m_uSelfId, m_strSelfName, m_nSelfAge, m_strSelfDesc);
    showBanner(QStringLiteral("个人信息已更新"), false);
}

/* ---------------------------------------------------------------- 断线重连 */

/*
 * 功能描述:  连接断开时清理会话状态并提示用户重新连接。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
 * 功能描述:  重新连接ontimeout。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onReconnectTimeout() {
    if (m_strLastHost.isEmpty()) {
        resetToLogin(QStringLiteral("无法重连：缺少服务器地址"));
        return;
    }
    m_pBridge->connectToServer(m_strLastHost, m_uLastPort);
}

/*
 * 功能描述:  重新连接schedule。
 * 作者:      JaTin
 * param [in]   strReason        失败原因描述，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::scheduleReconnect(const QString& strReason) {
    if (m_nReconnectAttempt >= kMaxReconnectAttempts) {
        resetToLogin(QStringLiteral("重连失败（%1），请检查服务端后重新登录").arg(strReason));
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
 * 功能描述:  重新连接finish。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::finishReconnect() {
    m_nReconnecting = false;
    m_nReconnectAttempt = 0;
    m_pChatPage->setOnline(true);
    showBanner(QStringLiteral("已重新连接"), false);
}

/*
 * 功能描述:  重置reconnect 状态。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::resetReconnectState() {
    m_pReconnectTimer->stop();
    m_nReconnecting = false;
    m_nReconnectAttempt = 0;
}

/* ---------------------------------------------------------------- 错误 */

/*
 * 功能描述:  执行on 服务端错误。
 * 作者:      JaTin
 * param [in]   strCode          输入数据，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onServerError(const QString& strCode, const QString& strText) {
    Q_UNUSED(strCode)

    if (m_nReconnecting) {
        resetToLogin(QStringLiteral("重连被服务端拒绝：%1").arg(strText));
        return;
    }

    if (m_pStack->currentWidget() == m_pLoginPage) {
        m_pLoginPage->setBusy(false);
        m_pLoginPage->showError(strText.isEmpty() ? QStringLiteral("登录失败") : strText);
        m_nSuppressDisconnectNotice = true;
        m_pBridge->disconnectFromServer();
        return;
    }
    if (m_pStack->currentWidget() == m_pHomePage) {
        m_pHomePage->setBusy(false);
        m_pHomePage->showError(strText);
        return;
    }
    showBanner(strText, true);
}

/*
 * 功能描述:  执行onnetwork 错误。
 * 作者:      JaTin
 * param [in]   strMessage       消息内容，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onNetworkError(const QString& strMessage) {
    if (m_pStack->currentWidget() == m_pLoginPage) {
        m_pLoginPage->setBusy(false);
        m_pLoginPage->showError(strMessage);
        resetReconnectState();
        return;
    }

    /* 已登录状态下出现任何网络错误（RST、超时、断网）都走重连，而不是踢回登录页。 */
    if (m_nReconnecting || m_uSelfId != 0) {
        scheduleReconnect(strMessage);
        return;
    }

    showBanner(strMessage, true);
    resetToLogin(strMessage);
}

/*
 * 功能描述:  重置tologin。
 * 作者:      JaTin
 * param [in]   strReason        失败原因描述，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::resetToLogin(const QString& strReason) {
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

    if (!strReason.isEmpty()) {
        m_pLoginPage->showError(strReason);
    }
}

/*
 * 功能描述:  退出登录on。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::onLogout() {
    m_nSuppressDisconnectNotice = true;
    m_pBridge->disconnectFromServer();
    resetToLogin(QString());
}

/* ---------------------------------------------------------------- 顶部提示条 */

/*
 * 功能描述:  显示提示条。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   nError           错误信息，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::showBanner(const QString& strText, bool nError) {
    m_pBanner->setObjectName(nError ? QStringLiteral("BannerError") : QStringLiteral("Banner"));
    m_pBanner->style()->unpolish(m_pBanner);
    m_pBanner->style()->polish(m_pBanner);

    m_pBanner->setText(strText);
    m_pBanner->adjustSize();
    m_pBanner->show();
    m_pBanner->raise();
    positionBanner();

    /* 错误信息留久一点，免得用户没看清就消失 */
    m_pBannerTimer->start(nError ? 6000 : 3200);
}

/*
 * 功能描述:  执行position 提示条。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMainWindow::positionBanner() {
    if (m_pBanner == nullptr || !m_pBanner->isVisible()) return;
    m_pBanner->adjustSize();
    const int nX = (width() - m_pBanner->width()) / 2;
    m_pBanner->move(qMax(0, nX), 76);
}
