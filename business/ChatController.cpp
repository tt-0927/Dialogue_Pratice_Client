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
CLlmClient::Config_S toClientConfig(const AppConfig::LlmSettings_S& stSource) {
    CLlmClient::Config_S stCfg;
    stCfg.baseUrl = stSource.baseUrl;
    stCfg.apiKey = stSource.apiKey;
    stCfg.model = stSource.model;
    stCfg.temperature = stSource.temperature;
    stCfg.reasoningEffort = stSource.reasoningEffort;
    stCfg.timeoutMs = stSource.timeoutMs;
    stCfg.enabled = stSource.enabled;
    return stCfg;
}

/* namespace */
}

/*
 * 功能描述:  构造 ChatController 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pModel           模型名称，类型 ChatModel*。
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject*。
 * return:    无返回值（构造函数或析构函数）。
 */
ChatController::ChatController(ChatModel* pModel, QObject* pParent)
    : QObject(pParent), model_(pModel) {
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
            [this](qint64 lSeq, const QString& strChinese) {
                if (model_ != nullptr) model_->setTranslation(lSeq, strChinese);
            });
    connect(m_pTranslation, &CTranslationService::inputTranslated, this,
            [this](const QString& stSource, const QString& english) {
                /* 用户已经改了输入内容就丢弃这次结果 */
                if (stSource != m_strPendingInput) return;
                emit inputTranslationReady(stSource, english);
            });
    connect(m_pTranslation, &CTranslationService::failed, this,
            [this](qint64 lSeq, const QString& reason) {
                if (lSeq <= 0) {
                    /* 输入框的中文翻译失败：卡片不能一直停在「翻译中…」 */
                    emit inputTranslationFailed(reason);
                    return;
                }
                if (model_ != nullptr) model_->setTranslationState(lSeq, 3);
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
    const QString strConfigPath = AppConfig::resolvePath();
    /* 记录「进来时本地到底有没有配置」：ensureTemplate 会补一份空模板， */
    /* 那份模板不代表用户配过东西，不能因此弹「没填 apiKey」的错误。 */
    const bool nHadLocalConfig = !strConfigPath.isEmpty() && QFileInfo::exists(strConfigPath);
    AppConfig::ensureTemplate(strConfigPath);

    AppConfig::Settings_S stSettings = AppConfig::load();
    if (!nHadLocalConfig) {
        /* 本次刚生成的模板：视为「未配置」，等登录后服务端下发； */
        /* 若服务端也没有，翻译功能静默不可用（不打扰用户）。 */
        stSettings.missing = true;
        stSettings.error.clear();
    }
    applyConfig(stSettings, true);
    watchConfig();
}

ChatController::~ChatController() = default;

/*
 * 功能描述:  执行watch 配置；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::watchConfig() {
    const QString strConfigPath = AppConfig::resolvePath();
    if (strConfigPath.isEmpty()) return;

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pConfigWatcher = new QFileSystemWatcher(this);
    m_pConfigWatcher->addPath(strConfigPath);
    m_pConfigWatcher->addPath(QFileInfo(strConfigPath).absolutePath());

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
 * 功能描述:  应用remote 配置。
 * 作者:      JaTin
 * param [in]   byJson           JSON 对象，类型 const QByteArray&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::applyRemoteConfig(const QByteArray& byJson) {
    if (byJson.isEmpty()) return;

    const AppConfig::Settings_S stSettings = AppConfig::parse(byJson, QStringLiteral("<server>"));

    if (!stSettings.loaded) {
        /* 下发内容有问题时保持原配置，不要因为服务端配错就把本地也弄坏 */
        DP_LOG_WARN(kTag, QStringLiteral("服务端下发的 LLM 配置无效：%1").arg(stSettings.error));
        return;
    }

    m_nUsingRemoteConfig = true;
    applyConfig(stSettings, false, /*silent=*/true);

    const QString strTranslateModel = stSettings.translation.hasOwnLlm
                                       ? stSettings.translation.llm.model
                                       : stSettings.llm.model;
    DP_LOG_INFO(kTag, QStringLiteral("已应用服务端下发的配置：对话 %1 / 翻译 %2")
                          .arg(stSettings.llm.model, strTranslateModel));
}

