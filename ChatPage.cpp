// ChatPage.cpp
#include "ChatPage.h"

#include "Avatar.h"
#include "MessageBubble.h"
#include "LlmClient.h"
#include "MicButton.h"
#include "SendButton.h"
#include "ReplyService.h"
#include "SuggestionBar.h"
#include "Theme.h"
#include "TranslationCard.h"
#include "TranslationService.h"
#include "TtsService.h"
#include "VoiceCodec.h"
#include "VoiceInput.h"
#include "VoicePlayer.h"

#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QQueue>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

// ---------------------------------------------------------------- ComposerEdit

ComposerEdit::ComposerEdit(QWidget* parent) : QPlainTextEdit(parent) {
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

void ComposerEdit::keyPressEvent(QKeyEvent* event) {
    const bool isEnter = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter);
    if (isEnter && !(event->modifiers() & Qt::ShiftModifier)) {
        emit submit();
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

// ---------------------------------------------------------------- ChatPage

namespace {

const char* kComposerHint = "输入消息，Enter 发送，Shift+Enter 换行";

QString formatTime(qint64 ts) {
    return QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("HH:mm"));
}

QString formatDate(const QDate& date) {
    const QDate today = QDate::currentDate();
    if (date == today) return QStringLiteral("今天");
    if (date == today.addDays(-1)) return QStringLiteral("昨天");
    if (date.year() == today.year()) return date.toString(QStringLiteral("M月d日"));
    return date.toString(QStringLiteral("yyyy年M月d日"));
}

/// 模型路径优先级：QSettings 覆盖 > 编译期默认（源码树 models/ 目录）
QString resolveModelPath() {
    const QString configured = QSettings().value(QStringLiteral("voice/modelPath")).toString();
    if (!configured.isEmpty()) return configured;
#ifdef DP_DEFAULT_MODEL_PATH
    return QString::fromUtf8(DP_DEFAULT_MODEL_PATH);
#else
    return QString();
#endif
}

}  // namespace

ChatPage::ChatPage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 左侧：聊天列 ----
    auto* chatColumn = new QWidget(this);
    auto* chatColumnLayout = new QVBoxLayout(chatColumn);
    chatColumnLayout->setContentsMargins(0, 0, 0, 0);
    chatColumnLayout->setSpacing(0);

    // 顶栏
    auto* header = new QFrame(chatColumn);
    header->setObjectName("ChatHeader");
    header->setFixedHeight(64);
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 16, 0);
    headerLayout->setSpacing(10);

    auto* backBtn = new QPushButton(QStringLiteral("←"), header);
    backBtn->setObjectName("LinkButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedSize(34, 34);
    backBtn->setStyleSheet(QStringLiteral("font-size: 17pt; color: #3390EC;"));
    backBtn->setToolTip(QStringLiteral("返回并离开房间"));
    headerLayout->addWidget(backBtn);

    roomAvatar_ = new Avatar(header);
    roomAvatar_->setFixedSize(40, 40);
    roomAvatar_->setFontPx(18);
    roomAvatar_->setText(QStringLiteral("#"));
    roomAvatar_->setColor(theme::accent);
    headerLayout->addWidget(roomAvatar_);

    auto* titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(1);
    titleColumn->setContentsMargins(0, 0, 0, 0);

    roomTitle_ = new QLabel(header);
    roomTitle_->setStyleSheet(QStringLiteral("font-size: 11.5pt; font-weight: 700;"));
    titleColumn->addWidget(roomTitle_);

    roomSubtitle_ = new QLabel(header);
    roomSubtitle_->setObjectName("HintLabel");
    titleColumn->addWidget(roomSubtitle_);

    headerLayout->addLayout(titleColumn, 1);

    memberBtn_ = new QPushButton(header);
    memberBtn_->setObjectName("LinkButton");
    memberBtn_->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(memberBtn_);

    chatColumnLayout->addWidget(header);

    // 消息区
    scroll_ = new QScrollArea(chatColumn);
    scroll_->setObjectName("ChatScroll");
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    canvas_ = new QWidget();
    canvas_->setObjectName("ChatCanvas");
    chatLayout_ = new QVBoxLayout(canvas_);
    chatLayout_->setContentsMargins(20, 16, 20, 16);
    chatLayout_->setSpacing(10);
    chatLayout_->addStretch(1);   // 索引 0 固定为弹簧：消息贴底

    scroll_->setWidget(canvas_);
    chatColumnLayout->addWidget(scroll_, 1);

    // 输入区
    suggestionBar_ = new SuggestionBar(chatColumn);
    chatColumnLayout->addWidget(suggestionBar_);

    translationCard_ = new TranslationCard(chatColumn);
    chatColumnLayout->addWidget(translationCard_);

    voiceStatus_ = new QLabel(chatColumn);
    voiceStatus_->setObjectName("VoiceStatus");
    voiceStatus_->setVisible(false);
    chatColumnLayout->addWidget(voiceStatus_);

    auto* composerFrame = new QFrame(chatColumn);
    composerFrame->setObjectName("Composer");
    auto* composerLayout = new QHBoxLayout(composerFrame);
    composerLayout->setContentsMargins(16, 10, 16, 10);
    composerLayout->setSpacing(10);

    micBtn_ = new MicButton(composerFrame);
    composerLayout->addWidget(micBtn_, 0, Qt::AlignVCenter);

    composer_ = new ComposerEdit(composerFrame);
    composer_->setObjectName("ComposerEdit");
    composer_->setPlaceholderText(QString::fromUtf8(kComposerHint));
    composer_->setFixedHeight(46);
    composerLayout->addWidget(composer_, 1);

    sendBtn_ = new SendButton(composerFrame);
    composerLayout->addWidget(sendBtn_, 0, Qt::AlignVCenter);

    chatColumnLayout->addWidget(composerFrame);

    root->addWidget(chatColumn, 1);

    // ---- 右侧：成员面板 ----
    memberPanel_ = new QFrame(this);
    memberPanel_->setObjectName("MemberPanel");
    memberPanel_->setFixedWidth(220);
    memberPanel_->setVisible(false);

    auto* memberPanelLayout = new QVBoxLayout(memberPanel_);
    memberPanelLayout->setContentsMargins(12, 14, 12, 14);
    memberPanelLayout->setSpacing(8);

    memberTitle_ = new QLabel(memberPanel_);
    memberTitle_->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: 700;"));
    memberPanelLayout->addWidget(memberTitle_);

    auto* memberScroll = new QScrollArea(memberPanel_);
    memberScroll->setWidgetResizable(true);
    memberScroll->setFrameShape(QFrame::NoFrame);
    memberScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    memberScroll->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* memberCanvas = new QWidget();
    memberCanvas->setStyleSheet(QStringLiteral("background: transparent;"));
    memberListLayout_ = new QVBoxLayout(memberCanvas);
    memberListLayout_->setContentsMargins(0, 0, 0, 0);
    memberListLayout_->setSpacing(4);
    memberListLayout_->addStretch(1);
    memberScroll->setWidget(memberCanvas);

    memberPanelLayout->addWidget(memberScroll, 1);
    root->addWidget(memberPanel_);

    // ---- 连接 ----
    connect(backBtn, &QPushButton::clicked, this, &ChatPage::backRequested);
    connect(sendBtn_, &SendButton::clicked, this, &ChatPage::sendCurrentText);
    connect(composer_, &ComposerEdit::submit, this, &ChatPage::sendCurrentText);
    connect(memberBtn_, &QPushButton::clicked, this, [this] {
        memberPanel_->setVisible(!memberPanel_->isVisible());
    });

    // 内容高度变化（气泡换行、历史批量插入）时，若仍在贴底状态就继续贴底
    connect(scroll_->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) {
        if (pendingScroll_) forceScrollToBottom();
    });

    // ---- 语音输入 ----
    voice_ = new VoiceInput(this);
    voice_->setModelPath(resolveModelPath());

    connect(micBtn_, &QAbstractButton::clicked, this, &ChatPage::toggleRecording);

    connect(voice_, &VoiceInput::modelLoaded, this, [this](bool ok, const QString& message) {
        voiceAvailable_ = ok;
        if (!ok) {
            micBtn_->setEnabled(false);
            micBtn_->setToolTip(message);
            showVoiceStatus(message, true, 6000);
        }
    });
    connect(voice_, &VoiceInput::pcmReady, this, &ChatPage::onVoiceRecorded);

    // 语音播放：状态同步到对应气泡的语音条
    connect(VoicePlayer::instance(), &VoicePlayer::playingChanged, this, [this](bool playing) {
        const QByteArray current = VoicePlayer::instance()->current();
        for (const ChatItem& item : model_) {
            if (item.kind != MessageKind::Voice || item.seq <= 0) continue;
            MessageBubble* bubble = bubbles_.value(item.seq, nullptr);
            if (bubble == nullptr) continue;
            bubble->setPlaying(playing && !current.isEmpty() && item.audio == current);
        }
    });
    connect(VoicePlayer::instance(), &VoicePlayer::failed, this, [this](const QString& reason) {
        showVoiceStatus(reason, true, 3000);
    });

    connect(voice_, &VoiceInput::recordingStarted, this, [this] {
        micBtn_->setRecording(true);
        showVoiceStatus(QStringLiteral("● 正在录音 0:00 · 再点一次结束"), false, 0);
    });
    connect(voice_, &VoiceInput::recordingTick, this, [this](int seconds, int /*level*/) {
        showVoiceStatus(QStringLiteral("● 正在录音 0:%1 · 再点一次结束")
                            .arg(seconds, 2, 10, QLatin1Char('0')),
                        false, 0);
    });
    connect(voice_, &VoiceInput::recognizing, this, [this] {
        micBtn_->setRecording(false);
        showVoiceStatus(QStringLiteral("识别中…"), false, 0);
    });
    connect(voice_, &VoiceInput::textReady, this, [this](const QString& text, qint64 elapsedMs) {
        micBtn_->setRecording(false);

        // 语音消息的转写结果：回填到气泡，不再插进输入框
        if (transcribingSeq_ > 0) {
            const qint64 seq = transcribingSeq_;
            transcribingSeq_ = 0;
            updateTranscript(seq, text, true);
            showVoiceStatus(QString(), false, 0);   // 清掉「识别中…」
            pumpTranscript();
            return;
        }

        // 兜底路径（--voice-file 诊断等）
        insertVoiceText(text);
        showVoiceStatus(QStringLiteral("已识别（%1 ms），可直接编辑后发送").arg(elapsedMs),
                        false, 2500);
    });
    connect(voice_, &VoiceInput::failed, this, [this](const QString& message) {
        micBtn_->setRecording(false);

        if (transcribingSeq_ > 0) {
            const qint64 seq = transcribingSeq_;
            transcribingSeq_ = 0;
            updateTranscript(seq, QString(), false);
            showVoiceStatus(QString(), false, 0);
            pumpTranscript();
        }

        showVoiceStatus(message, true, 3500);
    });

    voice_->start();   // 模型在后台线程加载，不阻塞界面

    // ---- 翻译（他人英文消息 -> 中文），配置来自 config/llm.json ----
    // 两个独立客户端：翻译可以单独指定专用模型（如 Hunyuan-MT），
    // 对话/智能建议用通用模型（如 Qwen），各发挥优势。
    llm_ = new LlmClient(this);
    translateLlm_ = new LlmClient(this);
    translation_ = new TranslationService(this);
    translation_->setClient(translateLlm_);

    const QString configPath = AppConfig::resolvePath();
    AppConfig::ensureTemplate(configPath);   // 首次运行生成一份可编辑的模板
    applyConfig(AppConfig::load(), true);

    // 配置文件热重载：改完保存就生效，不用重启
    configWatcher_ = new QFileSystemWatcher(this);
    if (!configPath.isEmpty()) {
        configWatcher_->addPath(configPath);
        configWatcher_->addPath(QFileInfo(configPath).absolutePath());
    }
    configReloadTimer_ = new QTimer(this);
    configReloadTimer_->setSingleShot(true);
    configReloadTimer_->setInterval(400);
    connect(configReloadTimer_, &QTimer::timeout, this, [this] {
        applyConfig(AppConfig::load(), false);
    });
    connect(configWatcher_, &QFileSystemWatcher::fileChanged, this,
            [this] { configReloadTimer_->start(); });
    connect(configWatcher_, &QFileSystemWatcher::directoryChanged, this,
            [this] { configReloadTimer_->start(); });

    connect(translation_, &TranslationService::messageTranslated, this,
            [this](qint64 seq, const QString& chinese) {
                for (ChatItem& item : model_) {
                    if (item.seq == seq) {
                        item.translation = chinese;
                        item.translationState = 2;
                        break;
                    }
                }
                if (MessageBubble* bubble = bubbles_.value(seq, nullptr)) {
                    bubble->setTranslation(chinese);
                }
            });
    connect(translation_, &TranslationService::inputTranslated, this,
            [this](const QString& source, const QString& english) {
                // 用户已经改了输入内容就丢弃这次结果
                if (source != composer_->toPlainText().trimmed()) return;
                translationCard_->showTranslation(english);
            });
    // ---- 语音合成（Piper）----
    tts_ = new TtsService(this);
    tts_->setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
    connect(tts_, &TtsService::speakingChanged, this, [this](bool speaking) {
        translationCard_->setPlaying(speaking);
        // 气泡里的播放按钮也要跟着变成暂停图标，否则用户看不出在播
        if (!speaking && playingBubble_ != nullptr) {
            playingBubble_->setPlaying(false);
            playingBubble_ = nullptr;
        } else if (speaking && playingBubble_ != nullptr) {
            playingBubble_->setPlaying(true);
        }
    });
    connect(tts_, &TtsService::failed, this, [this](const QString& reason) {
        showVoiceStatus(reason, true, 3000);
    });

    // ---- 中文输入 -> 英文译文卡 ----
    connect(translationCard_, &TranslationCard::playRequested, this,
            [this](const QString& text) { tts_->speak(text); });
    connect(translationCard_, &TranslationCard::useRequested, this, [this](const QString& text) {
        composer_->setPlainText(text);
        composer_->moveCursor(QTextCursor::End);
        composer_->setFocus();
    });

    // ---- 智能建议回复 ----
    reply_ = new ReplyService(this);
    reply_->setClient(llm_);
    connect(reply_, &ReplyService::suggestionsReady, this, [this](const QVector<Suggestion>& items) {
        if (!composer_->toPlainText().trimmed().isEmpty()) return;   // 用户已经在打字了
        translationCard_->hideCard();
        suggestionBar_->showSuggestions(items);
    });
    connect(reply_, &ReplyService::failed, this, [this](const QString&) {
        suggestionBar_->hideBar();
    });
    connect(suggestionBar_, &SuggestionBar::useRequested, this, [this](const QString& text) {
        composer_->setPlainText(text);
        composer_->moveCursor(QTextCursor::End);
        composer_->setFocus();
        suggestionBar_->hideBar();
    });
    connect(suggestionBar_, &SuggestionBar::playRequested, this,
            [this](const QString& text) { tts_->speak(text); });

    replyDebounce_ = new QTimer(this);
    replyDebounce_->setSingleShot(true);
    replyDebounce_->setInterval(800);
    connect(replyDebounce_, &QTimer::timeout, this, &ChatPage::requestSuggestions);

    inputDebounce_ = new QTimer(this);
    inputDebounce_->setSingleShot(true);
    inputDebounce_->setInterval(500);
    connect(inputDebounce_, &QTimer::timeout, this, &ChatPage::onInputDebounced);
    connect(composer_, &QPlainTextEdit::textChanged, this, [this] {
        if (composer_->toPlainText().trimmed().isEmpty()) {
            translationCard_->hideCard();
            return;
        }
        suggestionBar_->hideBar();   // 开始打字就把建议收起来，避免抢位置
        inputDebounce_->start();
    });

    connect(translation_, &TranslationService::failed, this,
            [this](qint64 seq, const QString& reason) {
                if (seq <= 0) {
                    // 输入框的中文翻译失败：卡片不能一直停在「翻译中…」
                    translationCard_->hideCard();
                    showVoiceStatus(reason, true, 3000);
                    return;
                }
                for (ChatItem& item : model_) {
                    if (item.seq == seq) {
                        item.translationState = 3;
                        break;
                    }
                }
                if (MessageBubble* bubble = bubbles_.value(seq, nullptr)) {
                    bubble->setTranslationFailed();
                }
            });
}

