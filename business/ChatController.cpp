/*
 * 文件名:    ChatController.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  聊天业务编排器的实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#include "business/ChatController.h"

#include "common/Protocol.h"

#include "business/LlmClient.h"
#include "business/ReplyService.h"
#include "business/TranslationService.h"
#include "core/Logger.h"
#include "media/VoiceCodec.h"
#include "media/VoiceRecorder.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QStandardPaths>
#include <QTimer>

namespace business {
namespace {

const QString kTag = QStringLiteral("chat");

/* 建议请求的防抖间隔：等待对方可能连续发来的多条消息。 */
constexpr int kReplyDebounceMs = 800;
/* 输入翻译的防抖间隔。 */
constexpr int kInputDebounceMs = 500;
/* 历史消息里最多转写几条语音，避免进房时一次跑太多 whisper。 */
constexpr int kHistoryVoiceBudget = 3;

/**
 * @brief 把 AppConfig 的 LLM 配置转成 CLlmClient 的配置。
 * @param source 配置快照。
 * @retval 客户端可直接应用的配置。
 */
CLlmClient::Config_S toClientConfig(const AppConfig::LlmSettings_S& source) {
    CLlmClient::Config_S cfg;
    cfg.baseUrl = source.baseUrl;
    cfg.apiKey = source.apiKey;
    cfg.model = source.model;
    cfg.temperature = source.temperature;
    cfg.reasoningEffort = source.reasoningEffort;
    cfg.timeoutMs = source.timeoutMs;
    cfg.enabled = source.enabled;
    return cfg;
}

/* namespace */
}

