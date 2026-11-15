/**
 * @file       MainWindow.h
 * @brief      应用主窗口：三页调度（登录 → 主页 → 聊天）与连接状态提示。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释；
 *             2026-11-15 JaTin 增加头像透传与历史房间刷新。
 */

#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include "business/ChatTypes.h"

class ChatPage;
class ChatTransport;
class CHomePage;
class CLoginPage;
class CMessageStore;
class QLabel;
class QStackedWidget;
class QTimer;

namespace net {
class ConnectionManager;
/**
 * @brief  namespace net。
 */
}

/**
 * @brief   应用主窗口。
 * @details 顶栏用一条悬浮的 Banner 显示提示（登录成功、断线重连、错误）。 Banner 单独放在中央控件之上，不占布局高度，避免页面切换时跳动。
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
     * @brief      窗口尺寸变化时重新居中 Banner。
     * @param[in] event          事件；类型 QResizeEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resizeEvent(QResizeEvent* event) override;

private slots:
    /**
     * @brief 用户在登录页提交。
     * @param host 服务器地址。
     * @param port 端口。
     * @param name 昵称。
     */
    void onLoginRequested(const QString& host, quint16 port, const QString& name,
                          const QString& password);

    /**
     * @brief      socket 已连上，发出登录请求。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onBridgeConnected();

    /**
     * @brief      登录成功。
     * @param[in] userId         用户编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @param[in] avatar         头像图片二进制，空表示无自定义头像；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onLoginSucceeded(qint64 userId, const QString& name, int age, const QString& desc,
                          const QByteArray& avatar);

    /**
     * @brief      资料更新成功。
     * @param[in] userId         用户编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @param[in] avatar         头像图片二进制，空表示无自定义头像；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onProfileUpdated(qint64 userId, const QString& name, int age, const QString& desc,
                          const QByteArray& avatar);

    /**
     * @brief      收到历史房间列表。
     * @param[in] rooms          历史房间，按最近访问时间倒序；类型 const QVector<VisitedRoom_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onHistoryRoomsReceived(const QVector<VisitedRoom_S>& rooms);

    /**
     * @brief      进房成功。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] created        输入参数；类型 bool。
     * @param[in] members        初始成员列表；类型 const QVector<MemberInfo_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onRoomJoined(const QString& room, bool created, const QVector<MemberInfo_S>& members);

    /**
     * @brief      收到历史消息。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] messages       消息记录；类型 const QVector<ChatMessage_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onHistoryReceived(const QString& room, const QVector<ChatMessage_S>& messages);

    /**
     * @brief  收到文本消息。
     */
    void onChatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                        const QString& text, qint64 ts);

    /**
     * @brief  收到语音消息。
     */
    void onVoiceReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                         const QByteArray& audio, int durationMs, qint64 ts);

    /**
     * @brief  收到系统事件。
     */
    void onSystemMessage(const QString& event, const QString& room, qint64 userId,
                         const QString& name, const QString& oldName,
                         const QString& text, qint64 ts);

    /**
     * @brief      服务端返回业务错误。
     * @param[in] code           输入参数；类型 const QString&。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onServerError(const QString& code, const QString& text);

    /**
     * @brief      网络层报告错误。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onNetworkError(const QString& message);

    /**
     * @brief      socket 断开。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onDisconnected();

    /**
     * @brief      连接管理器安排了一次重连。
     * @param[in] attempt        第几次；类型 int。
     * @param[in] maxAttempts    上限；类型 int。
     * @param[in] delayMs        等待时间；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onReconnectScheduled(int attempt, int maxAttempts, int delayMs);

    /**
     * @brief      重连次数用尽。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onReconnectFailed(const QString& reason);

    /**
     * @brief      业务状态已恢复（重新登录 + 回房完成）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onRestored();

    /**
     * @brief      打开资料编辑对话框。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onEditProfile();

    /**
     * @brief      请求加入房间。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onJoinRoom(const QString& room);

    /**
     * @brief      发送文本消息。
     * @param[in] text           正文；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onSendMessage(const QString& text);

    /**
     * @brief      返回主页并离开房间。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onLeaveRoom();

    /**
     * @brief      退出登录。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onLogout();

private:
    /**
     * @brief      显示顶部提示条。
     * @param[in] text           提示文本；类型 const QString&。
     * @param[in] error          true 表示错误样式（显示更久）；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showBanner(const QString& text, bool error);

    /**
     * @brief      把提示条居中放到顶部。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void positionBanner();

    /**
     * @brief      回到登录页并清理会话状态。
     * @param[in] reason         显示在登录页的错误原因，空串表示不显示；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resetToLogin(const QString& reason);

    /**
     * @brief  传输层
     * @note   类型 ChatTransport*；类/结构体成员变量。
     */
    ChatTransport*          m_pBridge = nullptr;
    /**
     * @brief  连接与重连管理
     * @note   类型 net::ConnectionManager*；类/结构体成员变量。
     */
    net::ConnectionManager* m_pConnection = nullptr;
    /**
     * @brief  本地历史缓存
     * @note   类型 CMessageStore*；类/结构体成员变量。
     */
    CMessageStore*           m_pStore = nullptr;

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
    QLabel* m_pBanner = nullptr;
    /**
     * @brief  提示条定时器。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer* m_pBannerTimer = nullptr;

    /**
     * @brief  本次登录用的账号名（重连时复用）
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingName;
    /**
     * @brief  本次登录用的密码（仅内存，重连时复用）
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingPassword;
    /**
     * @brief  断线前所在房间，重连后回房用
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingRoom;

    /**
     * @brief  主动断开时不提示「连接已断开」
     * @note   类型 bool；类/结构体成员变量。
     */
    bool m_nSuppressDisconnectNotice = false;
};