// ---------------------------------------------------------------- 进出房间

void ChatPage::enterRoom(const QString& room, bool created, const QVector<MemberInfo>& members,
                         qint64 selfId, const QString& selfName) {
    room_ = room;
    selfId_ = selfId;
    selfName_ = selfName;
    selfIds_.insert(selfId);

    members_.clear();
    for (const MemberInfo& m : members) {
        members_.insert(m.id, m.name);
    }

    roomTitle_->setText(QStringLiteral("房间 %1").arg(room));

    model_.clear();
    seqs_.clear();
    clearView();
    lastDate_ = QDate();

    appendSystemText(created ? QStringLiteral("你创建了房间 %1").arg(room)
                             : QStringLiteral("你加入了房间 %1").arg(room),
                     QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    rebuildMembers();
    setOnline(true);

    composer_->clear();
    translationCard_->hideCard();
    suggestionBar_->hideBar();
    composer_->setFocus();

    // 配置有问题时，进房再提醒一次（启动时的那条容易被错过）。
    // 延后 1.5s：先让「已加入房间」的提示显示完，避免互相覆盖。
    if (!configError_.isEmpty()) {
        QTimer::singleShot(1500, this, [this] { emit notice(configError_, true); });
    }
}

void ChatPage::resumeRoom(const QString& room, const QVector<MemberInfo>& members,
                          qint64 selfId, const QString& selfName) {
    room_ = room;
    selfId_ = selfId;
    selfName_ = selfName;
    selfIds_.insert(selfId);   // 重连后服务端会分配新 id，旧消息仍应显示为自己发的

    members_.clear();
    for (const MemberInfo& m : members) {
        members_.insert(m.id, m.name);
    }

    roomTitle_->setText(QStringLiteral("房间 %1").arg(room));
    appendSystemText(QStringLiteral("已重新连接"), QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    rebuildMembers();
    setOnline(true);
    composer_->setFocus();
}

void ChatPage::clearRoom() {
    room_.clear();
    members_.clear();
    model_.clear();
    seqs_.clear();
    selfIds_.clear();
    lastDate_ = QDate();

    // 语音相关的运行态一并复位，避免播到一半切房间
    VoicePlayer::instance()->stop();
    transcriptQueue_.clear();
    transcribingSeq_ = 0;
    clearView();
    composer_->clear();
    translationCard_->hideCard();
    suggestionBar_->hideBar();
    if (tts_ != nullptr) tts_->stop();
    memberPanel_->setVisible(false);
    setOnline(true);
}

// ---------------------------------------------------------------- 消息

void ChatPage::appendChat(qint64 seq, qint64 from, const QString& name,
                          const QString& text, qint64 ts) {
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    if (seq > 0) {
        if (seqs_.contains(seq)) return;   // 历史补发与实时推送可能重叠
        seqs_.insert(seq);
    }

    if (from != 0 && !members_.contains(from)) {
        members_.insert(from, name);
        rebuildMembers();
    }

    ChatItem item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.text = text;
    item.system = false;

    const bool needTranslation = from != 0 && !selfIds_.contains(from)
                                 && translation_ != nullptr && translation_->isEnabled()
                                 && TranslationService::looksEnglish(text);
    if (needTranslation) item.translationState = 1;

    appendItem(item);
    scrollToBottom();

    if (needTranslation) translation_->requestMessage(seq, text);

    // 他人消息到达且输入框为空 -> 稍后生成 3 条建议
    const bool incoming = (from != 0) && !selfIds_.contains(from);
    if (incoming && reply_ != nullptr && reply_->isEnabled()
        && composer_->toPlainText().trimmed().isEmpty()) {
        replyDebounce_->start();
    }
}

void ChatPage::mergeHistory(const QVector<ChatMessage>& messages) {
    if (messages.isEmpty()) return;

    bool changed = false;
    for (const ChatMessage& m : messages) {
        if (m.seq > 0 && seqs_.contains(m.seq)) continue;
        if (m.seq > 0) seqs_.insert(m.seq);

        // 历史里「同名、且已不在房间」的 id 视为本人旧会话：
        // 服务端每次连接都会分配新 userId，重启客户端后旧消息不该显示成别人的。
        if (m.from != 0 && !selfIds_.contains(m.from)
            && m.name == selfName_ && !members_.contains(m.from)) {
            selfIds_.insert(m.from);
        }

        ChatItem item;
        item.seq = m.seq;
        item.from = m.from;
        item.ts = m.ts > 0 ? m.ts : QDateTime::currentMSecsSinceEpoch();
        item.name = m.name;
        item.system = false;
        item.kind = m.kind;
        if (m.kind == MessageKind::Voice) {
            item.audio = m.audio;
            item.durationMs = m.durationMs;
        } else {
            item.text = m.text;
        }

        model_.push_back(item);
        if (m.from != 0 && !members_.contains(m.from)) {
            members_.insert(m.from, m.name);
        }
        changed = true;
    }

    if (!changed) return;

    sortModel();

    // 历史里只翻最近 12 条英文：一进房就排队几十个请求会拖慢首屏
    if (translation_ != nullptr && translation_->isEnabled()) {
        int budget = historyTranslationBudget_;
        for (int i = model_.size() - 1; i >= 0 && budget > 0; --i) {
            ChatItem& item = model_[i];
            if (item.system || item.translationState != 0) continue;
            if (item.from == 0 || selfIds_.contains(item.from)) continue;
            if (!TranslationService::looksEnglish(item.text)) continue;
            item.translationState = 1;
            --budget;
        }
    }

    rebuildView();
    rebuildMembers();
    scrollToBottom();

    if (translation_ != nullptr && translation_->isEnabled()) {
        for (const ChatItem& item : model_) {
            if (item.translationState == 1) {
                translation_->requestMessage(item.seq, item.text);
            }
        }
    }

    // 历史里的语音也补转写：只处理最近 3 条，避免进房时一次跑太多 whisper
    int voiceBudget = 3;
    for (int i = model_.size() - 1; i >= 0 && voiceBudget > 0; --i) {
        const ChatItem& item = model_.at(i);
        if (item.kind != MessageKind::Voice || item.transcriptState != 0) continue;
        requestVoiceTranscript(item.seq);
        --voiceBudget;
    }
}

void ChatPage::appendSystemText(const QString& text, qint64 ts) {
    if (text.isEmpty()) return;
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    ChatItem item;
    item.ts = ts;
    item.text = text;
    item.system = true;

    appendItem(item);
    scrollToBottom();
}

void ChatPage::onSystemEvent(const QString& event, qint64 userId, const QString& name,
                             const QString& /*oldName*/, const QString& text, qint64 ts) {
    if (event == QStringLiteral("welcome") || event == QStringLiteral("left")) {
        return;
    }

    if (event == QStringLiteral("join")) {
        if (!members_.contains(userId)) {
            members_.insert(userId, name);
            rebuildMembers();
        }
    } else if (event == QStringLiteral("leave")) {
        members_.remove(userId);
        rebuildMembers();
    } else if (event == QStringLiteral("rename")) {
        if (members_.contains(userId)) {
            members_[userId] = name;
            rebuildMembers();
        }
    }

    appendSystemText(text, ts);
}

void ChatPage::appendItem(const ChatItem& item) {
    model_.push_back(item);
    renderItem(item);
}

void ChatPage::renderItem(const ChatItem& item) {
    appendDateSeparatorIfNeeded(item.ts);

    if (item.system) {
        auto* row = new QWidget(canvas_);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 2, 0, 2);

        auto* pill = new QLabel(item.text, row);
        pill->setObjectName("SystemPill");
        pill->setWordWrap(true);
        pill->setAlignment(Qt::AlignCenter);

        pill->ensurePolished();
        const int maxPillWidth = 460;
        const QRect needed = pill->fontMetrics().boundingRect(
            QRect(0, 0, maxPillWidth - 30, 100000), Qt::TextWordWrap | Qt::AlignCenter, item.text);
        pill->setFixedWidth(qBound(60, needed.width() + 30, maxPillWidth));

        rowLayout->addStretch(1);
        rowLayout->addWidget(pill, 0);
        rowLayout->addStretch(1);

        chatLayout_->addWidget(row);
        return;
    }

    const bool outgoing = selfIds_.contains(item.from);

    // ---- 语音消息 ----
    if (item.kind == MessageKind::Voice) {
        auto* voiceBubble = new MessageBubble(item.name, QString(), formatTime(item.ts),
                                              outgoing, item.from, false, canvas_);
        voiceBubble->setVoice(item.audio, item.durationMs);
        connect(voiceBubble, &MessageBubble::voicePlayRequested, this,
                [](const QByteArray& audio) { VoicePlayer::instance()->play(audio); });

        if (item.transcriptState == 1) {
            voiceBubble->setTranscriptPending();
        } else if (item.transcriptState == 2) {
            voiceBubble->setTranscript(item.transcript);
        } else if (item.transcriptState == 3) {
            voiceBubble->setTranscript(QStringLiteral("（未能识别语音内容）"));
        }

        if (item.translationState == 1) {
            voiceBubble->setTranslationPending();
        } else if (item.translationState == 2) {
            voiceBubble->setTranslation(item.translation);
        } else if (item.translationState == 3) {
            voiceBubble->setTranslationFailed();
        }

        if (item.seq > 0) bubbles_.insert(item.seq, voiceBubble);
        chatLayout_->addWidget(voiceBubble);
        return;
    }

    const bool playable = !outgoing && TranslationService::looksEnglish(item.text);
    auto* bubble = new MessageBubble(item.name, item.text, formatTime(item.ts),
                                     outgoing, item.from, playable, canvas_);
    connect(bubble, &MessageBubble::playRequested, this,
            [this, bubble](const QString& text) {
                if (playingBubble_ != nullptr && playingBubble_ != bubble) {
                    playingBubble_->setPlaying(false);
                }
                playingBubble_ = bubble;
                tts_->speak(text);
            });
    if (item.translationState == 1) {
        bubble->setTranslationPending();
    } else if (item.translationState == 2) {
        bubble->setTranslation(item.translation);
    } else if (item.translationState == 3) {
        bubble->setTranslationFailed();
    }
    if (item.seq > 0) bubbles_.insert(item.seq, bubble);
    chatLayout_->addWidget(bubble);
}

void ChatPage::sortModel() {
    std::stable_sort(model_.begin(), model_.end(), [](const ChatItem& a, const ChatItem& b) {
        if (a.ts != b.ts) return a.ts < b.ts;
        return a.seq < b.seq;
    });
}

void ChatPage::rebuildView() {
    clearView();
    lastDate_ = QDate();
    for (const ChatItem& item : model_) {
        renderItem(item);
    }
}

void ChatPage::clearView() {
    bubbles_.clear();
    playingBubble_ = nullptr;
    // 索引 0 是弹簧，必须保留
    while (chatLayout_->count() > 1) {
        QLayoutItem* item = chatLayout_->takeAt(chatLayout_->count() - 1);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }
}

void ChatPage::appendDateSeparatorIfNeeded(qint64 ts) {
    const QDate date = QDateTime::fromMSecsSinceEpoch(ts).date();
    if (date == lastDate_) return;
    lastDate_ = date;

    auto* row = new QWidget(canvas_);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 6, 0, 2);

    auto* pill = new QLabel(formatDate(date), row);
    pill->setObjectName("DatePill");
    pill->setAlignment(Qt::AlignCenter);

    rowLayout->addStretch(1);
    rowLayout->addWidget(pill, 0);
    rowLayout->addStretch(1);

    chatLayout_->addWidget(row);
}