ChatController::ChatController(ChatModel* model, QObject* parent)
    : QObject(parent), model_(model) {
    /* 两个独立客户端：翻译可以单独指定专用模型（如 Hunyuan-MT）， */
    /* 对话/智能建议用通用模型（如 Qwen），各发挥优势。 */
    m_pLlm = new CLlmClient(this);
    m_pTranslateLlm = new CLlmClient(this);

    m_pTranslation = new CTranslationService(this);
    m_pTranslation->setClient(m_pTranslateLlm);

    m_pReply = new CReplyService(this);
    m_pReply->setClient(m_pLlm);

    /* ---- 消息 -> 编排 ---- */
    if (model_ != nullptr) {
        connect(model_, &ChatModel::itemAppended, this, &ChatController::onItemAppended);
        connect(model_, &ChatModel::modelReset, this, &ChatController::onModelReset);
    }

    /* ---- 翻译结果回填模型 ---- */
    connect(m_pTranslation, &CTranslationService::messageTranslated, this,
            [this](qint64 seq, const QString& chinese) {
                if (model_ != nullptr) model_->setTranslation(seq, chinese);
            });
    connect(m_pTranslation, &CTranslationService::inputTranslated, this,
            [this](const QString& source, const QString& english) {
                /* 用户已经改了输入内容就丢弃这次结果 */
                if (source != m_strPendingInput) return;
                emit inputTranslationReady(source, english);
            });
    connect(m_pTranslation, &CTranslationService::failed, this,
            [this](qint64 seq, const QString& reason) {
                if (seq <= 0) {
                    /* 输入框的中文翻译失败：卡片不能一直停在「翻译中…」 */
                    emit inputTranslationFailed(reason);
                    return;
                }
                if (model_ != nullptr) model_->setTranslationState(seq, 3);
            });

    /* ---- 建议回复 ---- */
    connect(m_pReply, &CReplyService::suggestionsReady, this,
            [this](const QVector<Suggestion_S>& items) {
                /* 用户已经在打字了 */
                if (!m_nComposerEmpty) return;
                emit suggestionsReady(items);
            });
    connect(m_pReply, &CReplyService::failed, this, [this](const QString&) {
        emit suggestionsCleared();
    });

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pReplyDebounce = new QTimer(this);
    m_pReplyDebounce->setSingleShot(true);
    m_pReplyDebounce->setInterval(kReplyDebounceMs);
    connect(m_pReplyDebounce, &QTimer::timeout, this, &ChatController::requestSuggestions);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pInputDebounce = new QTimer(this);
    m_pInputDebounce->setSingleShot(true);
    m_pInputDebounce->setInterval(kInputDebounceMs);
    connect(m_pInputDebounce, &QTimer::timeout, this, [this] {
        if (m_strPendingInput.isEmpty()) return;
        if (m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;
        m_pTranslation->requestInput(m_strPendingInput);
    });

    /* ---- 配置加载与热重载 ---- */
    const QString configPath = AppConfig::resolvePath();
    /* 记录「进来时本地到底有没有配置」：ensureTemplate 会补一份空模板， */
    /* 那份模板不代表用户配过东西，不能因此弹「没填 apiKey」的错误。 */
    const bool hadLocalConfig = !configPath.isEmpty() && QFileInfo::exists(configPath);
    AppConfig::ensureTemplate(configPath);

    AppConfig::Settings_S settings = AppConfig::load();
    if (!hadLocalConfig) {
        /* 本次刚生成的模板：视为「未配置」，等登录后服务端下发； */
        /* 若服务端也没有，翻译功能静默不可用（不打扰用户）。 */
        settings.missing = true;
        settings.error.clear();
    }
    applyConfig(settings, true);
    watchConfig();
}

ChatController::~ChatController() = default;

/*
 * 功能描述:  执行 watchConfig 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatController::watchConfig() {
    const QString configPath = AppConfig::resolvePath();
    if (configPath.isEmpty()) return;

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pConfigWatcher = new QFileSystemWatcher(this);
    m_pConfigWatcher->addPath(configPath);
    m_pConfigWatcher->addPath(QFileInfo(configPath).absolutePath());

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pConfigReloadTimer = new QTimer(this);
    m_pConfigReloadTimer->setSingleShot(true);
    m_pConfigReloadTimer->setInterval(400);
    connect(m_pConfigReloadTimer, &QTimer::timeout, this, [this] {
        /* 已采用服务端下发的配置：本地文件不再生效，否则两者会互相覆盖 */
        if (m_nUsingRemoteConfig) return;
        applyConfig(AppConfig::load(), false);
    });

    connect(m_pConfigWatcher, &QFileSystemWatcher::fileChanged, this,
            [this] { m_pConfigReloadTimer->start(); });
    connect(m_pConfigWatcher, &QFileSystemWatcher::directoryChanged, this,
            [this] { m_pConfigReloadTimer->start(); });
}

/*
 * 功能描述:  执行 applyRemoteConfig 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  json  参数。
 * return:    void
 */
void ChatController::applyRemoteConfig(const QByteArray& json) {
    if (json.isEmpty()) return;

    const AppConfig::Settings_S settings = AppConfig::parse(json, QStringLiteral("<server>"));

    if (!settings.loaded) {
        /* 下发内容有问题时保持原配置，不要因为服务端配错就把本地也弄坏 */
        DP_LOG_WARN(kTag, QStringLiteral("服务端下发的 LLM 配置无效：%1").arg(settings.error));
        return;
    }

    m_nUsingRemoteConfig = true;
    applyConfig(settings, false, /*silent=*/true);

    const QString translateModel = settings.translation.hasOwnLlm
                                       ? settings.translation.llm.model
                                       : settings.llm.model;
    DP_LOG_INFO(kTag, QStringLiteral("已应用服务端下发的配置：对话 %1 / 翻译 %2")
                          .arg(settings.llm.model, translateModel));
}

/*
 * 功能描述:  执行 applyConfig 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  settings  参数。
 * param [in]  initial  参数。
 * param [in]  silent  参数。
 * return:    void
 */
