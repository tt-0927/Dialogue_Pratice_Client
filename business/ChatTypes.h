/*
 * 文件名:    ChatTypes.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  ChatTypes 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* ChatTypes.h */

/* 传输层与界面共用的数据模型。 */
/* 单独抽出来是为了让 ChatTransport（抽象接口）与 CProtocolBridge */
/* 都能引用，而不用互相 include 造成循环依赖。 */
#pragma once

#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QVector>

struct MemberInfo_S {
    qint64  id = 0;
    QString name;
};

/* 消息类型：文本 / 语音。 */
enum class MessageKind_EN : uint8_t {
    Text  = 0,
    Voice = 1,
};

/* 一条聊天消息（历史与实时共用）。 */
struct ChatMessage_S {
    /* 全局单调序号，用于去重/补漏 */
    qint64      seq = 0;
    qint64      from = 0;
    QString     name;
    /* 文本消息正文；语音消息为空 */
    QString     text;
    /* 语音消息的 DPV1 容器；文本消息为空 */
    QByteArray  audio;
    int         durationMs = 0;
    MessageKind_EN kind = MessageKind_EN::Text;
    qint64      ts = 0;
};

/* 一条智能回复建议：英文原文 + 对应中文译文。 */
/* 中英由同一次 LLM 请求一起产出，保证语义对齐，也省掉逐条翻译的额外延迟。 */
struct Suggestion_S {
    QString english;
    /* 模型没给中文时为空 */
    QString chinese;
};

Q_DECLARE_METATYPE(MemberInfo_S)
Q_DECLARE_METATYPE(QVector<MemberInfo_S>)
Q_DECLARE_METATYPE(ChatMessage_S)
Q_DECLARE_METATYPE(QVector<ChatMessage_S>)
Q_DECLARE_METATYPE(Suggestion_S)
Q_DECLARE_METATYPE(QVector<Suggestion_S>)
