/**
 * @file  ChatModel.cpp
 * @brief 聊天消息模型的实现。
 */

#include "business/ChatModel.h"

#include <QDateTime>

#include <algorithm>

namespace business {

ChatModel::ChatModel(QObject* parent) : QObject(parent) {}

// ---------------------------------------------------------------- 房间生命周期

void ChatModel::enterRoom(const QString& room, qint64 selfId, const QString& selfName,
                          const QVector<MemberInfo>& members) {
    room_ = room;
    selfId_ = selfId;
    selfName_ = selfName;
    if (selfId != 0) selfIds_.insert(selfId);

    members_.clear();
    for (const MemberInfo& m : members) {
        members_.insert(m.id, m.name);
    }

    items_.clear();
    seqs_.clear();

    emit modelReset();
    emit membersChanged();
}

void ChatModel::resumeRoom(const QString& room, qint64 selfId, const QString& selfName,
                           const QVector<MemberInfo>& members) {
    room_ = room;
    selfId_ = selfId;
    selfName_ = selfName;
    // 重连后服务端会分配新 id，旧消息仍应显示为自己发的
    if (selfId != 0) selfIds_.insert(selfId);

    members_.clear();
    for (const MemberInfo& m : members) {
        members_.insert(m.id, m.name);
    }

    emit membersChanged();
}

void ChatModel::clear() {
    room_.clear();
    selfId_ = 0;
    selfName_.clear();
    selfIds_.clear();
    members_.clear();
    items_.clear();
    seqs_.clear();

    emit modelReset();
    emit membersChanged();
}

// ---------------------------------------------------------------- 写入

void ChatModel::ensureMember(qint64 userId, const QString& name) {
    if (userId == 0 || members_.contains(userId)) return;
    members_.insert(userId, name);
    emit membersChanged();
}

void ChatModel::adoptLegacySelfId(qint64 from, const QString& name) {
    if (from == 0 || selfIds_.contains(from)) return;
    // 同名、且不在当前成员表 -> 视为本人上一次连接留下的 id
    if (!name.isEmpty() && name == selfName_ && !members_.contains(from)) {
        selfIds_.insert(from);
    }
}

bool ChatModel::appendChat(qint64 seq, qint64 from, const QString& name, const QString& text,
                           qint64 ts) {
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    if (seq > 0) {
        if (seqs_.contains(seq)) return false;   // 历史补发与实时推送可能重叠
        seqs_.insert(seq);
    }

    ensureMember(from, name);

    ChatItem item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.text = text;
    item.system = false;

    items_.push_back(item);
    emit itemAppended(item);
    return true;
}

bool ChatModel::appendVoice(qint64 seq, qint64 from, const QString& name,
                            const QByteArray& audio, int durationMs, qint64 ts) {
    if (audio.isEmpty()) return false;
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    if (seq > 0) {
        if (seqs_.contains(seq)) return false;
        seqs_.insert(seq);
    }

    ensureMember(from, name);

    ChatItem item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.system = false;
    item.kind = MessageKind::Voice;
    item.audio = audio;
    item.durationMs = durationMs;

    items_.push_back(item);
    emit itemAppended(item);
    return true;
}

int ChatModel::mergeHistory(const QVector<ChatMessage>& messages) {
    if (messages.isEmpty()) return 0;

    int added = 0;
    for (const ChatMessage& m : messages) {
        if (m.seq > 0 && seqs_.contains(m.seq)) continue;
        if (m.seq > 0) seqs_.insert(m.seq);

        adoptLegacySelfId(m.from, m.name);

        ChatItem item;
        item.seq = m.seq;
        item.from = m.from;
        item.ts = m.ts > 0 ? m.ts : QDateTime::currentMSecsSinceEpoch();
        item.name = m.name;
        item.system = false;
        item.kind = m.kind;
        if (m.kind == MessageKind::Voice) {
            item.audio = m.audio;
            item.durationMs = m.durationMs;
        } else {
            item.text = m.text;
        }

        items_.push_back(item);
        if (m.from != 0 && !members_.contains(m.from)) {
            members_.insert(m.from, m.name);
        }
        ++added;
    }

    if (added == 0) return 0;

    sortItems();
    emit modelReset();
    emit membersChanged();
    return added;
}

void ChatModel::appendSystem(const QString& text, qint64 ts) {
    if (text.isEmpty()) return;
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    ChatItem item;
    item.ts = ts;
    item.text = text;
    item.system = true;

    items_.push_back(item);
    emit itemAppended(item);
}

void ChatModel::applySystemEvent(const QString& event, qint64 userId, const QString& name,
                                 const QString& text, qint64 ts) {
    if (event == QStringLiteral("join")) {
        ensureMember(userId, name);
    } else if (event == QStringLiteral("leave")) {
        if (members_.remove(userId) > 0) emit membersChanged();
    } else if (event == QStringLiteral("rename")) {
        if (members_.contains(userId)) {
            members_[userId] = name;
            emit membersChanged();
        }
    }

    // welcome / left 只影响状态，不产生可见消息
    if (event == QStringLiteral("welcome") || event == QStringLiteral("left")) return;

    appendSystem(text, ts);
}

// ---------------------------------------------------------------- 状态回填

void ChatModel::setTranslation(qint64 seq, const QString& chinese) {
    ChatItem* item = find(seq);
    if (item == nullptr) return;
    item->translation = chinese;
    item->translationState = 2;
    emit itemUpdated(seq);
}

void ChatModel::setTranslationState(qint64 seq, int state) {
    ChatItem* item = find(seq);
    if (item == nullptr) return;
    item->translationState = state;
    emit itemUpdated(seq);
}

void ChatModel::setTranscript(qint64 seq, const QString& text, bool ok) {
    ChatItem* item = find(seq);
    if (item == nullptr) return;

    if (ok && !text.isEmpty()) {
        item->transcript = text;
        item->transcriptState = 2;
    } else {
        item->transcriptState = 3;
    }
    emit itemUpdated(seq);
}

void ChatModel::setTranscriptState(qint64 seq, int state) {
    ChatItem* item = find(seq);
    if (item == nullptr) return;
    item->transcriptState = state;
    emit itemUpdated(seq);
}

// ---------------------------------------------------------------- 查询

ChatItem* ChatModel::find(qint64 seq) {
    for (ChatItem& item : items_) {
        if (item.seq == seq) return &item;
    }
    return nullptr;
}

const ChatItem* ChatModel::find(qint64 seq) const {
    for (const ChatItem& item : items_) {
        if (item.seq == seq) return &item;
    }
    return nullptr;
}

void ChatModel::sortItems() {
    std::stable_sort(items_.begin(), items_.end(),
                     [](const ChatItem& a, const ChatItem& b) {
                         if (a.ts != b.ts) return a.ts < b.ts;
                         return a.seq < b.seq;
                     });
}

QVector<ChatMessage> ChatModel::recentContext(int limit) const {
    QVector<ChatMessage> context;
    for (int i = items_.size() - 1; i >= 0 && context.size() < limit; --i) {
        const ChatItem& item = items_.at(i);
        if (item.system) continue;

        ChatMessage message;
        message.seq = item.seq;
        message.from = item.from;
        message.name = item.name;
        message.text = item.text;
        message.ts = item.ts;
        context.prepend(message);
    }
    return context;
}

}  // namespace business
