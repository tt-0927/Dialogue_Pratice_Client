/**
 * @file       MainWindow.cpp
 * @brief      主窗口的实现：页面调度、连接状态提示、消息转发。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
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
/**
 * @brief  本地历史缓存使用的 source 标识。
 */
const QString kTransportName = QStringLiteral("server");
/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CMainWindow 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QWidget* pParent) : QMainWindow(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CMainWindow::CMainWindow(QWidget* pParent) : QMainWindow(pParent) {
    setWindowTitle(QStringLiteral("Dialogue Pratice"));

    /**
     * @brief  桌面给一个舒服的默认尺寸；移动端由系统全屏接管。 最小尺寸只保证布局不塌陷，不再卡 940×620——那在手机上根本达不到。
     */
    if (QScreen* screen = QGuiApplication::primaryScreen()) {
        const QRect stAvail = screen->availableGeometry();
        resize(qMin(1080, stAvail.width() * 9 / 10), qMin(720, stAvail.height() * 9 / 10));
    } else {
        resize(1080, 720);
    }
    setMinimumSize(360, 480);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    auto* pCentral = new QWidget(this);
    pCentral->setObjectName("AppRoot");
    auto* pLayout = new QVBoxLayout(pCentral);
    pLayout->setContentsMargins(0, 0, 0, 0);
    pLayout->setSpacing(0);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pStack = new QStackedWidget(pCentral);
    m_pLoginPage = new CLoginPage(m_pStack);
    m_pHomePage = new CHomePage(m_pStack);
    m_pChatPage = new ChatPage(m_pStack);
    m_pStack->addWidget(m_pLoginPage);
    m_pStack->addWidget(m_pHomePage);
    m_pStack->addWidget(m_pChatPage);
    pLayout->addWidget(m_pStack);

    setCentralWidget(pCentral);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pBanner = new QLabel(pCentral);
    m_pBanner->setObjectName("Banner");
    m_pBanner->setAlignment(Qt::AlignCenter);
    m_pBanner->hide();

    /**
     * @brief  ---- 传输层：局域网自研服务端 ----。
     */
    const AppConfig::TransportSettings_S stTransport = AppConfig::loadTransport();
    if (stTransport.transport.compare(QLatin1String("server"), Qt::CaseInsensitive) != 0) {
        DP_LOG_WARN(kTag, QStringLiteral("配置里的 transport 不是 server，仍按 server 运行：%1")
                              .arg(stTransport.transport));
    }
    m_pBridge = new CProtocolBridge(this);
    m_pConnection = new net::ConnectionManager(m_pBridge, this);

    /**
     * @brief  ---- 本地消息缓存：兜住服务端重启后丢掉的历史 ----。
     */
    m_pStore = new CMessageStore();
    const QString strDbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
                           + QStringLiteral("/history.db");
    QString strStoreError;
    if (!m_pStore->open(strDbPath, &strStoreError)) {
        DP_LOG_WARN(kTag, QStringLiteral("本地消息缓存不可用：%1").arg(strStoreError));
    }

    /**
     * @brief  单一定时器：新横幅要能把旧横幅的隐藏计时重置掉， 否则「登录成功」那条的 3.2s 计时会把刚弹出的错误提示提前关掉。 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pBannerTimer = new QTimer(this);
    m_pBannerTimer->setSingleShot(true);
    connect(m_pBannerTimer, &QTimer::timeout, this, [this] { m_pBanner->hide(); });

    /**
     * @brief  ---- 页面事件 ----。
     */
    connect(m_pLoginPage, &CLoginPage::loginRequested, this, &CMainWindow::onLoginRequested);
    connect(m_pLoginPage, &CLoginPage::registerRequested, this, [this] {
        /**
         * @brief  注册用独立连接，不影响主窗口状态；成功后把账号密码回填到登录表单。
         */
        CRegisterDialog stDialog(m_pLoginPage->host(), m_pLoginPage->port(), this);
        if (stDialog.exec() == QDialog::Accepted) {
            m_pLoginPage->setCredentials(stDialog.username(), stDialog.password());
            showBanner(QStringLiteral("注册成功，请用新账号登录"), false);
        }
    });
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

    /**
     * @brief  ---- 传输层事件 ----。
     */
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

    /**
     * @brief  ---- 重连事件 ----。
     */
    connect(m_pConnection, &net::ConnectionManager::reconnectScheduled, this,
            &CMainWindow::onReconnectScheduled);
    connect(m_pConnection, &net::ConnectionManager::reconnectFailed, this,
            &CMainWindow::onReconnectFailed);
    connect(m_pConnection, &net::ConnectionManager::restored, this, &CMainWindow::onRestored);
}

CMainWindow::~CMainWindow() = default;

/**
 * @brief      重新计算尺寸事件。
 * @param[in] pEvent         事件；类型 QResizeEvent*。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::resizeEvent(QResizeEvent* pEvent) {
    QMainWindow::resizeEvent(pEvent);
    positionBanner();
}

/**
 * @brief      完成登录 onrequested。
 * @param[in] strHost        服务器主机地址；类型 const QString&。
 * @param[in] uPort          服务器端口；类型 quint16。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] strPassword    登录密码；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */

void CMainWindow::onLoginRequested(const QString& strHost, quint16 uPort, const QString& strName,
                                 const QString& strPassword) {
    m_strPendingName = strName;
    m_strPendingPassword = strPassword;

    m_pLoginPage->clearError();
    m_pLoginPage->setBusy(true);
    m_pConnection->connectTo(strHost, uPort);
}

/**
 * @brief      执行 on 协议桥接器 connected。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onBridgeConnected() {
    /**
     * @brief  首次登录与重连走同一套：都用登录时保存的账号密码。 重连不经过登录页，所以必须把账号密码记在内存里。
     */
    m_pBridge->login(m_strPendingName, m_strPendingPassword, 0, QString());
}

