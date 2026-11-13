/*
 * 文件名:    ProtocolBridge.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CProtocolBridge 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CProtocolBridge.h */

/* 自研 TCP 服务端实现（局域网模式）：负责组帧/解帧、JSON 解析，转成 ChatTransport 的信号。 */
/* 界面层只依赖 ChatTransport，不接触 socket 与字节流。 */
#pragma once

#include <QObject>
#include <QString>

#include <QTcpSocket>
#include <QTimer>

#include "ChatTransport.h"
#include "common/Protocol.h"

class CProtocolBridge : public ChatTransport {
    Q_OBJECT

public:
    explicit CProtocolBridge(QObject* parent = nullptr);

    void connectToServer(const QString& host, quint16 port) override;
    void disconnectFromServer() override;
    bool isConnected() const;

    void login(const QString& name, int age, const QString& desc) override;
    void updateProfile(const QString& name, int age, const QString& desc) override;
    void joinRoom(const QString& room) override;
    void leaveRoom() override;
    void sendChat(const QString& text) override;
    void sendVoice(const QByteArray& audio, int durationMs) override;

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();
    void onSocketError(QAbstractSocket::SocketError error);

private:
    void handleFrame(const dp::Frame_S& frame);
    void sendJson(dp::OpCode_EN op, const dp::Json& payload);

    QTcpSocket*     m_pSocket = nullptr;
    QTimer*         m_pPingTimer = nullptr;
    QTimer*         m_pConnectTimer = nullptr;
    dp::CFrameDecoder m_stDecoder;
    bool            m_nConnecting = false;
};
