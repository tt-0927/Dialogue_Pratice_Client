/**
 * @file  ChatPage.h
 * @brief 聊天界面（纯视图）。
 * @details 重构后本类只负责三件事：
 *          1. 搭建与维护控件；
 *          2. 监听 ChatModel / ChatController 的信号并刷新界面；
 *          3. 把用户操作转发出去（发消息、录音、播放）。
 *
 *          数据在 `business::ChatModel`，业务编排在 `business::ChatController`，
 *          本类不含任何翻译/建议/转写的触发逻辑。
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

class Avatar;
class MessageBubble;
class MicButton;
class QFrame;
class QLabel;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class SendButton;
class SuggestionBar;
class TranslationCard;
class TtsService;
class VoiceRecorder;

namespace business {
class ChatController;
class ChatModel;
struct ChatItem;
}  // namespace business

/**
 * @brief 输入框：Enter 发送，Shift+Enter 换行。
 */
class ComposerEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit ComposerEdit(QWidget* parent = nullptr);

signals:
    /// @brief 用户按下 Enter（未按 Shift）。
    void submit();

protected:
    /// @brief 拦截 Enter，其余键交给基类处理。
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

    // ---- 由 MainWindow 驱动的生命周期与消息投递 ----

    /**
     * @brief 首次进入房间：清空消息区并显示入房提示。
     * @param room     房间号。
     * @param created  房间是否为本次新建。
     * @param members  成员列表。
     * @param selfId   本人会话 id。
     * @param selfName 本人昵称。
     */
    void enterRoom(const QString& room, bool created, const QVector<MemberInfo>& members,
                   qint64 selfId, const QString& selfName);

    /**
     * @brief 断线重连后回到原房间：保留已有消息，只刷新成员与连接状态。
     * @param room     房间号。
     * @param members  最新成员列表。
     * @param selfId   新分配的会话 id。
     * @param selfName 本人昵称。
     */
    void resumeRoom(const QString& room, const QVector<MemberInfo>& members,
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
    void mergeHistory(const QVector<ChatMessage>& messages);

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

    /// @brief 清空房间数据（离开房间或登出）。
    void clearRoom();

    /**
     * @brief 应用服务端下发的 LLM 配置。
     * @param json 服务端随 LoginResp 下发的原始 JSON。
     * @details 由 MainWindow 在登录成功后转发；内部交给 ChatController 处理。
     */
    void applyRemoteConfig(const QByteArray& json);

    /// @brief 当前房间号，空串表示不在房间。
    QString room() const;

signals:
    /// @brief 用户点击返回箭头。
    void backRequested();

    /// @brief 用户要发送文本。
    void sendRequested(const QString& text);

    /// @brief 用户录完一段语音，请求发送。
    void sendVoiceRequested(const QByteArray& audio, int durationMs);

    /// @brief 需要主窗口弹出提示条。
    void notice(const QString& text, bool error);

protected:
    /// @brief 宽度变化时刷新响应式元素（占位文案等）。
    void resizeEvent(QResizeEvent* event) override;

private:
    // ---- 构建 ----
    void buildHeader(QVBoxLayout* columnLayout);
    void buildMessageArea(QVBoxLayout* columnLayout);
    void buildComposer(QVBoxLayout* columnLayout);
    void buildMemberPanel();
    void wireSignals();

    // ---- 渲染 ----
    /**
     * @brief 渲染一条消息（含日期分隔）。
     * @param item 消息数据。
     */
    void renderItem(const business::ChatItem& item);

    /// @brief 清空视图后按模型全量重画。
    void rebuildView();

    /// @brief 删除所有消息控件（保留底部弹簧）。
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

    /// @brief 滚到底部（布局结算后可能再触发一次）。
    void scrollToBottom();

    /// @brief 立即滚到底部。
    void forceScrollToBottom();

    /**
     * @brief 把控件高度钉死为它的 sizeHint。
     * @param widget 刚加入消息区布局的控件。
     * @details QVBoxLayout 会把子控件压进可用高度，导致 canvas 不增长、
     *          滚动条 maximum 恒为 0。钉死高度后 canvas 只能随内容变高。
     */
    static void pinHeight(QWidget* widget);

    // ---- 交互 ----
    /// @brief 读取输入框并发出 sendRequested。
    void sendCurrentText();

    /// @brief 开始或结束录音。
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

    /// @brief 重建右侧成员列表。
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

    /// @brief 刷新顶栏的成员计数。
    void updateHeader();

    /// @brief 通知编排器输入框当前是否为空。
    void syncComposerState();

    /// @brief 按当前宽度更新输入框占位文案。
    void updateComposerPlaceholder();

    // ---- 数据 ----
    business::ChatModel*      model_ = nullptr;       ///< 消息模型（持有）
    business::ChatController* controller_ = nullptr;  ///< 业务编排（持有）

    // ---- 顶栏 ----
    Avatar*      roomAvatar_ = nullptr;
    QLabel*      roomTitle_ = nullptr;
    QLabel*      roomSubtitle_ = nullptr;
    QPushButton* memberBtn_ = nullptr;

    // ---- 消息区 ----
    QScrollArea* scroll_ = nullptr;
    QWidget*     canvas_ = nullptr;
    QVBoxLayout* chatLayout_ = nullptr;
    QHash<qint64, MessageBubble*> bubbles_;   ///< seq -> 气泡，用于局部刷新
    QPointer<MessageBubble> playingBubble_;   ///< 正在播放 TTS 的气泡
    QDate        lastDate_;                   ///< 最后渲染的日期，用于分隔条
    bool         pendingScroll_ = false;      ///< 是否仍处于「贴底」状态
    bool         stickToBottom_ = true;       ///< 用户是否停在底部（滚动条到底）
    QTimer*      scrollSettleTimer_ = nullptr; ///< 贴底状态的结算计时（每次滚动都重置）

    // ---- 输入区 ----
    ComposerEdit*    composer_ = nullptr;
    SendButton*      sendBtn_ = nullptr;
    MicButton*       micBtn_ = nullptr;
    QLabel*          voiceStatus_ = nullptr;
    SuggestionBar*   suggestionBar_ = nullptr;
    TranslationCard* translationCard_ = nullptr;
    int              voiceStatusToken_ = 0;   ///< 防止旧的自动隐藏计时误关新提示

    // ---- 服务 ----
    VoiceRecorder* voice_ = nullptr;          ///< 录音与本地识别
    TtsService*    tts_ = nullptr;            ///< 语音合成
    bool           voiceAvailable_ = true;    ///< 识别模型是否可用

    // ---- 成员面板 ----
    QFrame*      memberPanel_ = nullptr;
    QVBoxLayout* memberListLayout_ = nullptr;
    QLabel*      memberTitle_ = nullptr;
};