/**
 * @brief      登录成功后切换到主页并初始化会话上下文。
 * @param[in] lUserId        用户编号；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onLoginSucceeded(qint64 lUserId, const QString& strName, int nAge,
                                  const QString& strDesc) {
    business::CSessionContext& stCtx = business::CSessionContext::instance();
    stCtx.setProfile(lUserId, strName, nAge, strDesc);

    m_pLoginPage->setBusy(false);
    m_pHomePage->setProfile(stCtx.userId(), stCtx.name(), stCtx.age(), stCtx.desc());
    m_pHomePage->setServerText(
        QStringLiteral("已连接 %1:%2").arg(m_pConnection->host()).arg(m_pConnection->port()));

    if (m_pConnection->isReconnecting()) {
        if (!m_strPendingRoom.isEmpty()) {
            /**
             * @brief  回到断线前所在的房间。
             */
            m_pBridge->joinRoom(m_strPendingRoom);
        } else {
            m_pConnection->markRestored();
        }
        return;
    }

    m_pHomePage->clearError();
    m_pStack->setCurrentWidget(m_pHomePage);
    m_pHomePage->focusRoomInput();

    DP_LOG_INFO(kTag, QStringLiteral("login ok, userId=%1").arg(lUserId));
    showBanner(QStringLiteral("登录成功，账号 ID %1").arg(lUserId), false);
}

/**
 * @brief  ---------------------------------------------------------------- 房间。
 * @details 加入 on 房间。
 */
void CMainWindow::onJoinRoom(const QString& strRoom) {
    m_strPendingRoom = strRoom;
    m_pHomePage->clearError();
    m_pHomePage->setBusy(true);
    m_pBridge->joinRoom(strRoom);
}

/**
 * @brief      执行 on 房间 joined。
 * @param[in] strRoom        房间号；类型 const QString&。
 * @param[in] nCreated       输入数据；类型 bool。
 * @param[in] aMembers       初始成员列表；类型 const QVector<MemberInfo_S>&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onRoomJoined(const QString& strRoom, bool nCreated,
                              const QVector<MemberInfo_S>& aMembers) {
    business::CSessionContext& stCtx = business::CSessionContext::instance();
    m_pHomePage->setBusy(false);

    if (m_pConnection->isReconnecting()) {
        m_pChatPage->resumeRoom(strRoom, aMembers, stCtx.userId(), stCtx.name());
        m_pStack->setCurrentWidget(m_pChatPage);
        m_pConnection->markRestored();
        return;
    }

    m_pChatPage->enterRoom(strRoom, nCreated, aMembers, stCtx.userId(), stCtx.name());
    m_pStack->setCurrentWidget(m_pChatPage);

    /**
     * @brief  服务端的历史之外，再用本地缓存补齐（按 seq 去重，不会重复）。
     */
    if (m_pStore != nullptr && m_pStore->isOpen()) {
        m_pStore->prune(kTransportName, strRoom, 500);
        m_pChatPage->mergeHistory(m_pStore->loadRecent(kTransportName, strRoom, 200));
    }

    showBanner(nCreated ? QStringLiteral("房间 %1 不存在，已自动创建").arg(strRoom)
                       : QStringLiteral("已加入房间 %1").arg(strRoom),
               false);
}