void ChatPage::scrollToBottom() {
    pendingScroll_ = true;
    QTimer::singleShot(0, this, [this] { forceScrollToBottom(); });
    // 布局是异步结算的：内容高度变化后 rangeChanged 会再触发一次贴底
    QTimer::singleShot(150, this, [this] { pendingScroll_ = false; });
}

void ChatPage::forceScrollToBottom() {
    QScrollBar* bar = scroll_->verticalScrollBar();
    bar->setValue(bar->maximum());
}

void ChatPage::sendCurrentText() {
    if (!composer_->isEnabled()) return;
    const QString text = composer_->toPlainText().trimmed();
    if (text.isEmpty()) return;
    emit sendRequested(text);
    composer_->clear();
    composer_->setFocus();
}

void ChatPage::setOnline(bool online) {
    composer_->setEnabled(online);
    sendBtn_->setEnabled(online);
    micBtn_->setEnabled(online && voiceAvailable_);
    composer_->setPlaceholderText(
        online ? QString::fromUtf8(kComposerHint)
               : QStringLiteral("连接已断开，正在重连…"));
}

// ---------------------------------------------------------------- 语音输入

void ChatPage::requestSuggestions() {
    if (reply_ == nullptr || !reply_->isEnabled()) return;

    // 取最近 6 条聊天（跳过系统提示）作为上下文
    QVector<ChatMessage> context;
    for (int i = model_.size() - 1; i >= 0 && context.size() < 6; --i) {
        const ChatItem& item = model_.at(i);
        if (item.system) continue;
        ChatMessage message;
        message.seq = item.seq;
        message.from = item.from;
        message.name = item.name;
        message.text = item.text;
        message.ts = item.ts;
        context.prepend(message);
    }
    if (context.isEmpty()) return;

    suggestionBar_->showPending();
    reply_->request(context);
}

