// ProtocolBridge.h
//
// 自研 TCP 服务端实现（局域网模式）：负责组帧/解帧、JSON 解析，转成 ChatTransport 的信号。
// 界面层只依赖 ChatTransport，不接触 socket 与字节流。
#pragma once

#include <QObject>
#include <QString>

#include <QTcpSocket>
#include <QTimer>

#include "ChatTransport.h"
#include "common/Protocol.h"

class ProtocolBridge : public ChatTransport {
    Q_OBJECT

public:
    explicit ProtocolBridge(QObject* parent = nullptr);

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
    void handleFrame(const dp::Frame& frame);
    void sendJson(dp::OpCode op, const dp::Json& payload);

    QTcpSocket*     socket_ = nullptr;
    QTimer*         pingTimer_ = nullptr;
    QTimer*         connectTimer_ = nullptr;
    dp::FrameDecoder decoder_;
    bool            connecting_ = false;
};
