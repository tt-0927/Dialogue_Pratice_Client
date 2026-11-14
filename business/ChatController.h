/**
 * @file       ChatController.h
 * @brief      聊天业务编排：翻译、智能建议、语音转写与语音发送。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
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
 * @brief   聊天页的业务编排器。
 * @details 职责边界： - 决定「哪些消息要翻译」并驱动 CTranslationService - 决定「何时请求建议回复」并驱动 CReplyService - 维护语音转写队列（一次只跑一条，避免 whisper 抢占 CPU） - 把录音 PCM 编码成协议容器并请求发送 不负责：渲染、滚动、控件状态、播放。
 * @author  JaTin
 */
class ChatController : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      构造编排器并创建各业务服务。
     * @param[in] model          消息模型，不取得所有权；类型 ChatModel*。
     * @param[in] parent         Qt 父对象；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit ChatController(ChatModel* model, QObject* parent = nullptr);

    ~ChatController() override;

    /**
     * @brief      注入录音器（语音转写需要它）。
     * @param[in] recorder       录音器实例，不取得所有权；可为空表示禁用转写；类型 CVoiceRecorder*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setRecorder(CVoiceRecorder* recorder);

    /**
     * @brief 告知当前输入框是否为空。
     * @param empty true 表示无内容。
     * @details 建议回复只在输入框为空时请求，避免打断用户输入。
     */
    void setComposerEmpty(bool empty) { m_nComposerEmpty = empty; }

    /**
     * @brief      请求生成智能建议回复。
     * @details    取最近 6 条非系统消息作为上下文；已在请求中则忽略。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestSuggestions();

    /**
     * @brief      请求把中文输入翻译成英文（译文卡）。
     * @param[in] chinese        输入框内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestInputTranslation(const QString& chinese);

    /**
     * @brief      把录音 PCM 编码成 Opus 容器并请求发送。
     * @param[in] pcm            16 kHz / 单声道 / 16-bit 原始音频；类型 const QByteArray&。
     * @param[in] durationMs     时长（毫秒）；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendVoice(const QByteArray& pcm, int durationMs);

    /**
     * @brief      应用服务端下发的 LLM 配置。
     * @details    解析成功则覆盖本地文件配置，并停止监听本地文件变化 （否则两者会互相覆盖）。解析失败保持原配置不变。
     * @param[in] json           服务端随 LoginResp 下发的原始 JSON；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void applyRemoteConfig(const QByteArray& json);

    /**
     * @brief      补翻此前失败或未启用的条目。
     * @details    配置热重载后调用：用户填好 Key，历史消息应自动补上译文。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void retryFailedTranslations();

    /**
     * @brief      翻译服务当前是否可用。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool translationEnabled() const;

    /**
     * @brief      建议服务当前是否可用。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool replyEnabled() const;

    /**
     * @brief      语音识别模型是否已就绪。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool transcriptReady() const;

signals:
    /**
     * @brief      需要向用户提示一条消息。
     * @param[in] text           提示文本；类型 const QString&。
     * @param[in] error          true 表示错误（界面用红色样式）；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void notice(const QString& text, bool error);

    /**
     * @brief      语音相关的状态提示（录音中、识别中、已发送等）。
     * @param[in] text           提示文本，空串表示清除；类型 const QString&。
     * @param[in] error          是否错误样式；类型 bool。
     * @param[in] autoHideMs     自动隐藏毫秒数，0 表示不自动隐藏；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void voiceStatus(const QString& text, bool error, int autoHideMs);

    /**
     * @brief      建议回复已生成。
     * @param[in] items          条目列表；类型 const QVector<Suggestion_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void suggestionsReady(const QVector<Suggestion_S>& items);

    /**
     * @brief      建议回复请求失败或取消，界面应收起建议条。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void suggestionsCleared();

    /**
     * @brief      输入翻译完成，界面应显示译文卡。
     * @param[in] source         配置来源路径；类型 const QString&。
     * @param[in] english        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void inputTranslationReady(const QString& source, const QString& english);

    /**
     * @brief      输入翻译失败，界面应收起译文卡。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void inputTranslationFailed(const QString& reason);

    /**
     * @brief      请求把编码好的语音发出去。
     * @param[in] audio          DPV1 容器；类型 const QByteArray&。
     * @param[in] durationMs     时长（毫秒）；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendVoiceRequested(const QByteArray& audio, int durationMs);

private:
    /**
     * @brief      模型新增消息后的编排：决定是否翻译、是否转写、是否请求建议。
     * @param[in] item           新增的消息；类型 const ChatItem_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onItemAppended(const ChatItem_S& item);

    /**
     * @brief      模型整体重建后的编排（历史批量合并、切房间）。
     * @details    历史消息不走 itemAppended，必须在这里补翻译与转写， 否则进房后所有历史气泡都没有译文。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onModelReset();

    /**
     * @brief      从队列取下一条待转写语音。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void pumpTranscript();

    /**
     * @brief      登记一条待转写语音。
     * @param[in] seq            消息序号；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void enqueueTranscript(qint64 seq);

    /**
     * @brief 应用 LLM 配置。
     * @param settings 配置快照。
     * @param initial  是否首次加载（首次的错误提示要延后到事件循环）。
     */
    /**
     * @brief      应用 LLM 配置。
     * @param[in] settings       配置快照；类型 const AppConfig::Settings_S&。
     * @param[in] initial        是否首次加载（首次的错误提示要延后到事件循环）；类型 bool。
     * @param[in] silent         true 时不发「配置已重新加载」的成功提示（登录下发时用）；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void applyConfig(const AppConfig::Settings_S& settings, bool initial, bool silent = false);

    /**
     * @brief      监听配置文件变化并热重载。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void watchConfig();

    /**
     * @brief  消息模型（不持有）
     * @note   类型 ChatModel*；类/结构体成员变量。
     */
    ChatModel*           model_ = nullptr;
    /**
     * @brief  录音器（不持有）
     * @note   类型 CVoiceRecorder*；类/结构体成员变量。
     */
    CVoiceRecorder*       m_pRecorder = nullptr;
    /**
     * @brief  对话/建议模型
     * @note   类型 CLlmClient*；类/结构体成员变量。
     */
    CLlmClient*           m_pLlm = nullptr;
    /**
     * @brief  翻译模型（可与上面不同）
     * @note   类型 CLlmClient*；类/结构体成员变量。
     */
    CLlmClient*           m_pTranslateLlm = nullptr;
    /**
     * @brief  翻译服务
     * @note   类型 CTranslationService*；类/结构体成员变量。
     */
    CTranslationService*  m_pTranslation = nullptr;
    /**
     * @brief  建议回复服务
     * @note   类型 CReplyService*；类/结构体成员变量。
     */
    CReplyService*        m_pReply = nullptr;

    /**
     * @brief  配置热重载
     * @note   类型 QFileSystemWatcher*；类/结构体成员变量。
     */
    QFileSystemWatcher* m_pConfigWatcher = nullptr;
    /**
     * @brief  配置重载防抖
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*             m_pConfigReloadTimer = nullptr;
    /**
     * @brief  建议请求防抖
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*             m_pReplyDebounce = nullptr;
    /**
     * @brief  输入翻译防抖
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*             m_pInputDebounce = nullptr;

    /**
     * @brief  配置错误，进房后再提醒一次
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strConfigError;
    /**
     * @brief  历史消息最多翻多少条
     * @note   类型 int；类/结构体成员变量。
     */
    int     m_nHistoryTranslationBudget = 12;

    /**
     * @brief  待转写队列
     * @note   类型 QQueue<qint64>；类/结构体成员变量。
     */
    QQueue<qint64> m_uTranscriptQueue;
    /**
     * @brief  正在转写的那条（0 = 空闲）
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64         m_uTranscribingSeq = 0;

    /**
     * @brief  输入框是否为空
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    m_nComposerEmpty = true;
    /**
     * @brief  已采用服务端下发的配置（忽略本地文件变化）
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    m_nUsingRemoteConfig = false;
    /**
     * @brief  最近一次请求翻译的输入内容
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingInput;
};

/**
 * @brief  namespace business。
 */
}