void ChatPage::onInputDebounced() {
    const QString text = composer_->toPlainText().trimmed();
    if (text.isEmpty() || !TranslationService::looksChinese(text)) {
        translationCard_->hideCard();
        return;
    }
    if (translation_ == nullptr || !translation_->isEnabled()) return;

    suggestionBar_->hideBar();
    translationCard_->showPending();
    translation_->requestInput(text);
}

void ChatPage::toggleRecording() {
    if (voice_ == nullptr) return;
    if (voice_->isRecording()) {
        voice_->stopRecording();
        return;
    }

    // 语音消息：录完直接把原始音频发出去，不再走「识别成文字填进输入框」
    voice_->startRecording(VoiceInput::Purpose::Send);
}

void ChatPage::onVoiceRecorded(const QByteArray& pcm, int durationMs) {
    if (pcm.isEmpty()) return;

    const QByteArray container = VoiceCodec::encode(pcm);
    if (container.isEmpty()) {
        showVoiceStatus(QStringLiteral("语音编码失败：%1").arg(VoiceCodec::lastError()), true, 4000);
        return;
    }
    if (container.size() > static_cast<int>(dp::kMaxVoiceBytes)) {
        showVoiceStatus(QStringLiteral("语音太长（%1 秒），请控制在 %2 秒内")
                            .arg(durationMs / 1000)
                            .arg(dp::kMaxVoiceDurationMs / 1000),
                        true, 4000);
        return;
    }

    emit sendVoiceRequested(container, durationMs);
    showVoiceStatus(QStringLiteral("已发送语音（%1 秒）").arg(durationMs / 1000.0, 0, 'f', 1),
                    false, 2000);
}

