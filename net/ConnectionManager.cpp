/**
 * @file       ConnectionManager.cpp
 * @brief      连接管理器的实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

#include "net/ConnectionManager.h"

#include "core/Logger.h"
#include "net/ChatTransport.h"

#include <QTimer>

namespace net {
namespace {
const QString kTag = QStringLiteral("net");
/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 ConnectionManager 对象，完成成员初始化与依赖注入。
 * @param[in] pTransport     传输层；类型 ChatTransport*。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
ConnectionManager::ConnectionManager(ChatTransport* pTransport, QObject* pParent)
    : QObject(pParent), m_pTransport(pTransport) {
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pTimer = new QTimer(this);
    m_pTimer->setSingleShot(true);
    connect(m_pTimer, &QTimer::timeout, this, &ConnectionManager::onTimerFired);

    if (m_pTransport != nullptr) {
        connect(m_pTransport, &ChatTransport::connected, this,
                &ConnectionManager::onTransportConnected);
    }
}

/**
 * @brief      建立连接 to。
 * @param[in] strHost        服务器主机地址；类型 const QString&。
 * @param[in] uPort          服务器端口；类型 quint16。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void ConnectionManager::connectTo(const QString& strHost, quint16 uPort) {
    m_strHost = strHost;
    m_uPort = uPort;
    resetState();

    if (m_pTransport == nullptr) return;
    DP_LOG_INFO(kTag, QStringLiteral("connecting to %1:%2").arg(strHost).arg(uPort));
    m_pTransport->connectToServer(strHost, uPort);
}

/**
 * @brief      断开连接 from。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void ConnectionManager::disconnectFrom() {
    resetState();
    if (m_pTransport != nullptr) m_pTransport->disconnectFromServer();
}

/**
 * @brief      通知 disconnected。
 * @param[in] strReason      失败原因描述；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void ConnectionManager::notifyDisconnected(const QString& strReason) {
    if (!m_nAutoReconnect) {
        DP_LOG_INFO(kTag, QStringLiteral("disconnected (auto reconnect off): %1").arg(strReason));
        resetState();
        return;
    }
    /**
     * @brief  socket error 可能先于 disconnected 到达。
     */
    if (m_nReconnecting) return;

    DP_LOG_WARN(kTag, QStringLiteral("disconnected: %1").arg(strReason));
    scheduleRetry();
}

/**
 * @brief      重试 schedule。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief      执行 on 定时器 fired。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief      执行 on 传输层 connected。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void ConnectionManager::onTransportConnected() {
    if (!m_nReconnecting) return;

    /**
     * @brief  重连成功：停掉定时器，把「重新登录 + 回房」交给外部处理。
     */
    m_pTimer->stop();
    DP_LOG_INFO(kTag, QStringLiteral("reconnected after %1 attempt(s)").arg(m_nAttempt));
    emit reconnected();
}

/**
 * @brief      标记指定消息为历史补回状态，避免重复触发翻译。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void ConnectionManager::markRestored() {
    const bool nWasReconnecting = m_nReconnecting;
    resetState();

    if (nWasReconnecting) {
        DP_LOG_INFO(kTag, QStringLiteral("session restored"));
        emit restored();
    }
}

/**
 * @brief      重置状态。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void ConnectionManager::resetState() {
    m_pTimer->stop();
    m_nReconnecting = false;
    m_nAttempt = 0;
}

/**
 * @brief  namespace net。
 */
}
