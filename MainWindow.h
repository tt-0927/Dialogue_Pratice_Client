/**
 * @file       MainWindow.h
 * @brief      CMainWindow 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CMainWindow.h。
 */

/**
 * @brief  三个页面的调度器：登录 → 主页（资料 + 进房）→ 聊天。 同时负责断线自动重连：断开后按固定间隔重试，成功后自动重新登录并回到原房间， 再由服务端下发的历史消息补齐断线期间的空档。
 */
#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include "ChatTransport.h"

class ChatPage;
class CMessageStore;
class CHomePage;
class CLoginPage;
class QLabel;
class QStackedWidget;
class QTimer;

/**
 * @brief   CMainWindow。
 * @author  JaTin
 */
class CMainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CMainWindow(QWidget* parent = nullptr);
    ~CMainWindow() override;

protected:
    /**
     * @brief      重新计算尺寸事件。
     * @param[in] event          事件；类型 QResizeEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resizeEvent(QResizeEvent* event) override;

private slots:
    /**
     * @brief      完成登录 onrequested。
     * @param[in] host           服务器主机地址；类型 const QString&。
     * @param[in] port           服务器端口；类型 quint16。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onLoginRequested(const QString& host, quint16 port, const QString& name);
    /**
     * @brief      执行 on 协议桥接器 connected。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onBridgeConnected();
    /**
     * @brief      登录成功后切换到主页并初始化会话上下文。
     * @param[in] userId         用户编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onLoginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);
    /**
     * @brief      执行 on 个人资料 updated。
     * @param[in] userId         用户编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onProfileUpdated(qint64 userId, const QString& name, int age, const QString& desc);
    /**
     * @brief      执行 on 房间 joined。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] created        输入参数；类型 bool。
     * @param[in] members        初始成员列表；类型 const QVector<MemberInfo_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onRoomJoined(const QString& room, bool created, const QVector<MemberInfo_S>& members);
    /**
     * @brief      执行 on 历史消息 received。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] messages       消息记录；类型 const QVector<ChatMessage_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onHistoryReceived(const QString& room, const QVector<ChatMessage_S>& messages);
    void onChatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                        const QString& text, qint64 ts);
    void onVoiceReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                         const QByteArray& audio, int durationMs, qint64 ts);
    void onSystemMessage(const QString& event, const QString& room, qint64 userId,
                         const QString& name, const QString& oldName,
                         const QString& text, qint64 ts);
    /**
     * @brief      执行 on 服务端错误。
     * @param[in] code           输入参数；类型 const QString&。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onServerError(const QString& code, const QString& text);
    /**
     * @brief      执行 onnetwork 错误。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onNetworkError(const QString& message);
    /**
     * @brief      连接断开时清理会话状态并提示用户重新连接。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onDisconnected();
    /**
     * @brief      重新连接 ontimeout。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onReconnectTimeout();

    /**
     * @brief      执行 onedit 个人资料。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onEditProfile();
    /**
     * @brief      加入 on 房间。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onJoinRoom(const QString& room);
    /**
     * @brief      发送 on 消息内容。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onSendMessage(const QString& text);
    /**
     * @brief      退出 on 房间。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onLeaveRoom();
    /**
     * @brief      退出登录 on。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onLogout();

private:
    /**
     * @brief  重新连接maxattempts。
     * @note   取值 6。
     */
    static constexpr int kMaxReconnectAttempts = 6;
    /**
     * @brief  重新连接intervalms。
     * @note   取值 3000。
     */
    static constexpr int kReconnectIntervalMs = 3000;

    /**
     * @brief      显示提示条。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] error          错误信息；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showBanner(const QString& text, bool error);
    /**
     * @brief      执行 position 提示条。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void positionBanner();
    /**
     * @brief      重置 tologin。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resetToLogin(const QString& reason);

    /**
     * @brief      重新连接 schedule。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void scheduleReconnect(const QString& reason);
    /**
     * @brief      重新连接 finish。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void finishReconnect();
    /**
     * @brief      重置 reconnect 状态。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resetReconnectState();

    /**
     * @brief  协议桥接器。 初值 = nullptr。
     * @note   类型 ChatTransport*；类/结构体成员变量。
     */
    ChatTransport* m_pBridge = nullptr;
    /**
     * @brief  存储。 初值 = nullptr。
     * @note   类型 CMessageStore*；类/结构体成员变量。
     */
    CMessageStore*  m_pStore = nullptr;
    /**
     * @brief  传输层名称。 初值 = QStringLiteral( )。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString        m_strTransportName = QStringLiteral("server");
    /**
     * @brief  Stack。 初值 = nullptr。
     * @note   类型 QStackedWidget*；类/结构体成员变量。
     */
    QStackedWidget* m_pStack = nullptr;
    /**
     * @brief  Login页面。 初值 = nullptr。
     * @note   类型 CLoginPage*；类/结构体成员变量。
     */
    CLoginPage*      m_pLoginPage = nullptr;
    /**
     * @brief  Home页面。 初值 = nullptr。
     * @note   类型 CHomePage*；类/结构体成员变量。
     */
    CHomePage*       m_pHomePage = nullptr;
    /**
     * @brief  Chat页面。 初值 = nullptr。
     * @note   类型 ChatPage*；类/结构体成员变量。
     */
    ChatPage*       m_pChatPage = nullptr;
    /**
     * @brief  提示条。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*         m_pBanner = nullptr;
    /**
     * @brief  Reconnect定时器。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*         m_pReconnectTimer = nullptr;
    /**
     * @brief  提示条定时器。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*         m_pBannerTimer = nullptr;

    /**
     * @brief  Last主机地址。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strLastHost;
    /**
     * @brief  Last端口。 初值 = 0。
     * @note   类型 quint16；类/结构体成员变量。
     */
    quint16 m_uLastPort = 0;

    /**
     * @brief  Pending主机地址。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingHost;
    /**
     * @brief  Pending端口。 初值 = 0。
     * @note   类型 quint16；类/结构体成员变量。
     */
    quint16 m_uPendingPort = 0;
    /**
     * @brief  Pending名称。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingName;
    /**
     * @brief  Pending房间。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingRoom;

    /**
     * @brief  本人会话编号。 初值 = 0。
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64  m_uSelfId = 0;
    /**
     * @brief  本人昵称。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strSelfName;
    /**
     * @brief  Self年龄。 初值 = 0。
     * @note   类型 int；类/结构体成员变量。
     */
    int     m_nSelfAge = 0;
    /**
     * @brief  Self个人简介。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strSelfDesc;

    /**
     * @brief  SuppressDisconnectNotice。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool m_nSuppressDisconnectNotice = false;
    /**
     * @brief  Reconnecting。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool m_nReconnecting = false;
    /**
     * @brief  ReconnectAttempt。 初值 = 0。
     * @note   类型 int；类/结构体成员变量。
     */
    int  m_nReconnectAttempt = 0;
};
