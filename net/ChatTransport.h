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

#include <QByteArray>
#include <QObject>
#include <QString>

#include "business/ChatTypes.h"

class ChatTransport : public QObject {
    Q_OBJECT

public:
    explicit ChatTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ChatTransport() override = default;

    /* 建立连接。RTM 没有"服务器地址"的概念，实现里可以忽略参数直接成功。 */
    virtual void connectToServer(const QString& host, quint16 port) = 0;
    virtual void disconnectFromServer() = 0;

    /**
     * @brief 登录。
     * @param name     账号名（或昵称，取决于服务端是否强制认证）。
     * @param password 密码；为空表示不带密码（走匿名/昵称匹配）。
     * @param age      年龄，0 表示未填。
     * @param desc     个人简介。
     */
    virtual void login(const QString& name, const QString& password, int age,
                       const QString& desc) = 0;
    virtual void updateProfile(const QString& name, int age, const QString& desc) = 0;

    /**
     * @brief 自助注册。
     * @param username 登录名（3..24 位字母/数字/下划线/短横线）。
     * @param password 密码（6..64 位）。
     * @param nickname 昵称，空则用登录名。
     * @param age      年龄，0 表示不填。
     * @param desc     简介，空表示不填。
     * @details 成功后发 `registerSucceeded`；失败走 `serverError`。
     *          注册不自动登录，调用方拿到账号名后应回填到登录表单。
     */
    virtual void registerAccount(const QString& username, const QString& password,
                                 const QString& nickname, int age, const QString& desc) = 0;

    virtual void joinRoom(const QString& room) = 0;
    virtual void leaveRoom() = 0;
    virtual void sendChat(const QString& text) = 0;

    /* 发送语音消息。audio 是 CVoiceCodec 产出的 DPV1 容器（已压缩）。 */
    virtual void sendVoice(const QByteArray& audio, int durationMs) = 0;

signals:
    void connected();
    void disconnected();

    void loginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);

    /* 服务端下发的 LLM 配置（原始 JSON 字节流）。 */
    /* 传输层不解析结构，只负责搬运；客户端用 AppConfig::parse 处理。 */
    /* 老服务端不下发该字段，此时不会发出这个信号，客户端回退读本地配置。 */
    void llmConfigReceived(const QByteArray& json);

    /* 注册成功。 */
    void registerSucceeded(qint64 userId, const QString& username, const QString& nickname);
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
