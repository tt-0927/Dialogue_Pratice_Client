/**
 * @file  ChatController.cpp
 * @brief 聊天业务编排器的实现。
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

/// @brief 建议请求的防抖间隔：等待对方可能连续发来的多条消息。
constexpr int kReplyDebounceMs = 800;
/// @brief 输入翻译的防抖间隔。
constexpr int kInputDebounceMs = 500;
/// @brief 历史消息里最多转写几条语音，避免进房时一次跑太多 whisper。
constexpr int kHistoryVoiceBudget = 3;

/**
 * @brief 把 AppConfig 的 LLM 配置转成 LlmClient 的配置。
 * @param source 配置快照。
 * @retval 客户端可直接应用的配置。
 */
LlmClient::Config toClientConfig(const AppConfig::LlmSettings& source) {
    LlmClient::Config cfg;
    cfg.baseUrl = source.baseUrl;
    cfg.apiKey = source.apiKey;
    cfg.model = source.model;
    cfg.temperature = source.temperature;
    cfg.reasoningEffort = source.reasoningEffort;
    cfg.timeoutMs = source.timeoutMs;
    cfg.enabled = source.enabled;
    return cfg;
}

}  // namespace

ChatController::ChatController(ChatModel* model, QObject* parent)
    : QObject(parent), model_(model) {
    // 两个独立客户端：翻译可以单独指定专用模型（如 Hunyuan-MT），
    // 对话/智能建议用通用模型（如 Qwen），各发挥优势。
    llm_ = new LlmClient(this);
    translateLlm_ = new LlmClient(this);

    translation_ = new TranslationService(this);
    translation_->setClient(translateLlm_);

    reply_ = new ReplyService(this);
    reply_->setClient(llm_);

    // ---- 消息 -> 编排 ----
    if (model_ != nullptr) {
        connect(model_, &ChatModel::itemAppended, this, &ChatController::onItemAppended);
        connect(model_, &ChatModel::modelReset, this, &ChatController::onModelReset);
    }

    // ---- 翻译结果回填模型 ----
    connect(translation_, &TranslationService::messageTranslated, this,
            [this](qint64 seq, const QString& chinese) {
                if (model_ != nullptr) model_->setTranslation(seq, chinese);
            });
    connect(translation_, &TranslationService::inputTranslated, this,
            [this](const QString& source, const QString& english) {
                // 用户已经改了输入内容就丢弃这次结果
                if (source != pendingInput_) return;
                emit inputTranslationReady(source, english);
            });
    connect(translation_, &TranslationService::failed, this,
            [this](qint64 seq, const QString& reason) {
                if (seq <= 0) {
                    // 输入框的中文翻译失败：卡片不能一直停在「翻译中…」
                    emit inputTranslationFailed(reason);
                    return;
                }
                if (model_ != nullptr) model_->setTranslationState(seq, 3);
            });

    // ---- 建议回复 ----
    connect(reply_, &ReplyService::suggestionsReady, this,
            [this](const QVector<Suggestion>& items) {
                if (!composerEmpty_) return;   // 用户已经在打字了
                emit suggestionsReady(items);
            });
    connect(reply_, &ReplyService::failed, this, [this](const QString&) {
        emit suggestionsCleared();
    });

    replyDebounce_ = new QTimer(this);
    replyDebounce_->setSingleShot(true);
    replyDebounce_->setInterval(kReplyDebounceMs);
    connect(replyDebounce_, &QTimer::timeout, this, &ChatController::requestSuggestions);

    inputDebounce_ = new QTimer(this);
    inputDebounce_->setSingleShot(true);
    inputDebounce_->setInterval(kInputDebounceMs);
    connect(inputDebounce_, &QTimer::timeout, this, [this] {
        if (pendingInput_.isEmpty()) return;
        if (translation_ == nullptr || !translation_->isEnabled()) return;
        translation_->requestInput(pendingInput_);
    });

    // ---- 配置加载与热重载 ----
    const QString configPath = AppConfig::resolvePath();
    // 记录「进来时本地到底有没有配置」：ensureTemplate 会补一份空模板，
    // 那份模板不代表用户配过东西，不能因此弹「没填 apiKey」的错误。
    const bool hadLocalConfig = !configPath.isEmpty() && QFileInfo::exists(configPath);
    AppConfig::ensureTemplate(configPath);

    AppConfig::Settings settings = AppConfig::load();
    if (!hadLocalConfig) {
        // 本次刚生成的模板：视为「未配置」，等登录后服务端下发；
        // 若服务端也没有，翻译功能静默不可用（不打扰用户）。
        settings.missing = true;
        settings.error.clear();
    }
    applyConfig(settings, true);
    watchConfig();
}