void ChatController::applyConfig(const AppConfig::Settings_S& settings, bool initial, bool silent) {
    /* 对话/建议用主模型 */
    m_pLlm->setConfig(toClientConfig(settings.llm));

    /* 翻译：配了专用模型就用它，否则沿用主模型参数 */
    const bool separate = settings.translation.hasOwnLlm;
    m_pTranslateLlm->setConfig(toClientConfig(separate ? settings.translation.llm : settings.llm));

    m_nHistoryTranslationBudget = qMax(0, settings.translation.maxHistoryMessages);
    m_strConfigError = settings.error;

    if (settings.missing) {
        /* 本地没有配置：等登录后服务端下发；若最终也没有，翻译功能静默不可用 */
        DP_LOG_WARN(kTag, QStringLiteral("本地无 LLM 配置（%1），等待服务端下发")
                              .arg(settings.path));
    }

    if (!settings.error.isEmpty() && !settings.missing) {
        if (initial) {
            /* 构造期发出会丢（外部尚未 connect），推到事件循环第一轮 */
            const QString message = settings.error;
            QTimer::singleShot(0, this, [this, message] { emit notice(message, true); });
        } else {
            emit notice(settings.error, true);
        }
    } else if (!initial && !silent) {
        const QString translateModel = separate ? settings.translation.llm.model
                                                : settings.llm.model;
        emit notice(QStringLiteral("配置已重新加载：对话 %1 / 翻译 %2")
                        .arg(settings.llm.model, translateModel),
                    false);
    }

    /* 之前因为没配 Key 而失败的条目，填好之后自动补翻 */
    if (m_pTranslateLlm->isEnabled()) retryFailedTranslations();
}

/* ---------------------------------------------------------------- 录音器 */

/*
 * 功能描述:  执行 setRecorder 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  recorder  参数。
 * return:    void
 */
void ChatController::setRecorder(CVoiceRecorder* recorder) {
    if (m_pRecorder == recorder) return;
    m_pRecorder = recorder;
    if (m_pRecorder == nullptr) return;

    /* 转写结果：回填模型后接着翻中文 */
    connect(m_pRecorder, &CVoiceRecorder::textReady, this,
            [this](const QString& text, qint64 elapsedMs) {
                if (m_uTranscribingSeq > 0) {
                    const qint64 seq = m_uTranscribingSeq;
                    m_uTranscribingSeq = 0;

                    model_->setTranscript(seq, text, true);
                    /* 清掉「识别中…」 */
                    emit voiceStatus(QString(), false, 0);

                    /* 转写出来的是英文，接着走翻译 */
                    const ChatItem_S* item = model_->find(seq);
                    if (item != nullptr && translationEnabled()
                        && CTranslationService::looksEnglish(text)) {
                        model_->setTranslationState(seq, 1);
                        m_pTranslation->requestMessage(seq, text);
                    }
                    pumpTranscript();
                    return;
                }

                /* 兜底路径（--voice-file 诊断等） */
                emit voiceStatus(QStringLiteral("已识别（%1 ms）").arg(elapsedMs), false, 2500);
            });

    connect(m_pRecorder, &CVoiceRecorder::failed, this, [this](const QString& message) {
        if (m_uTranscribingSeq > 0) {
            const qint64 seq = m_uTranscribingSeq;
            m_uTranscribingSeq = 0;
            model_->setTranscript(seq, QString(), false);
            emit voiceStatus(QString(), false, 0);
            pumpTranscript();
        }
        emit voiceStatus(message, true, 3500);
    });
}

/*
 * 功能描述:  执行 transcriptReady 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool ChatController::transcriptReady() const {
    return m_pRecorder != nullptr && m_pRecorder->modelReady();
}

/* ---------------------------------------------------------------- 编排 */