void ChatPage::requestVoiceTranscript(qint64 seq) {
    if (seq <= 0) return;

    for (ChatItem& item : model_) {
        if (item.seq != seq) continue;
        if (item.kind != MessageKind::Voice) return;
        if (item.transcriptState != 0) return;   // 已在转写或已完成
        item.transcriptState = 1;
        break;
    }

    transcriptQueue_.enqueue(seq);
    pumpTranscript();
}

void ChatPage::pumpTranscript() {
    if (transcribingSeq_ > 0 || transcriptQueue_.isEmpty()) return;

    if (voice_ == nullptr || !voice_->modelReady()) {
        // whisper 还在加载：过一会儿再试，别把队列丢了
        QTimer::singleShot(1500, this, &ChatPage::pumpTranscript);
        return;
    }

    while (!transcriptQueue_.isEmpty()) {
        const qint64 seq = transcriptQueue_.dequeue();

        const ChatItem* target = nullptr;
        for (const ChatItem& item : model_) {
            if (item.seq == seq) {
                target = &item;
                break;
            }
        }
        if (target == nullptr || target->transcriptState != 1) continue;

        const QByteArray pcm = VoiceCodec::decode(target->audio);
        if (pcm.isEmpty()) {
            updateTranscript(seq, QString(), false);
            continue;
        }

        const QString dir =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/voice");
        QDir().mkpath(dir);
        const QString wav = QStringLiteral("%1/seq_%2.wav").arg(dir).arg(seq);

        QFile file(wav);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            updateTranscript(seq, QString(), false);
            continue;
        }
        file.write(VoiceCodec::toWav(pcm));
        file.close();

        transcribingSeq_ = seq;
        voice_->transcribeFile(wav);
        return;   // 一次只跑一条，textReady 里再取下一条
    }
}

