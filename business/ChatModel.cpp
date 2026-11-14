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

/*
 * 功能描述:  构造 ChatModel 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject* parent) : QObject(。
 * return:    无返回值（构造函数或析构函数）。
 */
ChatModel::ChatModel(QObject* pParent) : QObject(pParent) {}

/*
 * 功能描述:  进入房间。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   lSelfId          本人会话编号，类型 qint64。
 * param [in]   strSelfName      本人昵称，类型 const QString&。
 * param [in]   aMembers         初始成员列表，类型 const QVector<MemberInfo_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */

void ChatModel::enterRoom(const QString& strRoom, qint64 lSelfId, const QString& strSelfName,
                          const QVector<MemberInfo_S>& aMembers) {
    m_strRoom = strRoom;
    m_uSelfId = lSelfId;
    m_strSelfName = strSelfName;
    if (lSelfId != 0) m_uSelfIds.insert(lSelfId);

    m_uMembers.clear();
    for (const MemberInfo_S& stM : aMembers) {
        m_uMembers.insert(stM.id, stM.name);
    }

    m_aItems.clear();
    m_uSeqs.clear();

    emit modelReset();
    emit membersChanged();
}

/*
 * 功能描述:  恢复房间。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   lSelfId          本人会话编号，类型 qint64。
 * param [in]   strSelfName      本人昵称，类型 const QString&。
 * param [in]   aMembers         初始成员列表，类型 const QVector<MemberInfo_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::resumeRoom(const QString& strRoom, qint64 lSelfId, const QString& strSelfName,
                           const QVector<MemberInfo_S>& aMembers) {
    m_strRoom = strRoom;
    m_uSelfId = lSelfId;
    m_strSelfName = strSelfName;
    /* 重连后服务端会分配新 id，旧消息仍应显示为自己发的 */
    if (lSelfId != 0) m_uSelfIds.insert(lSelfId);

    m_uMembers.clear();
    for (const MemberInfo_S& stM : aMembers) {
        m_uMembers.insert(stM.id, stM.name);
    }

    emit membersChanged();
}

/*
 * 功能描述:  清空clear。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
 * 功能描述:  确保成员。
 * 作者:      JaTin
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::ensureMember(qint64 lUserId, const QString& strName) {
    if (lUserId == 0 || m_uMembers.contains(lUserId)) return;
    m_uMembers.insert(lUserId, strName);
    emit membersChanged();
}

/*
 * 功能描述:  执行adoptlegacyself 编号。
 * 作者:      JaTin
 * param [in]   lFrom            输入数据，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::adoptLegacySelfId(qint64 lFrom, const QString& strName) {
    if (lFrom == 0 || m_uSelfIds.contains(lFrom)) return;
    /* 同名、且不在当前成员表 -> 视为本人上一次连接留下的 id */
    if (!strName.isEmpty() && strName == m_strSelfName && !m_uMembers.contains(lFrom)) {
        m_uSelfIds.insert(lFrom);
    }
}

/*
 * 功能描述:  追加chat；失败时返回 false。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   lFrom            输入数据，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool ChatModel::appendChat(qint64 lSeq, qint64 lFrom, const QString& strName, const QString& strText,
                           qint64 lTs) {
    if (lTs <= 0) lTs = QDateTime::currentMSecsSinceEpoch();

    if (lSeq > 0) {
        /* 历史补发与实时推送可能重叠 */
        if (m_uSeqs.contains(lSeq)) return false;
        m_uSeqs.insert(lSeq);
    }

    ensureMember(lFrom, strName);

    ChatItem_S stItem;
    stItem.seq = lSeq;
    stItem.from = lFrom;
    stItem.ts = lTs;
    stItem.name = strName;
    stItem.text = strText;
    stItem.system = false;

    m_aItems.push_back(stItem);
    emit itemAppended(stItem);
    return true;
}

/*
 * 功能描述:  追加语音消息；失败时返回 false。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   lFrom            输入数据，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   byAudio          Opus 编码的音频数据，类型 const QByteArray&。
 * param [in]   nDurationMs      语音时长，单位毫秒，类型 int。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool ChatModel::appendVoice(qint64 lSeq, qint64 lFrom, const QString& strName,
                            const QByteArray& byAudio, int nDurationMs, qint64 lTs) {
    if (byAudio.isEmpty()) return false;
    if (lTs <= 0) lTs = QDateTime::currentMSecsSinceEpoch();

    if (lSeq > 0) {
        if (m_uSeqs.contains(lSeq)) return false;
        m_uSeqs.insert(lSeq);
    }

    ensureMember(lFrom, strName);

    ChatItem_S stItem;
    stItem.seq = lSeq;
    stItem.from = lFrom;
    stItem.ts = lTs;
    stItem.name = strName;
    stItem.system = false;
    stItem.kind = MessageKind_EN::Voice;
    stItem.audio = byAudio;
    stItem.durationMs = nDurationMs;

    m_aItems.push_back(stItem);
    emit itemAppended(stItem);
    return true;
}

/*
 * 功能描述:  合并历史消息。
 * 作者:      JaTin
 * param [in]   aMessages        消息记录，类型 const QVector<ChatMessage_S>&。
 * return:    返回处理结果码；0 表示成功，负值或 -1 表示失败。
 */