/**
 * @brief      退出 on 房间。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief  ---------------------------------------------------------------- 消息。
 * @details 发送 on 消息内容。
 */
void CMainWindow::onSendMessage(const QString& strText) {
    m_pBridge->sendChat(strText);
}

/**
 * @brief      执行 on 历史消息 received。
 * @param[in] strRoom        房间号；类型 const QString&。
 * @param[in] aMessages      消息记录；类型 const QVector<ChatMessage_S>&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onHistoryReceived(const QString& strRoom, const QVector<ChatMessage_S>& aMessages) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (!strRoom.isEmpty() && strRoom != m_pChatPage->room()) return;
    m_pChatPage->mergeHistory(aMessages);
}

/**
 * @brief      执行 on 语音消息 received。
 * @param[in] lSeq           全局消息序号；类型 qint64。
 * @param[in] lFrom          输入数据；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] strRoom        房间号；类型 const QString&。
 * @param[in] byAudio        Opus 编码的音频数据；类型 const QByteArray&。
 * @param[in] nDurationMs    语音时长，单位毫秒；类型 int。
 * @param[in] lTs            Unix 毫秒时间戳；类型 qint64。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onVoiceReceived(qint64 lSeq, qint64 lFrom, const QString& strName,
                                 const QString& strRoom, const QByteArray& byAudio,
                                 int nDurationMs, qint64 lTs) {
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (strRoom != m_pChatPage->room()) return;

    m_pChatPage->appendVoice(lSeq, lFrom, strName, byAudio, nDurationMs, lTs);
}

/**
 * @brief      收到聊天消息时更新数据模型并刷新消息视图。
 * @param[in] lSeq           全局消息序号；类型 qint64。
 * @param[in] lFrom          输入数据；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] strRoom        房间号；类型 const QString&。
 * @param[in] strText        消息文本；类型 const QString&。
 * @param[in] lTs            Unix 毫秒时间戳；类型 qint64。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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
        m_pStore->save(kTransportName, strRoom, strMessage);
    }

    m_pChatPage->appendChat(lSeq, lFrom, strName, strText, lTs);
}

/**
 * @brief      执行 on 系统提示词消息内容。
 * @param[in] pEvent         事件；类型 const QString&。
 * @param[in] strRoom        房间号；类型 const QString&。
 * @param[in] lUserId        用户编号；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] strOldName     输入数据；类型 const QString&。
 * @param[in] strText        消息文本；类型 const QString&。
 * @param[in] lTs            Unix 毫秒时间戳；类型 qint64。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onSystemMessage(const QString& pEvent, const QString& strRoom, qint64 lUserId,
                                 const QString& strName, const QString& strOldName,
                                 const QString& strText, qint64 lTs) {
    if (pEvent == QStringLiteral("welcome")) return;
    if (m_pStack->currentWidget() != m_pChatPage) return;
    if (!strRoom.isEmpty() && strRoom != m_pChatPage->room()) return;
    m_pChatPage->onSystemEvent(pEvent, lUserId, strName, strOldName, strText, lTs);
}

/**
 * @brief  ---------------------------------------------------------------- 资料。
 * @details 执行 onedit 个人资料；实现上连接 Qt 信号与槽。
 */
void CMainWindow::onEditProfile() {
    business::CSessionContext& stCtx = business::CSessionContext::instance();
    auto* pDialog = new CProfileDialog(stCtx.name(), stCtx.age(), stCtx.desc(), this);
    pDialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(pDialog, &CProfileDialog::saved, this,
            [this](const QString& strName, int nAge, const QString& strDesc) {
                m_pBridge->updateProfile(strName, nAge, strDesc);
            });

    pDialog->exec();
}

/**
 * @brief      执行 on 个人资料 updated。
 * @param[in] lUserId        用户编号；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onProfileUpdated(qint64 lUserId, const QString& strName, int nAge,
                                  const QString& strDesc) {
    Q_UNUSED(lUserId)
    business::CSessionContext& stCtx = business::CSessionContext::instance();
    stCtx.setProfile(stCtx.userId(), strName, nAge, strDesc);
    m_pHomePage->setProfile(stCtx.userId(), stCtx.name(), stCtx.age(), stCtx.desc());
    showBanner(QStringLiteral("个人信息已更新"), false);
}

/**
 * @brief  ---------------------------------------------------------------- 断线重连。
 * @details 连接断开时清理会话状态并提示用户重新连接。
 */