ChatController::~ChatController() = default;

void ChatController::watchConfig() {
    const QString configPath = AppConfig::resolvePath();
    if (configPath.isEmpty()) return;

    configWatcher_ = new QFileSystemWatcher(this);
    configWatcher_->addPath(configPath);
    configWatcher_->addPath(QFileInfo(configPath).absolutePath());

    configReloadTimer_ = new QTimer(this);
    configReloadTimer_->setSingleShot(true);
    configReloadTimer_->setInterval(400);
    connect(configReloadTimer_, &QTimer::timeout, this, [this] {
        // 已采用服务端下发的配置：本地文件不再生效，否则两者会互相覆盖
        if (usingRemoteConfig_) return;
        applyConfig(AppConfig::load(), false);
    });

    connect(configWatcher_, &QFileSystemWatcher::fileChanged, this,
            [this] { configReloadTimer_->start(); });
    connect(configWatcher_, &QFileSystemWatcher::directoryChanged, this,
            [this] { configReloadTimer_->start(); });
}

void ChatController::applyRemoteConfig(const QByteArray& json) {
    if (json.isEmpty()) return;

    const AppConfig::Settings settings = AppConfig::parse(json, QStringLiteral("<server>"));

    if (!settings.loaded) {
        // 下发内容有问题时保持原配置，不要因为服务端配错就把本地也弄坏
        DP_LOG_WARN(kTag, QStringLiteral("服务端下发的 LLM 配置无效：%1").arg(settings.error));
        return;
    }

    usingRemoteConfig_ = true;
    applyConfig(settings, false, /*silent=*/true);

    const QString translateModel = settings.translation.hasOwnLlm
                                       ? settings.translation.llm.model
                                       : settings.llm.model;
    DP_LOG_INFO(kTag, QStringLiteral("已应用服务端下发的配置：对话 %1 / 翻译 %2")
                          .arg(settings.llm.model, translateModel));
}

