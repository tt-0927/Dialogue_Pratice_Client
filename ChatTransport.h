/**
 * @file       ChatTransport.h
 * @brief      ChatTransport 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  ChatTransport.h。
 */

/**
 * @brief  消息传输层抽象。目前只有一种实现： CProtocolBridge —— 自研 TCP 服务端（局域网） 界面只依赖这个接口，以后换 WebSocket / MQTT 之类只需再写一个实现。
 */
#pragma once

#include <QObject>
#include <QString>

#include "ChatTypes.h"

/**
 * @brief   Chat传输层。
 * @author  JaTin
 */
class ChatTransport : public QObject {
    Q_OBJECT

public:
    explicit ChatTransport(QObject* parent = nullptr) : QObject(parent) {}
    ~ChatTransport() override = default;

    /**
     * @brief      建立连接。RTM 没有"服务器地址"的概念，实现里可以忽略参数直接成功。
     * @param[in] host           服务器主机地址；类型 const QString&。
     * @param[in] port           服务器端口；类型 quint16。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void connectToServer(const QString& host, quint16 port) = 0;
    /**
     * @brief      断开连接 from 服务端。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void disconnectFromServer() = 0;

    /**
     * @brief      完成登录 login。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void login(const QString& name, int age, const QString& desc) = 0;
    /**
     * @brief      更新个人资料。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void updateProfile(const QString& name, int age, const QString& desc) = 0;

    /**
     * @brief      加入房间。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void joinRoom(const QString& room) = 0;
    /**
     * @brief      退出房间。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void leaveRoom() = 0;
    /**
     * @brief      发送 chat。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void sendChat(const QString& text) = 0;

    /**
     * @brief      发送语音消息。audio 是 CVoiceCodec 产出的 DPV1 容器（已压缩）。
     * @param[in] audio          Opus 编码的音频数据；类型 const QByteArray&。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    virtual void sendVoice(const QByteArray& audio, int durationMs) = 0;

signals:
    /**
     * @brief      执行 connected相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void connected();
    /**
     * @brief      执行 disconnected相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void disconnected();

    /**
     * @brief      完成登录 succeeded。
     * @param[in] userId         用户编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void loginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);
    /**
     * @brief      执行个人资料 updated。
     * @param[in] userId         用户编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void profileUpdated(qint64 userId, const QString& name, int age, const QString& desc);

    /**
     * @brief      执行房间 joined。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] created        输入参数；类型 bool。
     * @param[in] members        初始成员列表；类型 const QVector<MemberInfo_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void roomJoined(const QString& room, bool created, const QVector<MemberInfo_S>& members);
    /**
     * @brief      执行历史消息 received。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] messages       消息记录；类型 const QVector<ChatMessage_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void historyReceived(const QString& room, const QVector<ChatMessage_S>& messages);

    void chatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                      const QString& text, qint64 ts);

    /**
     * @brief  收到语音消息（含自己发的那条回显）。
     */
    void voiceReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                       const QByteArray& audio, int durationMs, qint64 ts);

    /**
     * @brief  event: join / leave / left / welcome / rename。
     */
    void systemMessage(const QString& event, const QString& room, qint64 userId,
                       const QString& name, const QString& oldName,
                       const QString& text, qint64 ts);

    /**
     * @brief      执行服务端错误。
     * @param[in] code           输入参数；类型 const QString&。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void serverError(const QString& code, const QString& text);
    /**
     * @brief      执行 network 错误。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void networkError(const QString& message);
};
