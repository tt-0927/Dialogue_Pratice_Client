/**
 * @file       ConnectionManager.h
 * @brief      连接生命周期与断线重连调度。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

#pragma once

#include <QObject>
#include <QString>

class QTimer;
class ChatTransport;

namespace net {

/**
 * @brief   连接管理器。
 * @details 状态机： `Idle` → connectTo() → `Connecting` → 连接成功 → `Online` 断线 → `Reconnecting`（按间隔重试）→ 成功回到 `Online`， 或重试次数用尽 → 发出 `reconnectFailed` 并回到 `Idle`。
 * @author  JaTin
 */
class ConnectionManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief  默认最大重连次数。
     * @note   取值 6。
     */
    static constexpr int kMaxAttempts = 6;
    /**
     * @brief  默认重连间隔（毫秒）。
     * @note   取值 3000。
     */
    static constexpr int kIntervalMs = 3000;

    /**
     * @brief      构造管理器。
     * @param[in] transport      传输层实例，不取得所有权；类型 ChatTransport*。
     * @param[in] parent         Qt 父对象；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit ConnectionManager(ChatTransport* transport, QObject* parent = nullptr);

    /**
     * @brief      主动连接服务器并记录地址（供重连复用）。
     * @param[in] host           主机名或 IP；类型 const QString&。
     * @param[in] port           端口；类型 quint16。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void connectTo(const QString& host, quint16 port);

    /**
     * @brief      主动断开，并停止重连调度。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void disconnectFrom();

    /**
     * @brief 开关自动重连。
     * @param on false 时断线不再重试（用于主动登出）。
     */
    void setAutoReconnect(bool on) { m_nAutoReconnect = on; }

    /**
     * @brief  是否正处于重连流程中。
     */
    bool isReconnecting() const { return m_nReconnecting; }

    /**
     * @brief  已尝试的重连次数（0 表示未在重连）。
     */
    int attempt() const { return m_nAttempt; }

    /**
     * @brief  最近一次连接使用的主机名。
     */
    QString host() const { return m_strHost; }

    /**
     * @brief  最近一次连接使用的端口。
     */
    quint16 port() const { return m_uPort; }

    /**
     * @brief      通知管理器「业务已恢复完毕」。
     * @details    外部在重连后完成重新登录与回房时调用，用于复位重试计数 并发出 `restored` 信号。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void markRestored();

    /**
     * @brief      由外部转告「连接已断开」。
     * @details    之所以不由本类直接监听 transport 的 disconnected 信号， 是因为「要不要重连」取决于当前处于哪个页面（登录页断线无需重连）， 那是界面层的判断。
     * @param[in] reason         断线原因，用于提示与日志；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void notifyDisconnected(const QString& reason);

signals:
    /**
     * @brief      已安排一次重连。
     * @param[in] attempt        本次是第几次（从 1 开始）；类型 int。
     * @param[in] maxAttempts    上限；类型 int。
     * @param[in] delayMs        距下次尝试的等待时间；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void reconnectScheduled(int attempt, int maxAttempts, int delayMs);

    /**
     * @brief      重试次数用尽，放弃重连。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void reconnectFailed(const QString& reason);

    /**
     * @brief      socket 已重新连上，外部应重新登录并恢复业务状态。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void reconnected();

    /**
     * @brief      业务恢复完成。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void restored();

private slots:
    /**
     * @brief      执行 on 定时器 fired。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onTimerFired();
    /**
     * @brief      执行 on 传输层 connected。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onTransportConnected();

private:
    /**
     * @brief      重试 schedule。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void scheduleRetry();
    /**
     * @brief      重置状态。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resetState();

    /**
     * @brief  传输层（不持有）
     * @note   类型 ChatTransport*；类/结构体成员变量。
     */
    ChatTransport* m_pTransport = nullptr;
    /**
     * @brief  重连定时器
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*        m_pTimer = nullptr;

    /**
     * @brief  记住的主机名
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strHost;
    /**
     * @brief  记住的端口
     * @note   类型 quint16；类/结构体成员变量。
     */
    quint16 m_uPort = 0;
    /**
     * @brief  已尝试次数
     * @note   类型 int；类/结构体成员变量。
     */
    int     m_nAttempt = 0;
    /**
     * @brief  是否处于重连流程
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    m_nReconnecting = false;
    /**
     * @brief  是否自动重连
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    m_nAutoReconnect = true;
};

/**
 * @brief  namespace net。
 */
}