/*
 * 功能描述:  应用配置。
 * 作者:      JaTin
 * param [in]   stSettings       配置集合，类型 const AppConfig::Settings_S&。
 * param [in]   nInitial         输入数据，类型 bool。
 * param [in]   nSilent          输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::applyConfig(const AppConfig::Settings_S& stSettings, bool nInitial, bool nSilent) {
    /* 对话/建议用主模型 */
    m_pLlm->setConfig(toClientConfig(stSettings.llm));

    /* 翻译：配了专用模型就用它，否则沿用主模型参数 */
    const bool nSeparate = stSettings.translation.hasOwnLlm;
    m_pTranslateLlm->setConfig(toClientConfig(nSeparate ? stSettings.translation.llm : stSettings.llm));

    m_nHistoryTranslationBudget = qMax(0, stSettings.translation.maxHistoryMessages);
    m_strConfigError = stSettings.error;

    if (stSettings.missing) {
        /* 本地没有配置：等登录后服务端下发；若最终也没有，翻译功能静默不可用 */
        DP_LOG_WARN(kTag, QStringLiteral("本地无 LLM 配置（%1），等待服务端下发")
                              .arg(stSettings.path));
    }

    if (!stSettings.error.isEmpty() && !stSettings.missing) {
        if (nInitial) {
            /* 构造期发出会丢（外部尚未 connect），推到事件循环第一轮 */
            const QString strMessage = stSettings.error;
            QTimer::singleShot(0, this, [this, strMessage] { emit notice(strMessage, true); });
        } else {
            emit notice(stSettings.error, true);
        }
    } else if (!nInitial && !nSilent) {
        const QString strTranslateModel = nSeparate ? stSettings.translation.llm.model
                                                : stSettings.llm.model;
        emit notice(QStringLiteral("配置已重新加载：对话 %1 / 翻译 %2")
                        .arg(stSettings.llm.model, strTranslateModel),
                    false);
    }

    /* 之前因为没配 Key 而失败的条目，填好之后自动补翻 */
    if (m_pTranslateLlm->isEnabled()) retryFailedTranslations();
}

/* ---------------------------------------------------------------- 录音器 */

/*
 * 功能描述:  设置录制器；实现上连接 Qt 信号与槽。
 * 作者:      JaTin
 * param [in]   pRecorder        录制器，类型 CVoiceRecorder*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::setRecorder(CVoiceRecorder* pRecorder) {
    if (m_pRecorder == pRecorder) return;
    m_pRecorder = pRecorder;
    if (m_pRecorder == nullptr) return;

    /* 转写结果：回填模型后接着翻中文 */
    connect(m_pRecorder, &CVoiceRecorder::textReady, this,
            [this](const QString& text, qint64 elapsedMs) {
                if (m_uTranscribingSeq > 0) {
                    const qint64 lSeq = m_uTranscribingSeq;
                    m_uTranscribingSeq = 0;

                    model_->setTranscript(lSeq, text, true);
                    /* 清掉「识别中…」 */
                    emit voiceStatus(QString(), false, 0);

                    /* 转写出来的是英文，接着走翻译 */
                    const ChatItem_S* stItem = model_->find(lSeq);
                    if (stItem != nullptr && translationEnabled()
                        && CTranslationService::looksEnglish(text)) {
                        model_->setTranslationState(lSeq, 1);
                        m_pTranslation->requestMessage(lSeq, text);
                    }
                    pumpTranscript();
                    return;
                }

                /* 兜底路径（--voice-file 诊断等） */
                emit voiceStatus(QStringLiteral("已识别（%1 ms）").arg(elapsedMs), false, 2500);
            });

    connect(m_pRecorder, &CVoiceRecorder::failed, this, [this](const QString& message) {
        if (m_uTranscribingSeq > 0) {
            const qint64 lSeq = m_uTranscribingSeq;
            m_uTranscribingSeq = 0;
            model_->setTranscript(lSeq, QString(), false);
            emit voiceStatus(QString(), false, 0);
            pumpTranscript();
        }
        emit voiceStatus(message, true, 3500);
    });
}

/*
 * 功能描述:  获取转写文本 ready。
 * 作者:      JaTin
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool ChatController::transcriptReady() const {
    return m_pRecorder != nullptr && m_pRecorder->modelReady();
}

/* ---------------------------------------------------------------- 编排 */