void CMainWindow::onDisconnected() {
    if (m_nSuppressDisconnectNotice) {
        m_nSuppressDisconnectNotice = false;
        return;
    }
    if (m_pStack->currentWidget() == m_pLoginPage) return;

    m_pConnection->notifyDisconnected(QStringLiteral("连接已断开"));
}

/**
 * @brief      重新连接 onscheduled。
 * @param[in] nAttempt       输入数据；类型 int。
 * @param[in] nMaxAttempts   输入数据；类型 int。
 * @param[in] nDelayMs       输入数据；类型 int。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onReconnectScheduled(int nAttempt, int nMaxAttempts, int nDelayMs) {
    m_pChatPage->setOnline(false);
    showBanner(QStringLiteral("连接已断开，%1 秒后第 %2/%3 次重连…")
                   .arg(nDelayMs / 1000)
                   .arg(nAttempt)
                   .arg(nMaxAttempts),
               true);
}

/**
 * @brief      重新连接 onfailed。
 * @param[in] strReason      失败原因描述；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onReconnectFailed(const QString& strReason) {
    resetToLogin(QStringLiteral("重连失败（%1），请检查服务端后重新登录").arg(strReason));
}

/**
 * @brief      历史消息恢复完成后重建消息列表并滚动到底部。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onRestored() {
    m_pChatPage->setOnline(true);
    showBanner(QStringLiteral("已重新连接"), false);
}

/**
 * @brief  ---------------------------------------------------------------- 错误。
 * @details 执行 on 服务端错误。
 */
void CMainWindow::onServerError(const QString& strCode, const QString& strText) {
    Q_UNUSED(strCode)

    if (m_pConnection->isReconnecting()) {
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

/**
 * @brief      执行 onnetwork 错误。
 * @param[in] strMessage     消息内容；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onNetworkError(const QString& strMessage) {
    if (m_pStack->currentWidget() == m_pLoginPage) {
        m_pLoginPage->setBusy(false);
        m_pLoginPage->showError(strMessage);
        m_pConnection->setAutoReconnect(false);
        return;
    }

    /**
     * @brief  已登录状态下出现任何网络错误（RST、超时、断网）都走重连，而不是踢回登录页。
     */
    const business::CSessionContext& stCtx = business::CSessionContext::instance();
    if (m_pConnection->isReconnecting() || stCtx.userId() != 0) {
        m_pConnection->notifyDisconnected(strMessage);
        return;
    }

    showBanner(strMessage, true);
    resetToLogin(strMessage);
}

/**
 * @brief      重置 tologin。
 * @param[in] strReason      失败原因描述；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::resetToLogin(const QString& strReason) {
    /**
     * @brief  清理过程中会触发 disconnect，避免被当成「意外断线」再走一次重连。
     */
    m_pConnection->setAutoReconnect(false);

    m_pChatPage->clearRoom();
    m_pHomePage->setBusy(false);
    m_pHomePage->clearError();
    m_pLoginPage->setBusy(false);
    m_pStack->setCurrentWidget(m_pLoginPage);

    business::CSessionContext::instance().clearProfile();
    m_strPendingRoom.clear();

    if (!strReason.isEmpty()) {
        m_pLoginPage->showError(strReason);
    }

    m_pConnection->setAutoReconnect(true);
}

/**
 * @brief      退出登录 on。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::onLogout() {
    m_nSuppressDisconnectNotice = true;
    m_pConnection->disconnectFrom();
    resetToLogin(QString());
}

/**
 * @brief  ---------------------------------------------------------------- 顶部提示条。
 * @details 显示提示条。
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

    /**
     * @brief  错误信息留久一点，免得用户没看清就消失。
     */
    m_pBannerTimer->start(nError ? 6000 : 3200);
}

/**
 * @brief      执行 position 提示条。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CMainWindow::positionBanner() {
    if (m_pBanner == nullptr || !m_pBanner->isVisible()) return;
    m_pBanner->adjustSize();
    const int nX = (width() - m_pBanner->width()) / 2;
    m_pBanner->move(qMax(0, nX), 76);
}
