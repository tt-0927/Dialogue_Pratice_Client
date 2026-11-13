/**
 * @file  ConnectionManager.cpp
 * @brief 连接管理器的实现。
 */

#include "net/ConnectionManager.h"

#include "core/Logger.h"
#include "net/ChatTransport.h"

#include <QTimer>

namespace net {
namespace {
const QString kTag = QStringLiteral("net");
}  // namespace

ConnectionManager::ConnectionManager(ChatTransport* transport, QObject* parent)
    : QObject(parent), transport_(transport) {
    timer_ = new QTimer(this);
    timer_->setSingleShot(true);
    connect(timer_, &QTimer::timeout, this, &ConnectionManager::onTimerFired);

    if (transport_ != nullptr) {
        connect(transport_, &ChatTransport::connected, this,
                &ConnectionManager::onTransportConnected);
    }
}

void ConnectionManager::connectTo(const QString& host, quint16 port) {
    host_ = host;
    port_ = port;
    resetState();

    if (transport_ == nullptr) return;
    DP_LOG_INFO(kTag, QStringLiteral("connecting to %1:%2").arg(host).arg(port));
    transport_->connectToServer(host, port);
}

void ConnectionManager::disconnectFrom() {
    resetState();
    if (transport_ != nullptr) transport_->disconnectFromServer();
}

void ConnectionManager::notifyDisconnected(const QString& reason) {
    if (!autoReconnect_) {
        DP_LOG_INFO(kTag, QStringLiteral("disconnected (auto reconnect off): %1").arg(reason));
        resetState();
        return;
    }
    if (reconnecting_) return;   // socket error 可能先于 disconnected 到达

    DP_LOG_WARN(kTag, QStringLiteral("disconnected: %1").arg(reason));
    scheduleRetry();
}

void ConnectionManager::scheduleRetry() {
    if (attempt_ >= kMaxAttempts) {
        reconnecting_ = false;
        DP_LOG_ERROR(kTag, QStringLiteral("reconnect gave up after %1 attempts").arg(attempt_));
        emit reconnectFailed(QStringLiteral("已重试 %1 次仍未成功").arg(attempt_));
        resetState();
        return;
    }

    reconnecting_ = true;
    ++attempt_;

    emit reconnectScheduled(attempt_, kMaxAttempts, kIntervalMs);
    timer_->start(kIntervalMs);
}

void ConnectionManager::onTimerFired() {
    if (transport_ == nullptr || host_.isEmpty()) {
        emit reconnectFailed(QStringLiteral("缺少服务器地址"));
        resetState();
        return;
    }

    DP_LOG_INFO(kTag, QStringLiteral("reconnect attempt %1/%2").arg(attempt_).arg(kMaxAttempts));
    transport_->connectToServer(host_, port_);
}

void ConnectionManager::onTransportConnected() {
    if (!reconnecting_) return;

    // 重连成功：停掉定时器，把「重新登录 + 回房」交给外部处理。
    timer_->stop();
    DP_LOG_INFO(kTag, QStringLiteral("reconnected after %1 attempt(s)").arg(attempt_));
    emit reconnected();
}

void ConnectionManager::markRestored() {
    const bool wasReconnecting = reconnecting_;
    resetState();

    if (wasReconnecting) {
        DP_LOG_INFO(kTag, QStringLiteral("session restored"));
        emit restored();
    }
}

void ConnectionManager::resetState() {
    timer_->stop();
    reconnecting_ = false;
    attempt_ = 0;
}

}  // namespace net
