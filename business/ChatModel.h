/**
 * @file       ChatModel.h
 * @brief      聊天消息模型：数据、去重、排序、成员表。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
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
 * @brief   一条已进入模型的消息。
 * @details 与 `ChatMessage_S` 的区别：`ChatMessage_S` 是「网络上的一条消息」， 本结构额外携带界面所需的状态（译文、转写进度）。
 * @author  JaTin
 */
struct ChatItem_S {
    /**
     * @brief  全局消息序号，0 表示本地生成的系统提示
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64  seq = 0;
    /**
     * @brief  发送者会话 id
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64  from = 0;
    /**
     * @brief  Unix 毫秒时间戳
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64  ts = 0;
    /**
     * @brief  发送者昵称
     * @note   类型 QString；类/结构体成员变量。
     */
    QString name;
    /**
     * @brief  文本正文；语音消息为空
     * @note   类型 QString；类/结构体成员变量。
     */
    QString text;
    /**
     * @brief  是否为系统提示（居中的灰色胶囊）
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    system = false;

    /**
     * @brief  文本或语音
     * @note   类型 MessageKind_EN；类/结构体成员变量。
     */
    MessageKind_EN kind = MessageKind_EN::Text;
    /**
     * @brief  语音的 DPV1 容器
     * @note   类型 QByteArray；类/结构体成员变量。
     */
    QByteArray  audio;
    /**
     * @brief  语音时长
     * @note   类型 int；类/结构体成员变量。
     */
    int         durationMs = 0;

    /**
     * @brief  中文译文
     * @note   类型 QString；类/结构体成员变量。
     */
    QString translation;
    /**
     * @brief  0 无 / 1 翻译中 / 2 完成 / 3 失败
     * @note   类型 int；类/结构体成员变量。
     */
    int     translationState = 0;

    /**
     * @brief  语音的英文转写
     * @note   类型 QString；类/结构体成员变量。
     */
    QString transcript;
    /**
     * @brief  0 无 / 1 转写中 / 2 完成 / 3 失败
     * @note   类型 int；类/结构体成员变量。
     */
    int     transcriptState = 0;
};

/**
 * @brief   聊天消息模型。
 * @details 所有方法都只能在 GUI 线程调用。变化通过三类信号广播： - `itemAppended`：新增一条，视图增量追加 - `modelReset`：全量重建（历史批量合并后） - `itemUpdated`：某条的译文/转写回填，视图局部刷新
 * @author  JaTin
 */
