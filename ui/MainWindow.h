/**
 * @file  MainWindow.h
 * @brief 应用主窗口：三页调度（登录 → 主页 → 聊天）与连接状态提示。
 * @details 重构后本类只做「页面调度 + 界面反应」：
 *          - 连接与重连由 `net::ConnectionManager` 负责
 *          - 本人身份与房间状态由 `business::SessionContext` 统一持有
 *          - 消息与翻译由 `ChatPage` 内部的 Model / Controller 处理
 */

#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include "business/ChatTypes.h"

class ChatPage;
class ChatTransport;
class HomePage;
class LoginPage;
class MessageStore;
class QLabel;
class QStackedWidget;
class QTimer;

namespace net {
class ConnectionManager;
}  // namespace net

/**
 * @brief 应用主窗口。
 *
 * @details 顶栏用一条悬浮的 Banner 显示提示（登录成功、断线重连、错误）。
 *          Banner 单独放在中央控件之上，不占布局高度，避免页面切换时跳动。
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    /// @brief 窗口尺寸变化时重新居中 Banner。
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

    /// @brief socket 已连上，发出登录请求。
    void onBridgeConnected();

    /// @brief 登录成功。
    void onLoginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);

    /// @brief 资料更新成功。
    void onProfileUpdated(qint64 userId, const QString& name, int age, const QString& desc);

    /// @brief 进房成功。
    void onRoomJoined(const QString& room, bool created, const QVector<MemberInfo>& members);

    /// @brief 收到历史消息。
    void onHistoryReceived(const QString& room, const QVector<ChatMessage>& messages);

    /// @brief 收到文本消息。
    void onChatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                        const QString& text, qint64 ts);

    /// @brief 收到语音消息。
    void onVoiceReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                         const QByteArray& audio, int durationMs, qint64 ts);

    /// @brief 收到系统事件。
    void onSystemMessage(const QString& event, const QString& room, qint64 userId,
                         const QString& name, const QString& oldName,
                         const QString& text, qint64 ts);

    /// @brief 服务端返回业务错误。
    void onServerError(const QString& code, const QString& text);

    /// @brief 网络层报告错误。
    void onNetworkError(const QString& message);

    /// @brief socket 断开。
    void onDisconnected();

    /**
     * @brief 连接管理器安排了一次重连。
     * @param attempt     第几次。
     * @param maxAttempts 上限。
     * @param delayMs     等待时间。
     */
    void onReconnectScheduled(int attempt, int maxAttempts, int delayMs);

    /// @brief 重连次数用尽。
    void onReconnectFailed(const QString& reason);

    /// @brief 业务状态已恢复（重新登录 + 回房完成）。
    void onRestored();

    /// @brief 打开资料编辑对话框。
    void onEditProfile();

    /**
     * @brief 请求加入房间。
     * @param room 房间号。
     */
    void onJoinRoom(const QString& room);

    /**
     * @brief 发送文本消息。
     * @param text 正文。
     */
    void onSendMessage(const QString& text);

    /// @brief 返回主页并离开房间。
    void onLeaveRoom();

    /// @brief 退出登录。
    void onLogout();

private:
    /**
     * @brief 显示顶部提示条。
     * @param text  提示文本。
     * @param error true 表示错误样式（显示更久）。
     */
    void showBanner(const QString& text, bool error);

    /// @brief 把提示条居中放到顶部。
    void positionBanner();

    /**
     * @brief 回到登录页并清理会话状态。
     * @param reason 显示在登录页的错误原因，空串表示不显示。
     */
    void resetToLogin(const QString& reason);

    ChatTransport*          bridge_ = nullptr;      ///< 传输层
    net::ConnectionManager* connection_ = nullptr;  ///< 连接与重连管理
    MessageStore*           store_ = nullptr;       ///< 本地历史缓存

    QStackedWidget* stack_ = nullptr;
    LoginPage*      loginPage_ = nullptr;
    HomePage*       homePage_ = nullptr;
    ChatPage*       chatPage_ = nullptr;

    QLabel* banner_ = nullptr;
    QTimer* bannerTimer_ = nullptr;

    QString pendingName_;     ///< 本次登录用的账号名（重连时复用）
    QString pendingPassword_; ///< 本次登录用的密码（仅内存，重连时复用）
    QString pendingRoom_;   ///< 断线前所在房间，重连后回房用

    bool suppressDisconnectNotice_ = false;  ///< 主动断开时不提示「连接已断开」
};
