/*
 * 文件名:    ConnectionManager.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  连接管理器的实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#include "net/ConnectionManager.h"

#include "core/Logger.h"
#include "net/ChatTransport.h"

#include <QTimer>

namespace net {
namespace {
const QString kTag = QStringLiteral("net");
/* namespace */
}

ConnectionManager::ConnectionManager(ChatTransport* transport, QObject* parent)
    : QObject(parent), m_pTransport(transport) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTimer = new QTimer(this);
    m_pTimer->setSingleShot(true);
    connect(m_pTimer, &QTimer::timeout, this, &ConnectionManager::onTimerFired);

    if (m_pTransport != nullptr) {
        connect(m_pTransport, &ChatTransport::connected, this,
                &ConnectionManager::onTransportConnected);
    }
}

/*
 * 功能描述:  执行 connectTo 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  host  参数。
 * param [in]  port  参数。
 * return:    void
 */
void ConnectionManager::connectTo(const QString& host, quint16 port) {
    m_strHost = host;
    m_uPort = port;
    resetState();

    if (m_pTransport == nullptr) return;
    DP_LOG_INFO(kTag, QStringLiteral("connecting to %1:%2").arg(host).arg(port));
    m_pTransport->connectToServer(host, port);
}

/*
 * 功能描述:  执行 disconnectFrom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ConnectionManager::disconnectFrom() {
    resetState();
    if (m_pTransport != nullptr) m_pTransport->disconnectFromServer();
}

/*
 * 功能描述:  执行 notifyDisconnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  reason  参数。
 * return:    void
 */
void ConnectionManager::notifyDisconnected(const QString& reason) {
    if (!m_nAutoReconnect) {
        DP_LOG_INFO(kTag, QStringLiteral("disconnected (auto reconnect off): %1").arg(reason));
        resetState();
        return;
    }
    /* socket error 可能先于 disconnected 到达 */
    if (m_nReconnecting) return;

    DP_LOG_WARN(kTag, QStringLiteral("disconnected: %1").arg(reason));
    scheduleRetry();
}

/*
 * 功能描述:  执行 scheduleRetry 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ConnectionManager::scheduleRetry() {
    if (m_nAttempt >= kMaxAttempts) {
        m_nReconnecting = false;
        DP_LOG_ERROR(kTag, QStringLiteral("reconnect gave up after %1 attempts").arg(m_nAttempt));
        emit reconnectFailed(QStringLiteral("已重试 %1 次仍未成功").arg(m_nAttempt));
        resetState();
        return;
    }

    m_nReconnecting = true;
    ++m_nAttempt;

    emit reconnectScheduled(m_nAttempt, kMaxAttempts, kIntervalMs);
    m_pTimer->start(kIntervalMs);
}

/*
 * 功能描述:  执行 onTimerFired 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ConnectionManager::onTimerFired() {
    if (m_pTransport == nullptr || m_strHost.isEmpty()) {
        emit reconnectFailed(QStringLiteral("缺少服务器地址"));
        resetState();
        return;
    }

    DP_LOG_INFO(kTag, QStringLiteral("reconnect attempt %1/%2").arg(m_nAttempt).arg(kMaxAttempts));
    m_pTransport->connectToServer(m_strHost, m_uPort);
}

/*
 * 功能描述:  执行 onTransportConnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ConnectionManager::onTransportConnected() {
    if (!m_nReconnecting) return;

    /* 重连成功：停掉定时器，把「重新登录 + 回房」交给外部处理。 */
    m_pTimer->stop();
    DP_LOG_INFO(kTag, QStringLiteral("reconnected after %1 attempt(s)").arg(m_nAttempt));
    emit reconnected();
}

/*
 * 功能描述:  执行 markRestored 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ConnectionManager::markRestored() {
    const bool wasReconnecting = m_nReconnecting;
    resetState();

    if (wasReconnecting) {
        DP_LOG_INFO(kTag, QStringLiteral("session restored"));
        emit restored();
    }
}

/*
 * 功能描述:  执行 resetState 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ConnectionManager::resetState() {
    m_pTimer->stop();
    m_nReconnecting = false;
    m_nAttempt = 0;
}

/* namespace net */
}
