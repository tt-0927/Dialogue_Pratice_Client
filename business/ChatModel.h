/**
 * @file  ChatModel.h
 * @brief 聊天消息模型：数据、去重、排序、成员表。
 * @details 从原 ChatPage 中剥离出来的纯数据层。它不知道任何控件，
 *          只负责「有哪些消息、每条消息处于什么状态」，变化通过信号广播。
 *
 *          去重依据是服务端分配的全局 `seq`：历史补发与实时推送可能重叠，
 *          同一 seq 只保留第一次出现的版本。
 */

#pragma once

#include <QByteArray>
#include <QDate>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

#include "business/ChatTypes.h"

namespace business {

/**
 * @brief 一条已进入模型的消息。
 *
 * @details 与 `ChatMessage` 的区别：`ChatMessage` 是「网络上的一条消息」，
 *          本结构额外携带界面所需的状态（译文、转写进度）。
 */
struct ChatItem {
    qint64  seq = 0;        ///< 全局消息序号，0 表示本地生成的系统提示
    qint64  from = 0;       ///< 发送者会话 id
    qint64  ts = 0;         ///< Unix 毫秒时间戳
    QString name;           ///< 发送者昵称
    QString text;           ///< 文本正文；语音消息为空
    bool    system = false; ///< 是否为系统提示（居中的灰色胶囊）

    MessageKind kind = MessageKind::Text; ///< 文本或语音
    QByteArray  audio;                    ///< 语音的 DPV1 容器
    int         durationMs = 0;           ///< 语音时长

    QString translation;          ///< 中文译文
    int     translationState = 0; ///< 0 无 / 1 翻译中 / 2 完成 / 3 失败

    QString transcript;          ///< 语音的英文转写
    int     transcriptState = 0; ///< 0 无 / 1 转写中 / 2 完成 / 3 失败
};

/**
 * @brief 聊天消息模型。
 *
 * @details 所有方法都只能在 GUI 线程调用。变化通过三类信号广播：
 *          - `itemAppended`：新增一条，视图增量追加
 *          - `modelReset`：全量重建（历史批量合并后）
 *          - `itemUpdated`：某条的译文/转写回填，视图局部刷新
 */
class ChatModel : public QObject {
    Q_OBJECT

public:
    explicit ChatModel(QObject* parent = nullptr);

    // ---- 房间生命周期 ----

    /**
     * @brief 进入一个新房间，清空旧数据。
     * @param room    房间号。
     * @param selfId  本人会话 id。
     * @param selfName 本人昵称。
     * @param members 初始成员表。
     */
    void enterRoom(const QString& room, qint64 selfId, const QString& selfName,
                   const QVector<MemberInfo>& members);

    /**
     * @brief 断线重连后回到原房间，保留已有消息。
     * @param room     房间号。
     * @param selfId   新分配的会话 id。
     * @param selfName 本人昵称。
     * @param members  最新成员表。
     */
    void resumeRoom(const QString& room, qint64 selfId, const QString& selfName,
                    const QVector<MemberInfo>& members);

    /// @brief 清空全部数据（离开房间或登出）。
    void clear();

    /// @brief 当前房间号，空串表示不在房间。
    QString room() const { return room_; }

    /// @brief 本人昵称。
    QString selfName() const { return selfName_; }

    // ---- 写入 ----

    /**
     * @brief 追加一条文本消息。
     * @param seq  消息序号，0 表示本地消息（不做去重）。
     * @param from 发送者 id。
     * @param name 发送者昵称。
     * @param text 正文。
     * @param ts   时间戳，0 表示用当前时间。
     * @retval true  已加入。
     * @retval false 序号重复，已忽略。
     */
    bool appendChat(qint64 seq, qint64 from, const QString& name, const QString& text,
                    qint64 ts);

    /**
     * @brief 追加一条语音消息。
     * @param seq        消息序号。
     * @param from       发送者 id。
     * @param name       发送者昵称。
     * @param audio      DPV1 容器。
     * @param durationMs 时长。
     * @param ts         时间戳。
     * @retval true  已加入。
     * @retval false 音频为空或序号重复。
     */
    bool appendVoice(qint64 seq, qint64 from, const QString& name, const QByteArray& audio,
                     int durationMs, qint64 ts);