/*
 * 功能描述:  执行 onModelReset 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatController::onModelReset() {
    if (model_ == nullptr) return;

    const QVector<ChatItem_S>& items = model_->items();

    /* ---- 历史文本：只翻最近 N 条，避免一进房排队几十个请求 ---- */
    if (translationEnabled()) {
        QVector<QPair<qint64, QString>> pending;
        int budget = m_nHistoryTranslationBudget;

        for (int i = items.size() - 1; i >= 0 && budget > 0; --i) {
            const ChatItem_S& item = items.at(i);
            if (item.system || item.kind != MessageKind_EN::Text) continue;
            if (item.translationState != 0) continue;
            if (model_->isSelf(item.from)) continue;
            if (!CTranslationService::looksEnglish(item.text)) continue;

            pending.prepend(qMakePair(item.seq, item.text));
            --budget;
        }

        for (const auto& pair : pending) {
            model_->setTranslationState(pair.first, 1);
            m_pTranslation->requestMessage(pair.first, pair.second);
        }
        if (!pending.isEmpty()) {
            DP_LOG_DEBUG(kTag, QStringLiteral("history: %1 message(s) queued for translation")
                                    .arg(pending.size()));
        }
    }

    /* ---- 历史语音：只转写最近 3 条，避免一次跑太多 whisper ---- */
    int voiceBudget = kHistoryVoiceBudget;
    for (int i = items.size() - 1; i >= 0 && voiceBudget > 0; --i) {
        const ChatItem_S& item = items.at(i);
        if (item.kind != MessageKind_EN::Voice || item.transcriptState != 0) continue;
        enqueueTranscript(item.seq);
        --voiceBudget;
    }
}

/*
 * 功能描述:  执行 onItemAppended 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  item  参数。
 * return:    void
 */
void ChatController::onItemAppended(const ChatItem_S& item) {
    if (item.system) return;

    const bool incoming = !model_->isSelf(item.from);
    DP_LOG_DEBUG(kTag, QStringLiteral("item seq=%1 from=%2 kind=%3 incoming=%4 "
                                      "translate=%5 reply=%6")
                           .arg(item.seq)
                           .arg(item.from)
                           .arg(item.kind == MessageKind_EN::Voice ? "voice" : "text")
                           .arg(incoming)
                           .arg(translationEnabled())
                           .arg(replyEnabled()));

    /* ---- 文本：他人英文消息需要翻译 ---- */
    if (item.kind == MessageKind_EN::Text && incoming && translationEnabled()
        && CTranslationService::looksEnglish(item.text)) {
        model_->setTranslationState(item.seq, 1);
        m_pTranslation->requestMessage(item.seq, item.text);
    }

    /* ---- 语音：先本地转写，转写完再翻 ---- */
    if (item.kind == MessageKind_EN::Voice) {
        enqueueTranscript(item.seq);
    }

    /* ---- 他人消息到达且输入框为空 -> 稍后生成建议 ---- */
    if (incoming && replyEnabled() && m_nComposerEmpty) {
        m_pReplyDebounce->start();
    }
}

/*
 * 功能描述:  执行 requestSuggestions 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatController::requestSuggestions() {
    if (model_ == nullptr || m_pReply == nullptr || !m_pReply->isEnabled()) return;

    const QVector<ChatMessage_S> context = model_->recentContext(6);
    if (context.isEmpty()) return;

    m_pReply->request(context);
}

/*
 * 功能描述:  执行 requestInputTranslation 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  chinese  参数。
 * return:    void
 */
