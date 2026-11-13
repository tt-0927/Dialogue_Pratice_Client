/*
 * 文件名:    ChatModel.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  聊天消息模型的实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#include "business/ChatModel.h"

#include <QDateTime>

#include <algorithm>

namespace business {

ChatModel::ChatModel(QObject* parent) : QObject(parent) {}

/* ---------------------------------------------------------------- 房间生命周期 */

void ChatModel::enterRoom(const QString& room, qint64 selfId, const QString& selfName,
                          const QVector<MemberInfo_S>& members) {
    m_strRoom = room;
    m_uSelfId = selfId;
    m_strSelfName = selfName;
    if (selfId != 0) m_uSelfIds.insert(selfId);

    m_uMembers.clear();
    for (const MemberInfo_S& m : members) {
        m_uMembers.insert(m.id, m.name);
    }

    m_aItems.clear();
    m_uSeqs.clear();

    emit modelReset();
    emit membersChanged();
}

void ChatModel::resumeRoom(const QString& room, qint64 selfId, const QString& selfName,
                           const QVector<MemberInfo_S>& members) {
    m_strRoom = room;
    m_uSelfId = selfId;
    m_strSelfName = selfName;
    /* 重连后服务端会分配新 id，旧消息仍应显示为自己发的 */
    if (selfId != 0) m_uSelfIds.insert(selfId);

    m_uMembers.clear();
    for (const MemberInfo_S& m : members) {
        m_uMembers.insert(m.id, m.name);
    }

    emit membersChanged();
}

/*
 * 功能描述:  执行 clear 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatModel::clear() {
    m_strRoom.clear();
    m_uSelfId = 0;
    m_strSelfName.clear();
    m_uSelfIds.clear();
    m_uMembers.clear();
    m_aItems.clear();
    m_uSeqs.clear();

    emit modelReset();
    emit membersChanged();
}

/* ---------------------------------------------------------------- 写入 */

/*
 * 功能描述:  执行 ensureMember 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  userId  参数。
 * param [in]  name  参数。
 * return:    void
 */
void ChatModel::ensureMember(qint64 userId, const QString& name) {
    if (userId == 0 || m_uMembers.contains(userId)) return;
    m_uMembers.insert(userId, name);
    emit membersChanged();
}

/*
 * 功能描述:  执行 adoptLegacySelfId 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  from  参数。
 * param [in]  name  参数。
 * return:    void
 */
void ChatModel::adoptLegacySelfId(qint64 from, const QString& name) {
    if (from == 0 || m_uSelfIds.contains(from)) return;
    /* 同名、且不在当前成员表 -> 视为本人上一次连接留下的 id */
    if (!name.isEmpty() && name == m_strSelfName && !m_uMembers.contains(from)) {
        m_uSelfIds.insert(from);
    }
}

bool ChatModel::appendChat(qint64 seq, qint64 from, const QString& name, const QString& text,
                           qint64 ts) {
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    if (seq > 0) {
        /* 历史补发与实时推送可能重叠 */
        if (m_uSeqs.contains(seq)) return false;
        m_uSeqs.insert(seq);
    }

    ensureMember(from, name);

    ChatItem_S item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.text = text;
    item.system = false;

    m_aItems.push_back(item);
    emit itemAppended(item);
    return true;
}

bool ChatModel::appendVoice(qint64 seq, qint64 from, const QString& name,
                            const QByteArray& audio, int durationMs, qint64 ts) {
    if (audio.isEmpty()) return false;
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    if (seq > 0) {
        if (m_uSeqs.contains(seq)) return false;
        m_uSeqs.insert(seq);
    }

    ensureMember(from, name);

    ChatItem_S item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.system = false;
    item.kind = MessageKind_EN::Voice;
    item.audio = audio;
    item.durationMs = durationMs;

    m_aItems.push_back(item);
    emit itemAppended(item);
    return true;
}

/*
 * 功能描述:  执行 mergeHistory 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  messages  参数。
 * return:    返回值，含义见功能描述。
 */
int ChatModel::mergeHistory(const QVector<ChatMessage_S>& messages) {
    if (messages.isEmpty()) return 0;

    int added = 0;
    for (const ChatMessage_S& m : messages) {
        if (m.seq > 0 && m_uSeqs.contains(m.seq)) continue;
        if (m.seq > 0) m_uSeqs.insert(m.seq);

        adoptLegacySelfId(m.from, m.name);

        ChatItem_S item;
        item.seq = m.seq;
        item.from = m.from;
        item.ts = m.ts > 0 ? m.ts : QDateTime::currentMSecsSinceEpoch();
        item.name = m.name;
        item.system = false;
        item.kind = m.kind;
        if (m.kind == MessageKind_EN::Voice) {
            item.audio = m.audio;
            item.durationMs = m.durationMs;
        } else {
            item.text = m.text;
        }

        m_aItems.push_back(item);
        if (m.from != 0 && !m_uMembers.contains(m.from)) {
            m_uMembers.insert(m.from, m.name);
        }
        ++added;
    }

    if (added == 0) return 0;

    sortItems();
    emit modelReset();
    emit membersChanged();
    return added;
}