    /**
     * @brief 批量合并历史消息。
     * @param messages 服务端或本地缓存给出的历史。
     * @retval 实际新增的条数（0 表示全是重复的）。
     * @details 合并后会按时间重排并发出 `modelReset`。
     */
    int mergeHistory(const QVector<ChatMessage>& messages);

    /**
     * @brief 追加一条系统提示。
     * @param text 提示文本。
     * @param ts   时间戳。
     */
    void appendSystem(const QString& text, qint64 ts);

    /**
     * @brief 应用一条系统事件，同时维护成员表。
     * @param event   事件名：join / leave / rename / welcome / left。
     * @param userId  相关用户 id。
     * @param name    用户昵称（rename 时为新名字）。
     * @param text    要显示的提示文本。
     * @param ts      时间戳。
     * @details `welcome` 与 `left` 不产生可见消息，仅更新成员表。
     */
    void applySystemEvent(const QString& event, qint64 userId, const QString& name,
                          const QString& text, qint64 ts);

    // ---- 状态回填 ----

    /**
     * @brief 写入译文。
     * @param seq       消息序号。
     * @param chinese   中文译文。
     */
    void setTranslation(qint64 seq, const QString& chinese);

    /**
     * @brief 设置翻译状态。
     * @param seq   消息序号。
     * @param state 0 无 / 1 翻译中 / 2 完成 / 3 失败。
     */
    void setTranslationState(qint64 seq, int state);

    /**
     * @brief 写入语音转写结果。
     * @param seq  消息序号。
     * @param text 英文转写；为空表示失败。
     * @param ok   是否识别成功。
     */
    void setTranscript(qint64 seq, const QString& text, bool ok);

    /**
     * @brief 设置转写状态。
     * @param seq   消息序号。
     * @param state 0 无 / 1 转写中 / 2 完成 / 3 失败。
     * @details 用于把条目标记为「转写中」，让界面立刻显示占位。
     */
    void setTranscriptState(qint64 seq, int state);

    // ---- 查询 ----

    /// @brief 全部消息（按时间正序）。
    const QVector<ChatItem>& items() const { return items_; }

    /**
     * @brief 按序号查找。
     * @param seq 消息序号。
     * @retval 指针，不存在时返回 nullptr；`modelReset` 后指针失效。
     */
    ChatItem* find(qint64 seq);
    const ChatItem* find(qint64 seq) const;

    /// @brief 成员表（id -> 昵称），按 id 有序。
    const QMap<qint64, QString>& members() const { return members_; }

    /**
     * @brief 判断某条消息是否由本人发出。
     * @param userId 消息里的 `from` 字段。
     * @retval true 本人（含历次重连前使用的 id）。
     */
    bool isSelf(qint64 userId) const { return userId != 0 && selfIds_.contains(userId); }

    /// @brief 当前成员数。
    int memberCount() const { return members_.size(); }

    /**
     * @brief 取最近若干条非系统消息，用于生成建议回复的上下文。
     * @param limit 最多取几条。
     * @retval 按时间正序的消息列表。
     */
    QVector<ChatMessage> recentContext(int limit) const;

signals:
    /// @brief 新增一条消息，视图应追加渲染。
    void itemAppended(const business::ChatItem& item);

    /// @brief 模型被整体重排或清空，视图应全量重建。
    void modelReset();

    /// @brief 某条消息的译文或转写状态变化，视图应局部刷新。
    void itemUpdated(qint64 seq);

    /// @brief 成员表变化。
    void membersChanged();

private:
    /**
     * @brief 判断并登记「本人旧会话 id」。
     * @param from 消息发送者 id。
     * @param name 消息发送者昵称。
     * @details 服务端每次连接都分配新 id，重启客户端后历史里本人的旧消息
     *          会看起来像别人发的。这里用「同名且不在成员表」作为启发式判定。
     */
    void adoptLegacySelfId(qint64 from, const QString& name);

    /// @brief 按时间戳排序，时间相同按 seq。
    void sortItems();

    void ensureMember(qint64 userId, const QString& name);

    QString             room_;      ///< 当前房间号
    qint64              selfId_ = 0;///< 当前会话 id
    QString             selfName_;  ///< 本人昵称
    QSet<qint64>        selfIds_;   ///< 本人历次 id
    QMap<qint64, QString> members_; ///< 成员表
    QVector<ChatItem>   items_;     ///< 消息列表
    QSet<qint64>        seqs_;      ///< 已收录的序号，用于去重
};

}  // namespace business