/*
 * 功能描述:  重置on 模型名。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::onModelReset() {
    if (model_ == nullptr) return;

    const QVector<ChatItem_S>& aItems = model_->items();

    /* ---- 历史文本：只翻最近 N 条，避免一进房排队几十个请求 ---- */
    if (translationEnabled()) {
        QVector<QPair<qint64, QString>> lPending;
        int nBudget = m_nHistoryTranslationBudget;

        for (int i = aItems.size() - 1; i >= 0 && nBudget > 0; --i) {
            const ChatItem_S& stItem = aItems.at(i);
            if (stItem.system || stItem.kind != MessageKind_EN::Text) continue;
            if (stItem.translationState != 0) continue;
            if (model_->isSelf(stItem.from)) continue;
            if (!CTranslationService::looksEnglish(stItem.text)) continue;

            lPending.prepend(qMakePair(stItem.seq, stItem.text));
            --nBudget;
        }

        for (const auto& stPair : lPending) {
            model_->setTranslationState(stPair.first, 1);
            m_pTranslation->requestMessage(stPair.first, stPair.second);
        }
        if (!lPending.isEmpty()) {
            DP_LOG_DEBUG(kTag, QStringLiteral("history: %1 message(s) queued for translation")
                                    .arg(lPending.size()));
        }
    }

    /* ---- 历史语音：只转写最近 3 条，避免一次跑太多 whisper ---- */
    int nVoiceBudget = kHistoryVoiceBudget;
    for (int i = aItems.size() - 1; i >= 0 && nVoiceBudget > 0; --i) {
        const ChatItem_S& stItem = aItems.at(i);
        if (stItem.kind != MessageKind_EN::Voice || stItem.transcriptState != 0) continue;
        enqueueTranscript(stItem.seq);
        --nVoiceBudget;
    }
}

/*
 * 功能描述:  执行on 条目 appended。
 * 作者:      JaTin
 * param [in]   stItem           条目，类型 const ChatItem_S&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::onItemAppended(const ChatItem_S& stItem) {
    if (stItem.system) return;

    const bool nIncoming = !model_->isSelf(stItem.from);
    DP_LOG_DEBUG(kTag, QStringLiteral("item seq=%1 from=%2 kind=%3 incoming=%4 "
                                      "translate=%5 reply=%6")
                           .arg(stItem.seq)
                           .arg(stItem.from)
                           .arg(stItem.kind == MessageKind_EN::Voice ? "voice" : "text")
                           .arg(nIncoming)
                           .arg(translationEnabled())
                           .arg(replyEnabled()));

    /* ---- 文本：他人英文消息需要翻译 ---- */
    if (stItem.kind == MessageKind_EN::Text && nIncoming && translationEnabled()
        && CTranslationService::looksEnglish(stItem.text)) {
        model_->setTranslationState(stItem.seq, 1);
        m_pTranslation->requestMessage(stItem.seq, stItem.text);
    }

    /* ---- 语音：先本地转写，转写完再翻 ---- */
    if (stItem.kind == MessageKind_EN::Voice) {
        enqueueTranscript(stItem.seq);
    }

    /* ---- 他人消息到达且输入框为空 -> 稍后生成建议 ---- */
    if (nIncoming && replyEnabled() && m_nComposerEmpty) {
        m_pReplyDebounce->start();
    }
}

/*
 * 功能描述:  请求智能建议列表。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::requestSuggestions() {
    if (model_ == nullptr || m_pReply == nullptr || !m_pReply->isEnabled()) return;

    const QVector<ChatMessage_S> aContext = model_->recentContext(6);
    if (aContext.isEmpty()) return;

    m_pReply->request(aContext);
}

/*
 * 功能描述:  请求输入译文。
 * 作者:      JaTin
 * param [in]   strChinese       输入数据，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::requestInputTranslation(const QString& strChinese) {
    m_strPendingInput = strChinese.trimmed();
    if (m_strPendingInput.isEmpty()) return;
    if (m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    m_pInputDebounce->start();
}

/*
 * 功能描述:  发送语音消息。
 * 作者:      JaTin
 * param [in]   byPcm            输入数据，类型 const QByteArray&。
 * param [in]   nDurationMs      语音时长，单位毫秒，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::sendVoice(const QByteArray& byPcm, int nDurationMs) {
    if (byPcm.isEmpty()) return;

    const QByteArray byContainer = CVoiceCodec::encode(byPcm);
    if (byContainer.isEmpty()) {
        emit voiceStatus(QStringLiteral("语音编码失败：%1").arg(CVoiceCodec::lastError()),
                         true, 4000);
        return;
    }
    if (byContainer.size() > static_cast<int>(dp::kMaxVoiceBytes)) {
        emit voiceStatus(QStringLiteral("语音太长（%1 秒），请控制在 %2 秒内")
                             .arg(nDurationMs / 1000)
                             .arg(dp::kMaxVoiceDurationMs / 1000),
                         true, 4000);
        return;
    }

    emit sendVoiceRequested(byContainer, nDurationMs);
    emit voiceStatus(QStringLiteral("已发送语音（%1 秒）").arg(nDurationMs / 1000.0, 0, 'f', 1),
                     false, 2000);
}

/*
 * 功能描述:  获取译文启用状态。
 * 作者:      JaTin
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool ChatController::translationEnabled() const {
    return m_pTranslation != nullptr && m_pTranslation->isEnabled();
}

/*
 * 功能描述:  应答启用状态。
 * 作者:      JaTin
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool ChatController::replyEnabled() const {
    return m_pReply != nullptr && m_pReply->isEnabled();
}

/* ---------------------------------------------------------------- 语音转写队列 */