void ChatController::applyConfig(const AppConfig::Settings& settings, bool initial, bool silent) {
    // 对话/建议用主模型
    llm_->setConfig(toClientConfig(settings.llm));

    // 翻译：配了专用模型就用它，否则沿用主模型参数
    const bool separate = settings.translation.hasOwnLlm;
    translateLlm_->setConfig(toClientConfig(separate ? settings.translation.llm : settings.llm));

    historyTranslationBudget_ = qMax(0, settings.translation.maxHistoryMessages);
    configError_ = settings.error;

    if (settings.missing) {
        // 本地没有配置：等登录后服务端下发；若最终也没有，翻译功能静默不可用
        DP_LOG_WARN(kTag, QStringLiteral("本地无 LLM 配置（%1），等待服务端下发")
                              .arg(settings.path));
    }

    if (!settings.error.isEmpty() && !settings.missing) {
        if (initial) {
            // 构造期发出会丢（外部尚未 connect），推到事件循环第一轮
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

    // 之前因为没配 Key 而失败的条目，填好之后自动补翻
    if (translateLlm_->isEnabled()) retryFailedTranslations();
}

// ---------------------------------------------------------------- 录音器

void ChatController::setRecorder(VoiceRecorder* recorder) {
    if (recorder_ == recorder) return;
    recorder_ = recorder;
    if (recorder_ == nullptr) return;

    // 转写结果：回填模型后接着翻中文
    connect(recorder_, &VoiceRecorder::textReady, this,
            [this](const QString& text, qint64 elapsedMs) {
                if (transcribingSeq_ > 0) {
                    const qint64 seq = transcribingSeq_;
                    transcribingSeq_ = 0;

                    model_->setTranscript(seq, text, true);
                    emit voiceStatus(QString(), false, 0);   // 清掉「识别中…」

                    // 转写出来的是英文，接着走翻译
                    const ChatItem* item = model_->find(seq);
                    if (item != nullptr && translationEnabled()
                        && TranslationService::looksEnglish(text)) {
                        model_->setTranslationState(seq, 1);
                        translation_->requestMessage(seq, text);
                    }
                    pumpTranscript();
                    return;
                }

                // 兜底路径（--voice-file 诊断等）
                emit voiceStatus(QStringLiteral("已识别（%1 ms）").arg(elapsedMs), false, 2500);
            });

    connect(recorder_, &VoiceRecorder::failed, this, [this](const QString& message) {
        if (transcribingSeq_ > 0) {
            const qint64 seq = transcribingSeq_;
            transcribingSeq_ = 0;
            model_->setTranscript(seq, QString(), false);
            emit voiceStatus(QString(), false, 0);
            pumpTranscript();
        }
        emit voiceStatus(message, true, 3500);
    });
}

bool ChatController::transcriptReady() const {
    return recorder_ != nullptr && recorder_->modelReady();
}

// ---------------------------------------------------------------- 编排

void ChatController::onModelReset() {
    if (model_ == nullptr) return;

    const QVector<ChatItem>& items = model_->items();

    // ---- 历史文本：只翻最近 N 条，避免一进房排队几十个请求 ----
    if (translationEnabled()) {
        QVector<QPair<qint64, QString>> pending;
        int budget = historyTranslationBudget_;

        for (int i = items.size() - 1; i >= 0 && budget > 0; --i) {
            const ChatItem& item = items.at(i);
            if (item.system || item.kind != MessageKind::Text) continue;
            if (item.translationState != 0) continue;
            if (model_->isSelf(item.from)) continue;
            if (!TranslationService::looksEnglish(item.text)) continue;

            pending.prepend(qMakePair(item.seq, item.text));
            --budget;
        }

        for (const auto& pair : pending) {
            model_->setTranslationState(pair.first, 1);
            translation_->requestMessage(pair.first, pair.second);
        }
        if (!pending.isEmpty()) {
            DP_LOG_DEBUG(kTag, QStringLiteral("history: %1 message(s) queued for translation")
                                    .arg(pending.size()));
        }
    }

    // ---- 历史语音：只转写最近 3 条，避免一次跑太多 whisper ----
    int voiceBudget = kHistoryVoiceBudget;
    for (int i = items.size() - 1; i >= 0 && voiceBudget > 0; --i) {
        const ChatItem& item = items.at(i);
        if (item.kind != MessageKind::Voice || item.transcriptState != 0) continue;
        enqueueTranscript(item.seq);
        --voiceBudget;
    }
}

void ChatController::onItemAppended(const ChatItem& item) {
    if (item.system) return;

    const bool incoming = !model_->isSelf(item.from);
    DP_LOG_DEBUG(kTag, QStringLiteral("item seq=%1 from=%2 kind=%3 incoming=%4 "
                                      "translate=%5 reply=%6")
                           .arg(item.seq)
                           .arg(item.from)
                           .arg(item.kind == MessageKind::Voice ? "voice" : "text")
                           .arg(incoming)
                           .arg(translationEnabled())
                           .arg(replyEnabled()));

    // ---- 文本：他人英文消息需要翻译 ----
    if (item.kind == MessageKind::Text && incoming && translationEnabled()
        && TranslationService::looksEnglish(item.text)) {
        model_->setTranslationState(item.seq, 1);
        translation_->requestMessage(item.seq, item.text);
    }

    // ---- 语音：先本地转写，转写完再翻 ----
    if (item.kind == MessageKind::Voice) {
        enqueueTranscript(item.seq);
    }

    // ---- 他人消息到达且输入框为空 -> 稍后生成建议 ----
    if (incoming && replyEnabled() && composerEmpty_) {
        replyDebounce_->start();
    }
}

void ChatController::requestSuggestions() {
    if (model_ == nullptr || reply_ == nullptr || !reply_->isEnabled()) return;

    const QVector<ChatMessage> context = model_->recentContext(6);
    if (context.isEmpty()) return;

    reply_->request(context);
}

void ChatController::requestInputTranslation(const QString& chinese) {
    pendingInput_ = chinese.trimmed();
    if (pendingInput_.isEmpty()) return;
    if (translation_ == nullptr || !translation_->isEnabled()) return;

    inputDebounce_->start();
}

void ChatController::sendVoice(const QByteArray& pcm, int durationMs) {
    if (pcm.isEmpty()) return;

    const QByteArray container = VoiceCodec::encode(pcm);
    if (container.isEmpty()) {
        emit voiceStatus(QStringLiteral("语音编码失败：%1").arg(VoiceCodec::lastError()),
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

bool ChatController::translationEnabled() const {
    return translation_ != nullptr && translation_->isEnabled();
}

bool ChatController::replyEnabled() const {
    return reply_ != nullptr && reply_->isEnabled();
}

// ---------------------------------------------------------------- 语音转写队列

void ChatController::enqueueTranscript(qint64 seq) {
    if (seq <= 0 || model_ == nullptr) return;

    ChatItem* item = model_->find(seq);
    if (item == nullptr) return;
    if (item->kind != MessageKind::Voice) return;
    if (item->transcriptState != 0) return;   // 已在转写或已完成

    model_->setTranscriptState(seq, 1);   // 标记为转写中，界面立刻显示占位

    transcriptQueue_.enqueue(seq);
    pumpTranscript();
}

void ChatController::pumpTranscript() {
    if (transcribingSeq_ > 0 || transcriptQueue_.isEmpty()) return;

    if (recorder_ == nullptr || !recorder_->modelReady()) {
        // whisper 还在加载：过一会儿再试，别把队列丢了
        QTimer::singleShot(1500, this, &ChatController::pumpTranscript);
        return;
    }

    while (!transcriptQueue_.isEmpty()) {
        const qint64 seq = transcriptQueue_.dequeue();

        const ChatItem* target = model_->find(seq);
        if (target == nullptr || target->transcriptState != 1) continue;

        const QByteArray pcm = VoiceCodec::decode(target->audio);
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
        file.write(VoiceCodec::toWav(pcm));
        file.close();

        transcribingSeq_ = seq;
        emit voiceStatus(QStringLiteral("识别中…"), false, 0);
        recorder_->transcribeFile(wav);
        return;   // 一次只跑一条，textReady 里再取下一条
    }
}

void ChatController::retryFailedTranslations() {
    if (model_ == nullptr || translation_ == nullptr || !translation_->isEnabled()) return;

    const QVector<ChatItem>& items = model_->items();
    QVector<QPair<qint64, QString>> retry;
    int budget = historyTranslationBudget_;

    for (int i = items.size() - 1; i >= 0 && budget > 0; --i) {
        const ChatItem& item = items.at(i);
        if (item.system || item.kind != MessageKind::Text) continue;
        // 3 = 之前失败；0 = 当时翻译没启用，配置改好后一起补
        if (item.translationState != 3 && item.translationState != 0) continue;
        if (model_->isSelf(item.from)) continue;
        if (!TranslationService::looksEnglish(item.text)) continue;

        model_->setTranslationState(item.seq, 1);
        retry.prepend(qMakePair(item.seq, item.text));
        --budget;
    }

    for (const auto& pair : retry) {
        translation_->requestMessage(pair.first, pair.second);
    }
}

}  // namespace business