class ChatModel : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit ChatModel(QObject* parent = nullptr);

    /**
     * @brief  ---- 房间生命周期 ----。
     * @details 进入一个新房间，清空旧数据。
     */
    void enterRoom(const QString& room, qint64 selfId, const QString& selfName,
                   const QVector<MemberInfo_S>& members);

    /**
     * @brief 断线重连后回到原房间，保留已有消息。
     * @param room     房间号。
     * @param selfId   新分配的会话 id。
     * @param selfName 本人昵称。
     * @param members  最新成员表。
     */
    void resumeRoom(const QString& room, qint64 selfId, const QString& selfName,
                    const QVector<MemberInfo_S>& members);

    /**
     * @brief      清空全部数据（离开房间或登出）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clear();

    /**
     * @brief  当前房间号，空串表示不在房间。
     */
    QString room() const { return m_strRoom; }

    /**
     * @brief  本人昵称。
     */
    QString selfName() const { return m_strSelfName; }

    /**
     * @brief  ---- 写入 ----。
     * @details 追加一条文本消息。
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
     * @brief      批量合并历史消息。
     * @details    合并后会按时间重排并发出 `modelReset`。
     * @param[in] messages       服务端或本地缓存给出的历史；类型 const QVector<ChatMessage_S>&。
     * @return     实际新增的条数（0 表示全是重复的）。
     * @author     JaTin
     */
    int mergeHistory(const QVector<ChatMessage_S>& messages);

    /**
     * @brief      追加一条系统提示。
     * @param[in] text           提示文本；类型 const QString&。
     * @param[in] ts             时间戳；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
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

    /**
     * @brief  ---- 状态回填 ----。
     * @details 写入译文。
     */
    void setTranslation(qint64 seq, const QString& chinese);

    /**
     * @brief      设置翻译状态。
     * @param[in] seq            消息序号；类型 qint64。
     * @param[in] state          0 无 / 1 翻译中 / 2 完成 / 3 失败；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranslationState(qint64 seq, int state);

    /**
     * @brief      写入语音转写结果。
     * @param[in] seq            消息序号；类型 qint64。
     * @param[in] text           英文转写；为空表示失败；类型 const QString&。
     * @param[in] ok             是否识别成功；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranscript(qint64 seq, const QString& text, bool ok);

    /**
     * @brief      设置转写状态。
     * @details    用于把条目标记为「转写中」，让界面立刻显示占位。
     * @param[in] seq            消息序号；类型 qint64。
     * @param[in] state          0 无 / 1 转写中 / 2 完成 / 3 失败；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranscriptState(qint64 seq, int state);

    /**
     * @brief  ---- 查询 ----。
     */

    /**
     * @brief  全部消息（按时间正序）。
     */
    const QVector<ChatItem_S>& items() const { return m_aItems; }

    /**
     * @brief      按序号查找。
     * @param[in] seq            消息序号；类型 qint64。
     * @return     指针，不存在时返回 nullptr；`modelReset` 后指针失效。
     * @author     JaTin
     */
    ChatItem_S* find(qint64 seq);
    /**
     * @brief      查找 find。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @return     成功返回对象指针；未找到或失败时返回 nullptr。
     * @author     JaTin
     */
    const ChatItem_S* find(qint64 seq) const;

    /**
     * @brief  成员表（id -> 昵称），按 id 有序。
     */
    const QMap<qint64, QString>& members() const { return m_uMembers; }

    /**
     * @brief 判断某条消息是否由本人发出。
     * @param userId 消息里的 `from` 字段。
     * @retval true 本人（含历次重连前使用的 id）。
     */
    bool isSelf(qint64 userId) const { return userId != 0 && m_uSelfIds.contains(userId); }

    /**
     * @brief  当前成员数。
     */
    int memberCount() const { return m_uMembers.size(); }

    /**
     * @brief      取最近若干条非系统消息，用于生成建议回复的上下文。
     * @param[in] limit          最多取几条；类型 int。
     * @return     按时间正序的消息列表。
     * @author     JaTin
     */
    QVector<ChatMessage_S> recentContext(int limit) const;

signals:
    /**
     * @brief      新增一条消息，视图应追加渲染。
     * @param[in] item           条目；类型 const business::ChatItem_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void itemAppended(const business::ChatItem_S& item);

    /**
     * @brief      模型被整体重排或清空，视图应全量重建。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void modelReset();

    /**
     * @brief      某条消息的译文或转写状态变化，视图应局部刷新。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void itemUpdated(qint64 seq);

    /**
     * @brief      成员表变化。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void membersChanged();

private:
    /**
     * @brief      判断并登记「本人旧会话 id」。
     * @details    服务端每次连接都分配新 id，重启客户端后历史里本人的旧消息 会看起来像别人发的。这里用「同名且不在成员表」作为启发式判定。
     * @param[in] from           消息发送者 id；类型 qint64。
     * @param[in] name           消息发送者昵称；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void adoptLegacySelfId(qint64 from, const QString& name);

    /**
     * @brief      按时间戳排序，时间相同按 seq。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sortItems();

    /**
     * @brief      确保成员。
     * @param[in] userId         用户编号；类型 qint64。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void ensureMember(qint64 userId, const QString& name);

    /**
     * @brief  当前房间号
     * @note   类型 QString；类/结构体成员变量。
     */
    QString             m_strRoom;
    /**
     * @brief  当前会话 id
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64              m_uSelfId = 0;
    /**
     * @brief  本人昵称
     * @note   类型 QString；类/结构体成员变量。
     */
    QString             m_strSelfName;
    /**
     * @brief  本人历次 id
     * @note   类型 QSet<qint64>；类/结构体成员变量。
     */
    QSet<qint64>        m_uSelfIds;
    /**
     * @brief  成员表
     * @note   类型 QMap<qint64, QString>；类/结构体成员变量。
     */
    QMap<qint64, QString> m_uMembers;
    /**
     * @brief  消息列表
     * @note   类型 QVector<ChatItem_S>；类/结构体成员变量。
     */
    QVector<ChatItem_S>   m_aItems;
    /**
     * @brief  已收录的序号，用于去重
     * @note   类型 QSet<qint64>；类/结构体成员变量。
     */
    QSet<qint64>        m_uSeqs;
};

/**
 * @brief  namespace business。
 */
}