/*
 * 功能描述:  执行 appendSystem 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * param [in]  ts  参数。
 * return:    void
 */
void ChatModel::appendSystem(const QString& text, qint64 ts) {
    if (text.isEmpty()) return;
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    ChatItem_S item;
    item.ts = ts;
    item.text = text;
    item.system = true;

    m_aItems.push_back(item);
    emit itemAppended(item);
}

void ChatModel::applySystemEvent(const QString& event, qint64 userId, const QString& name,
                                 const QString& text, qint64 ts) {
    if (event == QStringLiteral("join")) {
        ensureMember(userId, name);
    } else if (event == QStringLiteral("leave")) {
        if (m_uMembers.remove(userId) > 0) emit membersChanged();
    } else if (event == QStringLiteral("rename")) {
        if (m_uMembers.contains(userId)) {
            m_uMembers[userId] = name;
            emit membersChanged();
        }
    }

    /* welcome / left 只影响状态，不产生可见消息 */
    if (event == QStringLiteral("welcome") || event == QStringLiteral("left")) return;

    appendSystem(text, ts);
}

/* ---------------------------------------------------------------- 状态回填 */

/*
 * 功能描述:  执行 setTranslation 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * param [in]  chinese  参数。
 * return:    void
 */
void ChatModel::setTranslation(qint64 seq, const QString& chinese) {
    ChatItem_S* item = find(seq);
    if (item == nullptr) return;
    item->translation = chinese;
    item->translationState = 2;
    emit itemUpdated(seq);
}

/*
 * 功能描述:  执行 setTranslationState 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * param [in]  state  参数。
 * return:    void
 */
void ChatModel::setTranslationState(qint64 seq, int state) {
    ChatItem_S* item = find(seq);
    if (item == nullptr) return;
    item->translationState = state;
    emit itemUpdated(seq);
}

/*
 * 功能描述:  执行 setTranscript 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * param [in]  text  参数。
 * param [in]  ok  参数。
 * return:    void
 */
void ChatModel::setTranscript(qint64 seq, const QString& text, bool ok) {
    ChatItem_S* item = find(seq);
    if (item == nullptr) return;

    if (ok && !text.isEmpty()) {
        item->transcript = text;
        item->transcriptState = 2;
    } else {
        item->transcriptState = 3;
    }
    emit itemUpdated(seq);
}

/*
 * 功能描述:  执行 setTranscriptState 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * param [in]  state  参数。
 * return:    void
 */
void ChatModel::setTranscriptState(qint64 seq, int state) {
    ChatItem_S* item = find(seq);
    if (item == nullptr) return;
    item->transcriptState = state;
    emit itemUpdated(seq);
}

/* ---------------------------------------------------------------- 查询 */

/*
 * 功能描述:  执行 find 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * return:    返回值，含义见功能描述。
 */
ChatItem_S* ChatModel::find(qint64 seq) {
    for (ChatItem_S& item : m_aItems) {
        if (item.seq == seq) return &item;
    }
    return nullptr;
}

/*
 * 功能描述:  执行 find 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * return:    返回值，含义见功能描述。
 */
const ChatItem_S* ChatModel::find(qint64 seq) const {
    for (const ChatItem_S& item : m_aItems) {
        if (item.seq == seq) return &item;
    }
    return nullptr;
}

/*
 * 功能描述:  执行 sortItems 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatModel::sortItems() {
    std::stable_sort(m_aItems.begin(), m_aItems.end(),
                     [](const ChatItem_S& a, const ChatItem_S& b) {
                         if (a.ts != b.ts) return a.ts < b.ts;
                         return a.seq < b.seq;
                     });
}

/*
 * 功能描述:  执行 recentContext 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  limit  参数。
 * return:    返回值，含义见功能描述。
 */
QVector<ChatMessage_S> ChatModel::recentContext(int limit) const {
    QVector<ChatMessage_S> context;
    for (int i = m_aItems.size() - 1; i >= 0 && context.size() < limit; --i) {
        const ChatItem_S& item = m_aItems.at(i);
        if (item.system) continue;

        ChatMessage_S message;
        message.seq = item.seq;
        message.from = item.from;
        message.name = item.name;
        message.text = item.text;
        message.ts = item.ts;
        context.prepend(message);
    }
    return context;
}

/* namespace business */
}
