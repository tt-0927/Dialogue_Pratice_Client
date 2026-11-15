/**
 * @file       ChatTypes.h
 * @brief      ChatTypes 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释；
 *             2026-11-15 JaTin 增加历史房间条目类型。
 */

/**
 * @brief  ChatTypes.h。
 */

/**
 * @brief  传输层与界面共用的数据模型。 单独抽出来是为了让 ChatTransport（抽象接口）与 CProtocolBridge 都能引用，而不用互相 include 造成循环依赖。
 */
#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

/**
 * @brief   成员InfoS。
 * @author  JaTin
 */
struct MemberInfo_S {
    /**
     * @brief  编号。 初值 = 0。
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64  id = 0;
    /**
     * @brief  名称或昵称。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString name;
};

/**
 * @brief   消息类型：文本 / 语音。
 * @author  JaTin
 */
enum class MessageKind_EN : uint8_t {
    /**
     * @brief  文本。
     */
    Text  = 0,
    /**
     * @brief  语音消息。
     */
    Voice = 1,
};

/**
 * @brief   一条聊天消息（历史与实时共用）。
 * @author  JaTin
 */
struct ChatMessage_S {
    /**
     * @brief  全局单调序号，用于去重/补漏
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64      seq = 0;
    /**
     * @brief  from。 初值 = 0。
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64      from = 0;
    /**
     * @brief  名称或昵称。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString     name;
    /**
     * @brief  文本消息正文；语音消息为空
     * @note   类型 QString；类/结构体成员变量。
     */
    QString     text;
    /**
     * @brief  语音消息的 DPV1 容器；文本消息为空
     * @note   类型 QByteArray；类/结构体成员变量。
     */
    QByteArray  audio;
    /**
     * @brief  语音时长，单位毫秒。 初值 = 0。
     * @note   类型 int；类/结构体成员变量。
     */
    int         durationMs = 0;
    /**
     * @brief  消息类型。 初值 = MessageKind_EN::Text。
     * @note   类型 MessageKind_EN；类/结构体成员变量。
     */
    MessageKind_EN kind = MessageKind_EN::Text;
    /**
     * @brief  Unix 毫秒时间戳。 初值 = 0。
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64      ts = 0;
};

/**
 * @brief  一条智能回复建议：英文原文 + 对应中文译文。
 * @details 中英由同一次 LLM 请求一起产出，保证语义对齐，也省掉逐条翻译的额外延迟。
 */
struct Suggestion_S {
    /**
     * @brief  english。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString english;
    /**
     * @brief  模型没给中文时为空
     * @note   类型 QString；类/结构体成员变量。
     */
    QString chinese;
};

/**
 * @brief   一条历史房间条目（主页“对话练习”列表用）。
 * @author  JaTin
 */
struct VisitedRoom_S {
    /**
     * @brief  房间号
     * @note   类型 QString；类/结构体成员变量。
     */
    QString     room;
    /**
     * @brief  房间内最后一条消息的发送者；无消息时为空
     * @note   类型 QString；类/结构体成员变量。
     */
    QString     lastSender;
    /**
     * @brief  房间内最后一条消息预览；语音消息为“[语音]”；无消息时为空
     * @note   类型 QString；类/结构体成员变量。
     */
    QString     lastMessage;
    /**
     * @brief  最后一条消息时间戳；无消息时为 0
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64      lastTs = 0;
};

Q_DECLARE_METATYPE(MemberInfo_S)
Q_DECLARE_METATYPE(QVector<MemberInfo_S>)
Q_DECLARE_METATYPE(ChatMessage_S)
Q_DECLARE_METATYPE(QVector<ChatMessage_S>)
Q_DECLARE_METATYPE(Suggestion_S)
Q_DECLARE_METATYPE(QVector<Suggestion_S>)
Q_DECLARE_METATYPE(VisitedRoom_S)
Q_DECLARE_METATYPE(QVector<VisitedRoom_S>)
