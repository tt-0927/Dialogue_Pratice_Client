/*
 * 文件名:    ChatPage.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  聊天界面（纯视图）。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#pragma once

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QPlainTextEdit>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "business/ChatTypes.h"

class CAvatar;
class CMessageBubble;
class CMicButton;
class QFrame;
class QLabel;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class CSendButton;
class CSuggestionBar;
class CTranslationCard;
class CTtsService;
class CVoiceRecorder;

namespace business {
class ChatController;
class ChatModel;
struct ChatItem_S;
/* namespace business */
}

/**
 * @brief 输入框：Enter 发送，Shift+Enter 换行。
 */
class ComposerEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit ComposerEdit(QWidget* parent = nullptr);

signals:
    /* 用户按下 Enter（未按 Shift）。 */
    void submit();

protected:
    /* 拦截 Enter，其余键交给基类处理。 */
    void keyPressEvent(QKeyEvent* event) override;
};

/**
 * @brief 聊天页。
 *
 * @details 布局分三块：顶栏（返回 / 房间信息 / 成员按钮）、消息滚动区、
 *          底部输入区（建议条、译文卡、状态条、输入框）。
 *          右侧是可选成员面板。
 */
class ChatPage : public QWidget {
    Q_OBJECT

public:
    explicit ChatPage(QWidget* parent = nullptr);
    ~ChatPage() override;

    /* ---- 由 CMainWindow 驱动的生命周期与消息投递 ---- */

    /**
     * @brief 首次进入房间：清空消息区并显示入房提示。
     * @param room     房间号。
     * @param created  房间是否为本次新建。
     * @param members  成员列表。
     * @param selfId   本人会话 id。
     * @param selfName 本人昵称。
     */
    void enterRoom(const QString& room, bool created, const QVector<MemberInfo_S>& members,
                   qint64 selfId, const QString& selfName);

    /**
     * @brief 断线重连后回到原房间：保留已有消息，只刷新成员与连接状态。
     * @param room     房间号。
     * @param members  最新成员列表。
     * @param selfId   新分配的会话 id。
     * @param selfName 本人昵称。
     */
    void resumeRoom(const QString& room, const QVector<MemberInfo_S>& members,
                    qint64 selfId, const QString& selfName);

    /**
     * @brief 追加一条文本消息。
     * @param seq  消息序号。
     * @param from 发送者 id。
     * @param name 发送者昵称。
     * @param text 正文。
     * @param ts   时间戳。
     */
    void appendChat(qint64 seq, qint64 from, const QString& name, const QString& text, qint64 ts);

    /**
     * @brief 追加一条语音消息。
     * @param seq        消息序号。
     * @param from       发送者 id。
     * @param name       发送者昵称。
     * @param audio      DPV1 容器。
     * @param durationMs 时长。
     * @param ts         时间戳。
     */
    void appendVoice(qint64 seq, qint64 from, const QString& name, const QByteArray& audio,
                     int durationMs, qint64 ts);

    /**
     * @brief 批量合并历史消息。
     * @param messages 历史消息列表。
     */
    void mergeHistory(const QVector<ChatMessage_S>& messages);

    /**
     * @brief 追加一条系统提示。
     * @param text 提示文本。
     * @param ts   时间戳。
     */
    void appendSystemText(const QString& text, qint64 ts);

    /**
     * @brief 应用一条系统事件并刷新成员列表。
     * @param event   事件名。
     * @param userId  相关用户 id。
     * @param name    用户昵称。
     * @param oldName 旧昵称（rename 事件用，可为空）。
     * @param text    提示文本。
     * @param ts      时间戳。
     */
    void onSystemEvent(const QString& event, qint64 userId, const QString& name,
                       const QString& oldName, const QString& text, qint64 ts);

    /**
     * @brief 切换在线状态。
     * @param online false 时禁用输入并提示正在重连。
     */
    void setOnline(bool online);

    /* 清空房间数据（离开房间或登出）。 */
    void clearRoom();

    /**
     * @brief 应用服务端下发的 LLM 配置。
     * @param json 服务端随 LoginResp 下发的原始 JSON。
     * @details 由 CMainWindow 在登录成功后转发；内部交给 ChatController 处理。
     */
    void applyRemoteConfig(const QByteArray& json);

    /* 当前房间号，空串表示不在房间。 */
    QString room() const;

signals:
    /* 用户点击返回箭头。 */
    void backRequested();

    /* 用户要发送文本。 */
    void sendRequested(const QString& text);

    /* 用户录完一段语音，请求发送。 */
    void sendVoiceRequested(const QByteArray& audio, int durationMs);

    /* 需要主窗口弹出提示条。 */
    void notice(const QString& text, bool error);

protected:
    /* 宽度变化时刷新响应式元素（占位文案等）。 */
    void resizeEvent(QResizeEvent* event) override;

private:
    /* ---- 构建 ---- */
    void buildHeader(QVBoxLayout* columnLayout);
    void buildMessageArea(QVBoxLayout* columnLayout);
    void buildComposer(QVBoxLayout* columnLayout);
    void buildMemberPanel();
    void wireSignals();

