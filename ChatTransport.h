/*
 * 文件名:    ChatTransport.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  ChatTransport 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* ChatTransport.h */

/* 消息传输层抽象。目前只有一种实现： */
/* CProtocolBridge —— 自研 TCP 服务端（局域网） */
/* 界面只依赖这个接口，以后换 WebSocket / MQTT 之类只需再写一个实现。 */
#pragma once

#include <QObject>
#include <QString>

#include "ChatTypes.h"

class ChatTransport : public QObject {
    Q_OBJECT

public:
    explicit ChatTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ChatTransport() override = default;

    /* 建立连接。RTM 没有"服务器地址"的概念，实现里可以忽略参数直接成功。 */
    virtual void connectToServer(const QString& host, quint16 port) = 0;
    virtual void disconnectFromServer() = 0;

    virtual void login(const QString& name, int age, const QString& desc) = 0;
    virtual void updateProfile(const QString& name, int age, const QString& desc) = 0;

    virtual void joinRoom(const QString& room) = 0;
    virtual void leaveRoom() = 0;
    virtual void sendChat(const QString& text) = 0;

    /* 发送语音消息。audio 是 CVoiceCodec 产出的 DPV1 容器（已压缩）。 */
    virtual void sendVoice(const QByteArray& audio, int durationMs) = 0;

signals:
    void connected();
    void disconnected();

    void loginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);
    void profileUpdated(qint64 userId, const QString& name, int age, const QString& desc);

    void roomJoined(const QString& room, bool created, const QVector<MemberInfo_S>& members);
    void historyReceived(const QString& room, const QVector<ChatMessage_S>& messages);

    void chatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                      const QString& text, qint64 ts);

    /* 收到语音消息（含自己发的那条回显）。 */
    void voiceReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                       const QByteArray& audio, int durationMs, qint64 ts);

    /* event: join / leave / left / welcome / rename */
    void systemMessage(const QString& event, const QString& room, qint64 userId,
                       const QString& name, const QString& oldName,
                       const QString& text, qint64 ts);

    void serverError(const QString& code, const QString& text);
    void networkError(const QString& message);
};
