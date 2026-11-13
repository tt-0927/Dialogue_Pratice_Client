/**
 * @file  ChatController.h
 * @brief 聊天业务编排：翻译、智能建议、语音转写与语音发送。
 * @details 从原 ChatPage 中剥离出来的逻辑层。它持有各业务服务并驱动 ChatModel，
 *          但不持有任何控件——界面反应全部通过信号发出。
 *
 *          这样拆的好处：编排逻辑可以脱离界面单独测试，
 *          界面改动不会碰到翻译/建议的触发条件。
 */

#pragma once

#include <QByteArray>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QVector>

#include "business/ChatModel.h"
#include "business/ChatTypes.h"
#include "core/AppConfig.h"

class LlmClient;
class ReplyService;
class TranslationService;
class VoiceRecorder;
class QFileSystemWatcher;
class QTimer;

namespace business {

/**
 * @brief 聊天页的业务编排器。
 *
 * @details 职责边界：
 *          - 决定「哪些消息要翻译」并驱动 TranslationService
 *          - 决定「何时请求建议回复」并驱动 ReplyService
 *          - 维护语音转写队列（一次只跑一条，避免 whisper 抢占 CPU）
 *          - 把录音 PCM 编码成协议容器并请求发送
 *
 *          不负责：渲染、滚动、控件状态、播放。
 */
class ChatController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 构造编排器并创建各业务服务。
     * @param model  消息模型，不取得所有权。
     * @param parent Qt 父对象。
     */
    explicit ChatController(ChatModel* model, QObject* parent = nullptr);

    ~ChatController() override;

    /**
     * @brief 注入录音器（语音转写需要它）。
     * @param recorder 录音器实例，不取得所有权；可为空表示禁用转写。
     */
    void setRecorder(VoiceRecorder* recorder);

    /**
     * @brief 告知当前输入框是否为空。
     * @param empty true 表示无内容。
     * @details 建议回复只在输入框为空时请求，避免打断用户输入。
     */
    void setComposerEmpty(bool empty) { composerEmpty_ = empty; }

    /**
     * @brief 请求生成智能建议回复。
     * @details 取最近 6 条非系统消息作为上下文；已在请求中则忽略。
     */
    void requestSuggestions();

    /**
     * @brief 请求把中文输入翻译成英文（译文卡）。
     * @param chinese 输入框内容。
     */
    void requestInputTranslation(const QString& chinese);

    /**
     * @brief 把录音 PCM 编码成 Opus 容器并请求发送。
     * @param pcm        16 kHz / 单声道 / 16-bit 原始音频。
     * @param durationMs 时长（毫秒）。
     */
    void sendVoice(const QByteArray& pcm, int durationMs);

    /**
     * @brief 应用服务端下发的 LLM 配置。
     * @param json 服务端随 LoginResp 下发的原始 JSON。
     * @details 解析成功则覆盖本地文件配置，并停止监听本地文件变化
     *          （否则两者会互相覆盖）。解析失败保持原配置不变。
     */
    void applyRemoteConfig(const QByteArray& json);

    /**
     * @brief 补翻此前失败或未启用的条目。
     * @details 配置热重载后调用：用户填好 Key，历史消息应自动补上译文。
     */
    void retryFailedTranslations();

    /// @brief 翻译服务当前是否可用。
    bool translationEnabled() const;

    /// @brief 建议服务当前是否可用。
    bool replyEnabled() const;

    /// @brief 语音识别模型是否已就绪。
    bool transcriptReady() const;

signals:
    /**
     * @brief 需要向用户提示一条消息。
     * @param text  提示文本。
     * @param error true 表示错误（界面用红色样式）。
     */
    void notice(const QString& text, bool error);

    /**
     * @brief 语音相关的状态提示（录音中、识别中、已发送等）。
     * @param text       提示文本，空串表示清除。
     * @param error      是否错误样式。
     * @param autoHideMs 自动隐藏毫秒数，0 表示不自动隐藏。
     */
    void voiceStatus(const QString& text, bool error, int autoHideMs);

    /// @brief 建议回复已生成。
    void suggestionsReady(const QVector<Suggestion>& items);

    /// @brief 建议回复请求失败或取消，界面应收起建议条。
    void suggestionsCleared();

    /// @brief 输入翻译完成，界面应显示译文卡。
    void inputTranslationReady(const QString& source, const QString& english);

    /// @brief 输入翻译失败，界面应收起译文卡。
    void inputTranslationFailed(const QString& reason);

    /**
     * @brief 请求把编码好的语音发出去。
     * @param audio      DPV1 容器。
     * @param durationMs 时长（毫秒）。
     */
    void sendVoiceRequested(const QByteArray& audio, int durationMs);

private:
    /**
     * @brief 模型新增消息后的编排：决定是否翻译、是否转写、是否请求建议。
     * @param item 新增的消息。
     */
    void onItemAppended(const ChatItem& item);

    /**
     * @brief 模型整体重建后的编排（历史批量合并、切房间）。
     * @details 历史消息不走 itemAppended，必须在这里补翻译与转写，
     *          否则进房后所有历史气泡都没有译文。
     */
    void onModelReset();

    /// @brief 从队列取下一条待转写语音。
    void pumpTranscript();

    /**
     * @brief 登记一条待转写语音。
     * @param seq 消息序号。
     */
    void enqueueTranscript(qint64 seq);

    /**
     * @brief 应用 LLM 配置。
     * @param settings 配置快照。
     * @param initial  是否首次加载（首次的错误提示要延后到事件循环）。
     */
    /**
     * @brief 应用 LLM 配置。
     * @param settings 配置快照。
     * @param initial  是否首次加载（首次的错误提示要延后到事件循环）。
     * @param silent   true 时不发「配置已重新加载」的成功提示（登录下发时用）。
     */
    void applyConfig(const AppConfig::Settings& settings, bool initial, bool silent = false);

    /// @brief 监听配置文件变化并热重载。
    void watchConfig();

    ChatModel*           model_ = nullptr;        ///< 消息模型（不持有）
    VoiceRecorder*       recorder_ = nullptr;     ///< 录音器（不持有）
    LlmClient*           llm_ = nullptr;          ///< 对话/建议模型
    LlmClient*           translateLlm_ = nullptr; ///< 翻译模型（可与上面不同）
    TranslationService*  translation_ = nullptr;  ///< 翻译服务
    ReplyService*        reply_ = nullptr;        ///< 建议回复服务

    QFileSystemWatcher* configWatcher_ = nullptr;      ///< 配置热重载
    QTimer*             configReloadTimer_ = nullptr;  ///< 配置重载防抖
    QTimer*             replyDebounce_ = nullptr;      ///< 建议请求防抖
    QTimer*             inputDebounce_ = nullptr;      ///< 输入翻译防抖

    QString configError_;                ///< 配置错误，进房后再提醒一次
    int     historyTranslationBudget_ = 12; ///< 历史消息最多翻多少条

    QQueue<qint64> transcriptQueue_;     ///< 待转写队列
    qint64         transcribingSeq_ = 0; ///< 正在转写的那条（0 = 空闲）

    bool    composerEmpty_ = true;       ///< 输入框是否为空
    bool    usingRemoteConfig_ = false;  ///< 已采用服务端下发的配置（忽略本地文件变化）
    QString pendingInput_;               ///< 最近一次请求翻译的输入内容
};

}  // namespace business
