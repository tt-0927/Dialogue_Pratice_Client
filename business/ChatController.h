/*
 * 文件名:    ChatController.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  聊天业务编排：翻译、智能建议、语音转写与语音发送。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
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

class CLlmClient;
class CReplyService;
class CTranslationService;
class CVoiceRecorder;
class QFileSystemWatcher;
class QTimer;

namespace business {

/**
 * @brief 聊天页的业务编排器。
 *
 * @details 职责边界：
 *          - 决定「哪些消息要翻译」并驱动 CTranslationService
 *          - 决定「何时请求建议回复」并驱动 CReplyService
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
    void setRecorder(CVoiceRecorder* recorder);

    /**
     * @brief 告知当前输入框是否为空。
     * @param empty true 表示无内容。
     * @details 建议回复只在输入框为空时请求，避免打断用户输入。
     */
    void setComposerEmpty(bool empty) { m_nComposerEmpty = empty; }

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

    /* 翻译服务当前是否可用。 */
    bool translationEnabled() const;

    /* 建议服务当前是否可用。 */
    bool replyEnabled() const;

    /* 语音识别模型是否已就绪。 */
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

    /* 建议回复已生成。 */
    void suggestionsReady(const QVector<Suggestion_S>& items);

    /* 建议回复请求失败或取消，界面应收起建议条。 */
    void suggestionsCleared();

    /* 输入翻译完成，界面应显示译文卡。 */
    void inputTranslationReady(const QString& source, const QString& english);

    /* 输入翻译失败，界面应收起译文卡。 */
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
    void onItemAppended(const ChatItem_S& item);

    /**
     * @brief 模型整体重建后的编排（历史批量合并、切房间）。
     * @details 历史消息不走 itemAppended，必须在这里补翻译与转写，
     *          否则进房后所有历史气泡都没有译文。
     */
    void onModelReset();

    /* 从队列取下一条待转写语音。 */
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
    void applyConfig(const AppConfig::Settings_S& settings, bool initial, bool silent = false);

    /* 监听配置文件变化并热重载。 */
    void watchConfig();

    /* < 消息模型（不持有） */
    ChatModel*           model_ = nullptr;
    /* < 录音器（不持有） */
    CVoiceRecorder*       m_pRecorder = nullptr;
    /* < 对话/建议模型 */
    CLlmClient*           m_pLlm = nullptr;
    /* < 翻译模型（可与上面不同） */
    CLlmClient*           m_pTranslateLlm = nullptr;
    /* < 翻译服务 */
    CTranslationService*  m_pTranslation = nullptr;
    /* < 建议回复服务 */
    CReplyService*        m_pReply = nullptr;

    /* < 配置热重载 */
    QFileSystemWatcher* m_pConfigWatcher = nullptr;
    /* < 配置重载防抖 */
    QTimer*             m_pConfigReloadTimer = nullptr;
    /* < 建议请求防抖 */
    QTimer*             m_pReplyDebounce = nullptr;
    /* < 输入翻译防抖 */
    QTimer*             m_pInputDebounce = nullptr;

    /* < 配置错误，进房后再提醒一次 */
    QString m_strConfigError;
    /* < 历史消息最多翻多少条 */
    int     m_nHistoryTranslationBudget = 12;

    /* < 待转写队列 */
    QQueue<qint64> m_uTranscriptQueue;
    /* < 正在转写的那条（0 = 空闲） */
    qint64         m_uTranscribingSeq = 0;

    /* < 输入框是否为空 */
    bool    m_nComposerEmpty = true;
    /* < 已采用服务端下发的配置（忽略本地文件变化） */
    bool    m_nUsingRemoteConfig = false;
    /* < 最近一次请求翻译的输入内容 */
    QString m_strPendingInput;
};

/* namespace business */
}