int ChatModel::mergeHistory(const QVector<ChatMessage_S>& aMessages) {
    if (aMessages.isEmpty()) return 0;

    int nAdded = 0;
    for (const ChatMessage_S& stM : aMessages) {
        if (stM.seq > 0 && m_uSeqs.contains(stM.seq)) continue;
        if (stM.seq > 0) m_uSeqs.insert(stM.seq);

        adoptLegacySelfId(stM.from, stM.name);

        ChatItem_S stItem;
        stItem.seq = stM.seq;
        stItem.from = stM.from;
        stItem.ts = stM.ts > 0 ? stM.ts : QDateTime::currentMSecsSinceEpoch();
        stItem.name = stM.name;
        stItem.system = false;
        stItem.kind = stM.kind;
        if (stM.kind == MessageKind_EN::Voice) {
            stItem.audio = stM.audio;
            stItem.durationMs = stM.durationMs;
        } else {
            stItem.text = stM.text;
        }

        m_aItems.push_back(stItem);
        if (stM.from != 0 && !m_uMembers.contains(stM.from)) {
            m_uMembers.insert(stM.from, stM.name);
        }
        ++nAdded;
    }

    if (nAdded == 0) return 0;

    sortItems();
    emit modelReset();
    emit membersChanged();
    return nAdded;
}

/*
 * 功能描述:  追加系统提示词。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::appendSystem(const QString& strText, qint64 lTs) {
    if (strText.isEmpty()) return;
    if (lTs <= 0) lTs = QDateTime::currentMSecsSinceEpoch();

    ChatItem_S stItem;
    stItem.ts = lTs;
    stItem.text = strText;
    stItem.system = true;

    m_aItems.push_back(stItem);
    emit itemAppended(stItem);
}

/*
 * 功能描述:  应用系统提示词事件。
 * 作者:      JaTin
 * param [in]   strEvent         事件，类型 const QString&。
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::applySystemEvent(const QString& strEvent, qint64 lUserId, const QString& strName,
                                 const QString& strText, qint64 lTs) {
    if (strEvent == QStringLiteral("join")) {
        ensureMember(lUserId, strName);
    } else if (strEvent == QStringLiteral("leave")) {
        if (m_uMembers.remove(lUserId) > 0) emit membersChanged();
    } else if (strEvent == QStringLiteral("rename")) {
        if (m_uMembers.contains(lUserId)) {
            m_uMembers[lUserId] = strName;
            emit membersChanged();
        }
    }

    /* welcome / left 只影响状态，不产生可见消息 */
    if (strEvent == QStringLiteral("welcome") || strEvent == QStringLiteral("left")) return;

    appendSystem(strText, lTs);
}

/* ---------------------------------------------------------------- 状态回填 */

/*
 * 功能描述:  设置译文。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   strChinese       输入数据，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::setTranslation(qint64 lSeq, const QString& strChinese) {
    ChatItem_S* pItem = find(lSeq);
    if (pItem == nullptr) return;
    pItem->translation = strChinese;
    pItem->translationState = 2;
    emit itemUpdated(lSeq);
}

/*
 * 功能描述:  设置译文状态。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   nState           状态值，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::setTranslationState(qint64 lSeq, int nState) {
    ChatItem_S* pItem = find(lSeq);
    if (pItem == nullptr) return;
    pItem->translationState = nState;
    emit itemUpdated(lSeq);
}

/*
 * 功能描述:  设置语音消息的转写文本及其状态。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   nOk              输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::setTranscript(qint64 lSeq, const QString& strText, bool nOk) {
    ChatItem_S* pItem = find(lSeq);
    if (pItem == nullptr) return;

    if (nOk && !strText.isEmpty()) {
        pItem->transcript = strText;
        pItem->transcriptState = 2;
    } else {
        pItem->transcriptState = 3;
    }
    emit itemUpdated(lSeq);
}

/*
 * 功能描述:  设置转写文本状态。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   nState           状态值，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::setTranscriptState(qint64 lSeq, int nState) {
    ChatItem_S* pItem = find(lSeq);
    if (pItem == nullptr) return;
    pItem->transcriptState = nState;
    emit itemUpdated(lSeq);
}

/* ---------------------------------------------------------------- 查询 */

/*
 * 功能描述:  查找find。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * return:    成功返回对象指针；未找到或失败时返回 nullptr。
 */
ChatItem_S* ChatModel::find(qint64 lSeq) {
    for (ChatItem_S& stItem : m_aItems) {
        if (stItem.seq == lSeq) return &stItem;
    }
    return nullptr;
}

/*
 * 功能描述:  查找find。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * return:    成功返回对象指针；未找到或失败时返回 nullptr。
 */
const ChatItem_S* ChatModel::find(qint64 lSeq) const {
    for (const ChatItem_S& stItem : m_aItems) {
        if (stItem.seq == lSeq) return &stItem;
    }
    return nullptr;
}

/*
 * 功能描述:  执行sort 条目列表。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatModel::sortItems() {
    std::stable_sort(m_aItems.begin(), m_aItems.end(),
                     [](const ChatItem_S& a, const ChatItem_S& b) {
                         if (a.ts != b.ts) return a.ts < b.ts;
                         return a.seq < b.seq;
                     });
}

/*
 * 功能描述:  获取recent 上下文。
 * 作者:      JaTin
 * param [in]   nLimit           输入数据，类型 int。
 * return:    返回 QVector<ChatMessage_S> 类型的处理结果，具体含义见功能描述。
 */
QVector<ChatMessage_S> ChatModel::recentContext(int nLimit) const {
    QVector<ChatMessage_S> aContext;
    for (int i = m_aItems.size() - 1; i >= 0 && aContext.size() < nLimit; --i) {
        const ChatItem_S& stItem = m_aItems.at(i);
        if (stItem.system) continue;

        ChatMessage_S stMessage;
        stMessage.seq = stItem.seq;
        stMessage.from = stItem.from;
        stMessage.name = stItem.name;
        stMessage.text = stItem.text;
        stMessage.ts = stItem.ts;
        aContext.prepend(stMessage);
    }
    return aContext;
}

/* namespace business */
}
