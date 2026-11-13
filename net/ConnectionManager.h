/**
 * @file  ConnectionManager.h
 * @brief 连接生命周期与断线重连调度。
 * @details 把「记住服务器地址、按固定间隔重试、次数用尽后放弃」这套逻辑
 *          从 MainWindow 里抽出来。MainWindow 只负责界面反应，不再自己
 *          维护重试计数与定时器。
 *
 *          它**不决定**重连成功后要不要回原房间——那属于业务编排，
 *          由外部监听 `reconnected` 信号后自行处理，处理完调用 `markRestored()`。
 */

#pragma once

#include <QObject>
#include <QString>

class QTimer;
class ChatTransport;

namespace net {

/**
 * @brief 连接管理器。
 *
 * @details 状态机：
 *          `Idle` → connectTo() → `Connecting` → 连接成功 → `Online`
 *          断线 → `Reconnecting`（按间隔重试）→ 成功回到 `Online`，
 *          或重试次数用尽 → 发出 `reconnectFailed` 并回到 `Idle`。
 */
class ConnectionManager : public QObject {
    Q_OBJECT

public:
    /// @brief 默认最大重连次数。
    static constexpr int kMaxAttempts = 6;
    /// @brief 默认重连间隔（毫秒）。
    static constexpr int kIntervalMs = 3000;

    /**
     * @brief 构造管理器。
     * @param transport 传输层实例，不取得所有权。
     * @param parent    Qt 父对象。
     */
    explicit ConnectionManager(ChatTransport* transport, QObject* parent = nullptr);

    /**
     * @brief 主动连接服务器并记录地址（供重连复用）。
     * @param host 主机名或 IP。
     * @param port 端口。
     */
    void connectTo(const QString& host, quint16 port);

    /// @brief 主动断开，并停止重连调度。
    void disconnectFrom();

    /**
     * @brief 开关自动重连。
     * @param on false 时断线不再重试（用于主动登出）。
     */
    void setAutoReconnect(bool on) { autoReconnect_ = on; }

    /// @brief 是否正处于重连流程中。
    bool isReconnecting() const { return reconnecting_; }

    /// @brief 已尝试的重连次数（0 表示未在重连）。
    int attempt() const { return attempt_; }

    /// @brief 最近一次连接使用的主机名。
    QString host() const { return host_; }

    /// @brief 最近一次连接使用的端口。
    quint16 port() const { return port_; }

    /**
     * @brief 通知管理器「业务已恢复完毕」。
     * @details 外部在重连后完成重新登录与回房时调用，用于复位重试计数
     *          并发出 `restored` 信号。
     */
    void markRestored();

    /**
     * @brief 由外部转告「连接已断开」。
     * @param reason 断线原因，用于提示与日志。
     * @details 之所以不由本类直接监听 transport 的 disconnected 信号，
     *          是因为「要不要重连」取决于当前处于哪个页面（登录页断线无需重连），
     *          那是界面层的判断。
     */
    void notifyDisconnected(const QString& reason);

signals:
    /**
     * @brief 已安排一次重连。
     * @param attempt     本次是第几次（从 1 开始）。
     * @param maxAttempts 上限。
     * @param delayMs     距下次尝试的等待时间。
     */
    void reconnectScheduled(int attempt, int maxAttempts, int delayMs);

    /// @brief 重试次数用尽，放弃重连。
    void reconnectFailed(const QString& reason);

    /// @brief socket 已重新连上，外部应重新登录并恢复业务状态。
    void reconnected();

    /// @brief 业务恢复完成。
    void restored();

private slots:
    void onTimerFired();
    void onTransportConnected();

private:
    void scheduleRetry();
    void resetState();

    ChatTransport* transport_ = nullptr;  ///< 传输层（不持有）
    QTimer*        timer_ = nullptr;      ///< 重连定时器

    QString host_;                        ///< 记住的主机名
    quint16 port_ = 0;                    ///< 记住的端口
    int     attempt_ = 0;                 ///< 已尝试次数
    bool    reconnecting_ = false;        ///< 是否处于重连流程
    bool    autoReconnect_ = true;        ///< 是否自动重连
};

}  // namespace net