void ChatPage::updateTranscript(qint64 seq, const QString& text, bool ok) {
    for (ChatItem& item : model_) {
        if (item.seq != seq) continue;
        if (ok && !text.isEmpty()) {
            item.transcript = text;
            item.transcriptState = 2;
            // 转写出来的是英文，接着走翻译
            if (translation_ != nullptr && translation_->isEnabled()
                && TranslationService::looksEnglish(text)) {
                item.translationState = 1;
                translation_->requestMessage(seq, text);
            }
        } else {
            item.transcriptState = 3;
        }
        break;
    }

    if (MessageBubble* bubble = bubbles_.value(seq, nullptr)) {
        if (ok && !text.isEmpty()) {
            bubble->setTranscript(text);
            if (translation_ != nullptr && translation_->isEnabled()
                && TranslationService::looksEnglish(text)) {
                bubble->setTranslationPending();
            }
        } else {
            bubble->setTranscript(QStringLiteral("（未能识别语音内容）"));
        }
    }
}

void ChatPage::appendVoice(qint64 seq, qint64 from, const QString& name,
                           const QByteArray& audio, int durationMs, qint64 ts) {
    if (audio.isEmpty()) return;
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    if (seq > 0) {
        if (seqs_.contains(seq)) return;
        seqs_.insert(seq);
    }

    if (from != 0 && !members_.contains(from)) {
        members_.insert(from, name);
        rebuildMembers();
    }

    ChatItem item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.kind = MessageKind::Voice;
    item.audio = audio;
    item.durationMs = durationMs;
    item.system = false;

    appendItem(item);
    scrollToBottom();

    // 语音也要能看懂：本地 whisper 转写成英文，再翻译成中文
    requestVoiceTranscript(seq);

    const bool incoming = (from != 0) && !selfIds_.contains(from);
    if (incoming && reply_ != nullptr && reply_->isEnabled()
        && composer_->toPlainText().trimmed().isEmpty()) {
        replyDebounce_->start();
    }
}