void ChatController::requestInputTranslation(const QString& chinese) {
    m_strPendingInput = chinese.trimmed();
    if (m_strPendingInput.isEmpty()) return;
    if (m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    m_pInputDebounce->start();
}

/*
 * 功能描述:  执行 sendVoice 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  pcm  参数。
 * param [in]  durationMs  参数。
 * return:    void
 */
void ChatController::sendVoice(const QByteArray& pcm, int durationMs) {
    if (pcm.isEmpty()) return;

    const QByteArray container = CVoiceCodec::encode(pcm);
    if (container.isEmpty()) {
        emit voiceStatus(QStringLiteral("语音编码失败：%1").arg(CVoiceCodec::lastError()),
                         true, 4000);
        return;
    }
    if (container.size() > static_cast<int>(dp::kMaxVoiceBytes)) {
        emit voiceStatus(QStringLiteral("语音太长（%1 秒），请控制在 %2 秒内")
                             .arg(durationMs / 1000)
                             .arg(dp::kMaxVoiceDurationMs / 1000),
                         true, 4000);
        return;
    }

    emit sendVoiceRequested(container, durationMs);
    emit voiceStatus(QStringLiteral("已发送语音（%1 秒）").arg(durationMs / 1000.0, 0, 'f', 1),
                     false, 2000);
}

/*
 * 功能描述:  执行 translationEnabled 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool ChatController::translationEnabled() const {
    return m_pTranslation != nullptr && m_pTranslation->isEnabled();
}

/*
 * 功能描述:  执行 replyEnabled 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool ChatController::replyEnabled() const {
    return m_pReply != nullptr && m_pReply->isEnabled();
}

/* ---------------------------------------------------------------- 语音转写队列 */

/*
 * 功能描述:  执行 enqueueTranscript 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * return:    void
 */
void ChatController::enqueueTranscript(qint64 seq) {
    if (seq <= 0 || model_ == nullptr) return;

    ChatItem_S* item = model_->find(seq);
    if (item == nullptr) return;
    if (item->kind != MessageKind_EN::Voice) return;
    /* 已在转写或已完成 */
    if (item->transcriptState != 0) return;

    /* 标记为转写中，界面立刻显示占位 */
    model_->setTranscriptState(seq, 1);

    m_uTranscriptQueue.enqueue(seq);
    pumpTranscript();
}

/*
 * 功能描述:  执行 pumpTranscript 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatController::pumpTranscript() {
    if (m_uTranscribingSeq > 0 || m_uTranscriptQueue.isEmpty()) return;

    if (m_pRecorder == nullptr || !m_pRecorder->modelReady()) {
        /* whisper 还在加载：过一会儿再试，别把队列丢了 */
        QTimer::singleShot(1500, this, &ChatController::pumpTranscript);
        return;
    }

    while (!m_uTranscriptQueue.isEmpty()) {
        const qint64 seq = m_uTranscriptQueue.dequeue();

        const ChatItem_S* target = model_->find(seq);
        if (target == nullptr || target->transcriptState != 1) continue;

        const QByteArray pcm = CVoiceCodec::decode(target->audio);
        if (pcm.isEmpty()) {
            model_->setTranscript(seq, QString(), false);
            continue;
        }

        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/voice");
        QDir().mkpath(dir);
        const QString wav = QStringLiteral("%1/seq_%2.wav").arg(dir).arg(seq);

        QFile file(wav);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            model_->setTranscript(seq, QString(), false);
            continue;
        }
        file.write(CVoiceCodec::toWav(pcm));
        file.close();

        m_uTranscribingSeq = seq;
        emit voiceStatus(QStringLiteral("识别中…"), false, 0);
        m_pRecorder->transcribeFile(wav);
        /* 一次只跑一条，textReady 里再取下一条 */
        return;
    }
}

/*
 * 功能描述:  执行 retryFailedTranslations 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatController::retryFailedTranslations() {
    if (model_ == nullptr || m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    const QVector<ChatItem_S>& items = model_->items();
    QVector<QPair<qint64, QString>> retry;
    int budget = m_nHistoryTranslationBudget;

    for (int i = items.size() - 1; i >= 0 && budget > 0; --i) {
        const ChatItem_S& item = items.at(i);
        if (item.system || item.kind != MessageKind_EN::Text) continue;
        /* 3 = 之前失败；0 = 当时翻译没启用，配置改好后一起补 */
        if (item.translationState != 3 && item.translationState != 0) continue;
        if (model_->isSelf(item.from)) continue;
        if (!CTranslationService::looksEnglish(item.text)) continue;

        model_->setTranslationState(item.seq, 1);
        retry.prepend(qMakePair(item.seq, item.text));
        --budget;
    }

    for (const auto& pair : retry) {
        m_pTranslation->requestMessage(pair.first, pair.second);
    }
}

/* namespace business */
}