    /* ---- 渲染 ---- */
    /**
     * @brief 渲染一条消息（含日期分隔）。
     * @param item 消息数据。
     */
    void renderItem(const business::ChatItem_S& item);

    /* 清空视图后按模型全量重画。 */
    void rebuildView();

    /* 删除所有消息控件（保留底部弹簧）。 */
    void clearView();

    /**
     * @brief 某条消息状态变化时刷新对应气泡。
     * @param seq 消息序号。
     */
    void refreshBubble(qint64 seq);

    /**
     * @brief 时间戳跨天时插入日期分隔条。
     * @param ts 时间戳。
     */
    void appendDateSeparatorIfNeeded(qint64 ts);

    /* 滚到底部（布局结算后可能再触发一次）。 */
    void scrollToBottom();

    /* 立即滚到底部。 */
    void forceScrollToBottom();

    /**
     * @brief 把控件高度钉死为它的 sizeHint。
     * @param widget 刚加入消息区布局的控件。
     * @details QVBoxLayout 会把子控件压进可用高度，导致 canvas 不增长、
     *          滚动条 maximum 恒为 0。钉死高度后 canvas 只能随内容变高。
     */
    static void pinHeight(QWidget* widget);

    /* ---- 交互 ---- */
    /* 读取输入框并发出 sendRequested。 */
    void sendCurrentText();

    /* 开始或结束录音。 */
    void toggleRecording();

    /**
     * @brief 录音结束，交给编排器编码发送。
     * @param pcm        原始音频。
     * @param durationMs 时长。
     */
    void onVoiceRecorded(const QByteArray& pcm, int durationMs);

    /**
     * @brief 显示或清除底部状态提示。
     * @param text       文本，空串表示清除。
     * @param error      是否错误样式。
     * @param autoHideMs 自动隐藏毫秒数，0 表示不自动隐藏。
     */
    void showVoiceStatus(const QString& text, bool error, int autoHideMs);

    /* 重建右侧成员列表。 */
    void rebuildMembers();

    /**
     * @brief 窄屏下的成员列表弹窗。
     * @details 手机竖屏没有横向空间放 220px 侧栏，改成模态弹窗。
     *          与侧栏共用 `createMemberRow()`，避免两处渲染逻辑漂移。
     */
    void showMemberDialog();

    /**
     * @brief 构造一行成员（头像 + 昵称 + 「我」标记）。
     * @param userId 成员 id。
     * @param name   昵称。
     * @retval 未挂到任何布局的新控件，调用方负责加入布局。
     */
    QWidget* createMemberRow(qint64 userId, const QString& name);

    /* 刷新顶栏的成员计数。 */
    void updateHeader();

    /* 通知编排器输入框当前是否为空。 */
    void syncComposerState();

    /* 按当前宽度更新输入框占位文案。 */
    void updateComposerPlaceholder();

    /* ---- 数据 ---- */
    /* < 消息模型（持有） */
    business::ChatModel*      model_ = nullptr;
    /* < 业务编排（持有） */
    business::ChatController* m_pController = nullptr;

    /* ---- 顶栏 ---- */
    CAvatar*      m_pRoomAvatar = nullptr;
    QLabel*      m_pRoomTitle = nullptr;
    QLabel*      m_pRoomSubtitle = nullptr;
    QPushButton* m_pMemberBtn = nullptr;

    /* ---- 消息区 ---- */
    QScrollArea* m_pScroll = nullptr;
    QWidget*     m_pCanvas = nullptr;
    QVBoxLayout* m_pChatLayout = nullptr;
    /* < seq -> 气泡，用于局部刷新 */
    QHash<qint64, CMessageBubble*> m_uBubbles;
    /* < 正在播放 TTS 的气泡 */
    QPointer<CMessageBubble> m_stPlayingBubble;
    /* < 最后渲染的日期，用于分隔条 */
    QDate        m_stLastDate;
    /* < 是否仍处于「贴底」状态 */
    bool         m_nPendingScroll = false;
    /* < 用户是否停在底部（滚动条到底） */
    bool         m_nStickToBottom = true;
    /* < 贴底状态的结算计时（每次滚动都重置） */
    QTimer*      m_pScrollSettleTimer = nullptr;

    /* ---- 输入区 ---- */
    ComposerEdit*    m_pComposer = nullptr;
    CSendButton*      m_pSendBtn = nullptr;
    CMicButton*       m_pMicBtn = nullptr;
    QLabel*          m_pVoiceStatus = nullptr;
    CSuggestionBar*   m_pSuggestionBar = nullptr;
    CTranslationCard* m_pTranslationCard = nullptr;
    /* < 防止旧的自动隐藏计时误关新提示 */
    int              m_nVoiceStatusToken = 0;

    /* ---- 服务 ---- */
    /* < 录音与本地识别 */
    CVoiceRecorder* m_pVoice = nullptr;
    /* < 语音合成 */
    CTtsService*    m_pTts = nullptr;
    /* < 识别模型是否可用 */
    bool           m_nVoiceAvailable = true;

    /* ---- 成员面板 ---- */
    QFrame*      m_pMemberPanel = nullptr;
    QVBoxLayout* m_pMemberListLayout = nullptr;
    QLabel*      m_pMemberTitle = nullptr;
};