/*
 * 功能描述:  入队转写文本。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::enqueueTranscript(qint64 lSeq) {
    if (lSeq <= 0 || model_ == nullptr) return;

    ChatItem_S* stItem = model_->find(lSeq);
    if (stItem == nullptr) return;
    if (stItem->kind != MessageKind_EN::Voice) return;
    /* 已在转写或已完成 */
    if (stItem->transcriptState != 0) return;

    /* 标记为转写中，界面立刻显示占位 */
    model_->setTranscriptState(lSeq, 1);

    m_uTranscriptQueue.enqueue(lSeq);
    pumpTranscript();
}

/*
 * 功能描述:  执行pump 转写文本。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::pumpTranscript() {
    if (m_uTranscribingSeq > 0 || m_uTranscriptQueue.isEmpty()) return;

    if (m_pRecorder == nullptr || !m_pRecorder->modelReady()) {
        /* whisper 还在加载：过一会儿再试，别把队列丢了 */
        QTimer::singleShot(1500, this, &ChatController::pumpTranscript);
        return;
    }

    while (!m_uTranscriptQueue.isEmpty()) {
        const qint64 lSeq = m_uTranscriptQueue.dequeue();

        const ChatItem_S* pTarget = model_->find(lSeq);
        if (pTarget == nullptr || pTarget->transcriptState != 1) continue;

        const QByteArray byPcm = CVoiceCodec::decode(pTarget->audio);
        if (byPcm.isEmpty()) {
            model_->setTranscript(lSeq, QString(), false);
            continue;
        }

        const QString strDir =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/voice");
        QDir().mkpath(strDir);
        const QString strWav = QStringLiteral("%1/seq_%2.wav").arg(strDir).arg(lSeq);

        QFile stFile(strWav);
        if (!stFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            model_->setTranscript(lSeq, QString(), false);
            continue;
        }
        stFile.write(CVoiceCodec::toWav(byPcm));
        stFile.close();

        m_uTranscribingSeq = lSeq;
        emit voiceStatus(QStringLiteral("识别中…"), false, 0);
        m_pRecorder->transcribeFile(strWav);
        /* 一次只跑一条，textReady 里再取下一条 */
        return;
    }
}

/*
 * 功能描述:  重试failedtranslations。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatController::retryFailedTranslations() {
    if (model_ == nullptr || m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    const QVector<ChatItem_S>& aItems = model_->items();
    QVector<QPair<qint64, QString>> lRetry;
    int nBudget = m_nHistoryTranslationBudget;

    for (int i = aItems.size() - 1; i >= 0 && nBudget > 0; --i) {
        const ChatItem_S& stItem = aItems.at(i);
        if (stItem.system || stItem.kind != MessageKind_EN::Text) continue;
        /* 3 = 之前失败；0 = 当时翻译没启用，配置改好后一起补 */
        if (stItem.translationState != 3 && stItem.translationState != 0) continue;
        if (model_->isSelf(stItem.from)) continue;
        if (!CTranslationService::looksEnglish(stItem.text)) continue;

        model_->setTranslationState(stItem.seq, 1);
        lRetry.prepend(qMakePair(stItem.seq, stItem.text));
        --nBudget;
    }

    for (const auto& stPair : lRetry) {
        m_pTranslation->requestMessage(stPair.first, stPair.second);
    }
}

/* namespace business */
}