void ChatPage::insertVoiceText(const QString& text) {
    if (text.isEmpty()) return;

    QString payload = text;
    const QString existing = composer_->toPlainText();
    if (!existing.isEmpty()
        && !existing.endsWith(QLatin1Char(' '))
        && !existing.endsWith(QLatin1Char('\n'))) {
        payload.prepend(QLatin1Char(' '));
    }

    QTextCursor cursor = composer_->textCursor();
    cursor.insertText(payload);
    composer_->setTextCursor(cursor);
    composer_->setFocus();
}

namespace {
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

void ChatPage::applyConfig(const AppConfig::Settings& settings, bool initial) {
    // 对话/建议用主模型
    llm_->setConfig(toClientConfig(settings.llm));

    // 翻译：配了专用模型就用它，否则沿用主模型参数
    const bool separate = settings.translation.hasOwnLlm;
    translateLlm_->setConfig(toClientConfig(separate ? settings.translation.llm : settings.llm));

    historyTranslationBudget_ = qMax(0, settings.translation.maxHistoryMessages);
    configError_ = settings.error;

    if (!settings.error.isEmpty()) {
        if (initial) {
            // 构造期发出会丢（MainWindow 尚未 connect），推到事件循环第一轮
            const QString message = settings.error;
            QTimer::singleShot(0, this, [this, message] { emit notice(message, true); });
        } else {
            emit notice(settings.error, true);
        }
    } else if (!initial) {
        const QString translateModel = separate ? settings.translation.llm.model
                                                : settings.llm.model;
        emit notice(QStringLiteral("配置已重新加载：对话 %1 / 翻译 %2")
                        .arg(settings.llm.model, translateModel),
                    false);
    }

    // 之前因为没配 Key 而失败的条目，填好之后自动补翻
    if (translateLlm_->isEnabled()) retryFailedTranslations();
}

void ChatPage::retryFailedTranslations() {
    if (translation_ == nullptr || !translation_->isEnabled()) return;

    QVector<QPair<qint64, QString>> retry;
    int budget = historyTranslationBudget_;
    for (int i = model_.size() - 1; i >= 0 && budget > 0; --i) {
        ChatItem& item = model_[i];
        if (item.system) continue;
        // 3 = 之前失败；0 = 当时翻译没启用，配置改好后一起补
        if (item.translationState != 3 && item.translationState != 0) continue;
        if (item.from == 0 || selfIds_.contains(item.from)) continue;
        if (!TranslationService::looksEnglish(item.text)) continue;
        item.translationState = 1;
        if (MessageBubble* bubble = bubbles_.value(item.seq, nullptr)) {
            bubble->setTranslationPending();
        }
        retry.prepend(qMakePair(item.seq, item.text));
        --budget;
    }
    for (const auto& pair : retry) {
        translation_->requestMessage(pair.first, pair.second);
    }
}

void ChatPage::showVoiceStatus(const QString& text, bool error, int autoHideMs) {
    if (voiceStatus_ == nullptr) return;

    ++voiceStatusToken_;
    const int token = voiceStatusToken_;

    if (text.isEmpty()) {   // 传空串表示清掉状态条
        voiceStatus_->clear();
        voiceStatus_->setVisible(false);
        return;
    }

    voiceStatus_->setText(text);
    voiceStatus_->setStyleSheet(
        error ? QStringLiteral("color:#E5484D; background:#FDF2F2; padding:6px 18px; font-size:9pt;")
              : QStringLiteral("color:#5B6675; background:#F2F5F9; padding:6px 18px; font-size:9pt;"));
    voiceStatus_->setVisible(true);

    if (autoHideMs > 0) {
        QTimer::singleShot(autoHideMs, this, [this, token] {
            if (token == voiceStatusToken_) voiceStatus_->setVisible(false);
        });
    }
}

// ---------------------------------------------------------------- 成员

void ChatPage::updateHeader() {
    roomSubtitle_->setText(QStringLiteral("%1 位成员").arg(members_.size()));
    memberBtn_->setText(QStringLiteral("成员 %1").arg(members_.size()));
    if (memberTitle_ != nullptr) {
        memberTitle_->setText(QStringLiteral("成员（%1）").arg(members_.size()));
    }
}

void ChatPage::rebuildMembers() {
    while (memberListLayout_->count() > 1) {
        QLayoutItem* item = memberListLayout_->takeAt(0);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }

    for (auto it = members_.constBegin(); it != members_.constEnd(); ++it) {
        auto* row = new QWidget();
        row->setObjectName("MemberRow");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(6, 4, 6, 4);
        rowLayout->setSpacing(8);

        auto* avatar = new Avatar(row);
        avatar->setFixedSize(28, 28);
        avatar->setFontPx(12);
        avatar->setText(it.value());
        avatar->setColor(Avatar::colorForId(it.key()));

        auto* nameLabel = new QLabel(it.value(), row);
        nameLabel->setStyleSheet(QStringLiteral("font-size: 9.5pt;"));
        if (selfIds_.contains(it.key())) {
            nameLabel->setText(it.value() + QStringLiteral("（我）"));
        }

        rowLayout->addWidget(avatar);
        rowLayout->addWidget(nameLabel, 1);

        memberListLayout_->insertWidget(memberListLayout_->count() - 1, row);
    }

    updateHeader();
}
