/**
 * @file  ChatPage.cpp
 * @brief 聊天界面的实现：控件构建、信号接线与渲染。
 */

#include "ui/ChatPage.h"

#include "business/ChatController.h"
#include "business/ChatModel.h"
#include "business/TranslationService.h"
#include "core/Logger.h"
#include "core/UiMetrics.h"
#include "core/Theme.h"
#include "media/TtsService.h"
#include "media/VoicePlayer.h"
#include "media/VoiceRecorder.h"
#include "ui/widgets/Avatar.h"
#include "ui/widgets/MessageBubble.h"
#include "ui/widgets/MicButton.h"
#include "ui/widgets/SendButton.h"
#include "ui/widgets/SuggestionBar.h"
#include "ui/widgets/TranslationCard.h"

#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const QString kTag = QStringLiteral("ui");
const char* kComposerHint = "输入消息，Enter 发送，Shift+Enter 换行";

/// @brief 把时间戳格式化成 `HH:mm`。
QString formatTime(qint64 ts) {
    return QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("HH:mm"));
}

/// @brief 把日期格式化成「今天 / 昨天 / M月d日」。
QString formatDate(const QDate& date) {
    const QDate today = QDate::currentDate();
    if (date == today) return QStringLiteral("今天");
    if (date == today.addDays(-1)) return QStringLiteral("昨天");
    if (date.year() == today.year()) return date.toString(QStringLiteral("M月d日"));
    return date.toString(QStringLiteral("yyyy年M月d日"));
}

/// @brief 模型路径优先级：QSettings 覆盖 > 编译期默认（源码树 models/ 目录）。
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

// ---------------------------------------------------------------- 构造

ChatPage::ChatPage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    model_ = new business::ChatModel(this);
    controller_ = new business::ChatController(model_, this);

    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ---- 左侧：聊天列 ----
    auto* chatColumn = new QWidget(this);
    auto* columnLayout = new QVBoxLayout(chatColumn);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(0);

    buildHeader(columnLayout);
    buildMessageArea(columnLayout);
    buildComposer(columnLayout);

    root->addWidget(chatColumn, 1);

    buildMemberPanel();
    root->addWidget(memberPanel_);

    wireSignals();

    voice_->start();   // 模型在后台线程加载，不阻塞界面
}

ChatPage::~ChatPage() = default;

void ChatPage::buildHeader(QVBoxLayout* columnLayout) {
    auto* header = new QFrame(this);
    header->setObjectName("ChatHeader");
    header->setFixedHeight(64);

    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 0, 16, 0);
    headerLayout->setSpacing(10);

    auto* backBtn = new QPushButton(QStringLiteral("←"), header);
    backBtn->setObjectName("LinkButton");
    backBtn->setCursor(Qt::PointingHandCursor);
    backBtn->setFixedSize(core::ui::kTouchTarget, core::ui::kTouchTarget);
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

    columnLayout->addWidget(header);

    connect(backBtn, &QPushButton::clicked, this, &ChatPage::backRequested);
    connect(memberBtn_, &QPushButton::clicked, this, [this] {
        // 窄屏没有空间放侧栏，改成弹窗；宽屏保持原来的侧栏切换
        if (core::ui::isCompact(width())) {
            showMemberDialog();
            return;
        }
        memberPanel_->setVisible(!memberPanel_->isVisible());
    });
}

void ChatPage::buildMessageArea(QVBoxLayout* columnLayout) {
    scroll_ = new QScrollArea(this);
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
    columnLayout->addWidget(scroll_, 1);

    QScrollBar* bar = scroll_->verticalScrollBar();

    // 内容或视口高度变化时决定要不要跟着贴底。
    //
    // 关键场景：建议条/译文卡弹出会把消息区变矮，maximum 随之增大。
    // 此时若用户本来就在底部，必须继续贴底，否则位置不变就会「停在中间」——
    // 表现为新到的消息明明渲染了却看不见，必须手动滚一下。
    connect(bar, &QScrollBar::rangeChanged, this, [this](int, int max) {
        if (pendingScroll_ || stickToBottom_) {
            scroll_->verticalScrollBar()->setValue(max);
        }
    });

    // 用户手动滚动时记录是否停在底部（留 4px 容差，避免浮点/整型误差）
    connect(bar, &QScrollBar::valueChanged, this, [this](int value) {
        stickToBottom_ = (value >= scroll_->verticalScrollBar()->maximum() - 4);
    });
}

void ChatPage::buildComposer(QVBoxLayout* columnLayout) {
    suggestionBar_ = new SuggestionBar(this);
    columnLayout->addWidget(suggestionBar_);

    translationCard_ = new TranslationCard(this);
    columnLayout->addWidget(translationCard_);

    voiceStatus_ = new QLabel(this);
    voiceStatus_->setObjectName("VoiceStatus");
    voiceStatus_->setVisible(false);
    columnLayout->addWidget(voiceStatus_);

    auto* composerFrame = new QFrame(this);
    composerFrame->setObjectName("Composer");
    auto* composerLayout = new QHBoxLayout(composerFrame);
    composerLayout->setContentsMargins(16, 10, 16, 10);
    composerLayout->setSpacing(10);

    micBtn_ = new MicButton(composerFrame);
    composerLayout->addWidget(micBtn_, 0, Qt::AlignVCenter);

    composer_ = new ComposerEdit(composerFrame);
    composer_->setObjectName("ComposerEdit");
    updateComposerPlaceholder();
    composer_->setFixedHeight(46);
    composerLayout->addWidget(composer_, 1);

    sendBtn_ = new SendButton(composerFrame);
    composerLayout->addWidget(sendBtn_, 0, Qt::AlignVCenter);

    columnLayout->addWidget(composerFrame);

    connect(sendBtn_, &SendButton::clicked, this, &ChatPage::sendCurrentText);
    connect(composer_, &ComposerEdit::submit, this, &ChatPage::sendCurrentText);
}

void ChatPage::buildMemberPanel() {
    memberPanel_ = new QFrame(this);
    memberPanel_->setObjectName("MemberPanel");
    memberPanel_->setFixedWidth(220);
    memberPanel_->setVisible(false);

    auto* panelLayout = new QVBoxLayout(memberPanel_);
    panelLayout->setContentsMargins(12, 14, 12, 14);
    panelLayout->setSpacing(8);

    memberTitle_ = new QLabel(memberPanel_);
    memberTitle_->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: 700;"));
    panelLayout->addWidget(memberTitle_);

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

    panelLayout->addWidget(memberScroll, 1);
}

// ---------------------------------------------------------------- 接线

void ChatPage::wireSignals() {
    // ---- 模型 -> 视图 ----
    connect(model_, &business::ChatModel::itemAppended, this, [this](const business::ChatItem& item) {
        renderItem(item);
        scrollToBottom();
    });
    connect(model_, &business::ChatModel::modelReset, this, &ChatPage::rebuildView);
    connect(model_, &business::ChatModel::itemUpdated, this, &ChatPage::refreshBubble);
    connect(model_, &business::ChatModel::membersChanged, this, &ChatPage::rebuildMembers);

    // ---- 编排器 -> 视图 ----
    connect(controller_, &business::ChatController::notice, this, &ChatPage::notice);
    connect(controller_, &business::ChatController::voiceStatus, this, &ChatPage::showVoiceStatus);
    connect(controller_, &business::ChatController::suggestionsReady, this,
            [this](const QVector<Suggestion>& items) {
                translationCard_->hideCard();
                suggestionBar_->showSuggestions(items);
            });
    connect(controller_, &business::ChatController::suggestionsCleared, this,
            [this] { suggestionBar_->hideBar(); });
    connect(controller_, &business::ChatController::inputTranslationReady, this,
            [this](const QString& source, const QString& english) {
                // 用户已经改了输入内容就丢弃这次结果
                if (source != composer_->toPlainText().trimmed()) return;
                translationCard_->showTranslation(english);
            });
    connect(controller_, &business::ChatController::inputTranslationFailed, this,
            [this](const QString& reason) {
                translationCard_->hideCard();
                showVoiceStatus(reason, true, 3000);
            });
    connect(controller_, &business::ChatController::sendVoiceRequested, this,
            &ChatPage::sendVoiceRequested);

    // ---- 语音输入 ----
    voice_ = new VoiceRecorder(this);
    voice_->setModelPath(resolveModelPath());
    controller_->setRecorder(voice_);

    connect(micBtn_, &QAbstractButton::clicked, this, &ChatPage::toggleRecording);
    connect(voice_, &VoiceRecorder::modelLoaded, this, [this](bool ok, const QString& message) {
        voiceAvailable_ = ok;
        if (!ok) {
            micBtn_->setEnabled(false);
            micBtn_->setToolTip(message);
            showVoiceStatus(message, true, 6000);
        }
    });
    connect(voice_, &VoiceRecorder::pcmReady, this, &ChatPage::onVoiceRecorded);
    connect(voice_, &VoiceRecorder::recordingStarted, this, [this] {
        micBtn_->setRecording(true);
        showVoiceStatus(QStringLiteral("● 正在录音 0:00 · 再点一次结束"), false, 0);
    });
    connect(voice_, &VoiceRecorder::recordingTick, this, [this](int seconds, int /*level*/) {
        showVoiceStatus(QStringLiteral("● 正在录音 0:%1 · 再点一次结束")
                            .arg(seconds, 2, 10, QLatin1Char('0')),
                        false, 0);
    });
    connect(voice_, &VoiceRecorder::recognizing, this, [this] {
        micBtn_->setRecording(false);
        showVoiceStatus(QStringLiteral("识别中…"), false, 0);
    });
    connect(voice_, &VoiceRecorder::failed, this, [this](const QString& message) {
        micBtn_->setRecording(false);
        showVoiceStatus(message, true, 3500);
    });

    // ---- 语音消息播放 ----
    connect(VoicePlayer::instance(), &VoicePlayer::playingChanged, this, [this](bool playing) {
        const QByteArray current = VoicePlayer::instance()->current();
        for (auto it = bubbles_.constBegin(); it != bubbles_.constEnd(); ++it) {
            const business::ChatItem* item = model_->find(it.key());
            if (item == nullptr || item->kind != MessageKind::Voice) continue;
            it.value()->setPlaying(playing && !current.isEmpty() && item->audio == current);
        }
    });
    connect(VoicePlayer::instance(), &VoicePlayer::failed, this, [this](const QString& reason) {
        showVoiceStatus(reason, true, 3000);
    });

    // ---- 语音合成 ----
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

    // ---- 译文卡 / 建议条 ----
    connect(translationCard_, &TranslationCard::playRequested, this,
            [this](const QString& text) { tts_->speak(text); });
    connect(translationCard_, &TranslationCard::useRequested, this, [this](const QString& text) {
        composer_->setPlainText(text);
        composer_->moveCursor(QTextCursor::End);
        composer_->setFocus();
    });
    connect(suggestionBar_, &SuggestionBar::useRequested, this, [this](const QString& text) {
        composer_->setPlainText(text);
        composer_->moveCursor(QTextCursor::End);
        composer_->setFocus();
        suggestionBar_->hideBar();
    });
    connect(suggestionBar_, &SuggestionBar::playRequested, this,
            [this](const QString& text) { tts_->speak(text); });

    // ---- 输入框 ----
    connect(composer_, &QPlainTextEdit::textChanged, this, [this] {
        syncComposerState();

        const QString text = composer_->toPlainText().trimmed();
        if (text.isEmpty()) {
            translationCard_->hideCard();
            return;
        }
        suggestionBar_->hideBar();   // 开始打字就把建议收起来，避免抢位置

        if (TranslationService::looksChinese(text)) {
            translationCard_->showPending();
            controller_->requestInputTranslation(text);
        } else {
            translationCard_->hideCard();
        }
    });
}

// ---------------------------------------------------------------- 生命周期

void ChatPage::enterRoom(const QString& room, bool created, const QVector<MemberInfo>& members,
                         qint64 selfId, const QString& selfName) {
    model_->enterRoom(room, selfId, selfName, members);

    roomTitle_->setText(QStringLiteral("房间 %1").arg(room));
    updateHeader();

    appendSystemText(created ? QStringLiteral("你创建了房间 %1").arg(room)
                             : QStringLiteral("你加入了房间 %1").arg(room),
                     QDateTime::currentMSecsSinceEpoch());

    composer_->clear();
    translationCard_->hideCard();
    suggestionBar_->hideBar();
    composer_->setFocus();
    setOnline(true);
    syncComposerState();
}

void ChatPage::resumeRoom(const QString& room, const QVector<MemberInfo>& members,
                          qint64 selfId, const QString& selfName) {
    model_->resumeRoom(room, selfId, selfName, members);

    roomTitle_->setText(QStringLiteral("房间 %1").arg(room));
    appendSystemText(QStringLiteral("已重新连接"), QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    setOnline(true);
    composer_->setFocus();
}

void ChatPage::clearRoom() {
    model_->clear();

    // 语音相关的运行态一并复位，避免播到一半切房间
    VoicePlayer::instance()->stop();
    if (tts_ != nullptr) tts_->stop();

    clearView();
    composer_->clear();
    translationCard_->hideCard();
    suggestionBar_->hideBar();
    memberPanel_->setVisible(false);
    setOnline(true);
    syncComposerState();
}

void ChatPage::applyRemoteConfig(const QByteArray& json) {
    if (controller_ != nullptr) controller_->applyRemoteConfig(json);
}

QString ChatPage::room() const {
    return model_ != nullptr ? model_->room() : QString();
}

// ---------------------------------------------------------------- 消息投递

void ChatPage::appendChat(qint64 seq, qint64 from, const QString& name, const QString& text,
                          qint64 ts) {
    model_->appendChat(seq, from, name, text, ts);
}

void ChatPage::appendVoice(qint64 seq, qint64 from, const QString& name,
                           const QByteArray& audio, int durationMs, qint64 ts) {
    model_->appendVoice(seq, from, name, audio, durationMs, ts);
}

void ChatPage::mergeHistory(const QVector<ChatMessage>& messages) {
    model_->mergeHistory(messages);
}

void ChatPage::appendSystemText(const QString& text, qint64 ts) {
    model_->appendSystem(text, ts);
}

void ChatPage::onSystemEvent(const QString& event, qint64 userId, const QString& name,
                             const QString& /*oldName*/, const QString& text, qint64 ts) {
    model_->applySystemEvent(event, userId, name, text, ts);
}

// ---------------------------------------------------------------- 渲染

void ChatPage::renderItem(const business::ChatItem& item) {
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
        pinHeight(row);
        return;
    }

    const bool outgoing = model_->isSelf(item.from);
    // 气泡宽度跟着消息区走：窄屏按比例收缩，宽屏保持 520 上限
    const int bubbleWidth = core::ui::bubbleMaxWidth(canvas_->width());

    // ---- 语音消息 ----
    if (item.kind == MessageKind::Voice) {
        auto* bubble = new MessageBubble(item.name, QString(), formatTime(item.ts),
                                         outgoing, item.from, false, bubbleWidth, canvas_);
        bubble->setVoice(item.audio, item.durationMs);
        connect(bubble, &MessageBubble::voicePlayRequested, this,
                [](const QByteArray& audio) { VoicePlayer::instance()->play(audio); });

        if (item.seq > 0) bubbles_.insert(item.seq, bubble);
        chatLayout_->addWidget(bubble);
        pinHeight(bubble);
        refreshBubble(item.seq);
        return;
    }

    // ---- 文本消息 ----
    const bool playable = !outgoing && TranslationService::looksEnglish(item.text);
    auto* bubble = new MessageBubble(item.name, item.text, formatTime(item.ts),
                                     outgoing, item.from, playable, bubbleWidth, canvas_);
    connect(bubble, &MessageBubble::playRequested, this, [this, bubble](const QString& text) {
        if (playingBubble_ != nullptr && playingBubble_ != bubble) {
            playingBubble_->setPlaying(false);
        }
        playingBubble_ = bubble;
        tts_->speak(text);
    });

    if (item.seq > 0) bubbles_.insert(item.seq, bubble);
    chatLayout_->addWidget(bubble);
    pinHeight(bubble);
    refreshBubble(item.seq);
}

void ChatPage::refreshBubble(qint64 seq) {
    if (seq <= 0) return;

    MessageBubble* bubble = bubbles_.value(seq, nullptr);
    const business::ChatItem* item = model_->find(seq);
    if (bubble == nullptr || item == nullptr) return;

    if (item->kind == MessageKind::Voice) {
        switch (item->transcriptState) {
            case 1: bubble->setTranscriptPending(); break;
            case 2: bubble->setTranscript(item->transcript); break;
            case 3: bubble->setTranscript(QStringLiteral("（未能识别语音内容）")); break;
            default: break;
        }
    }

    switch (item->translationState) {
        case 1: bubble->setTranslationPending(); break;
        case 2: bubble->setTranslation(item->translation); break;
        case 3: bubble->setTranslationFailed(); break;
        default: break;
    }
}

void ChatPage::rebuildView() {
    clearView();
    lastDate_ = QDate();
    for (const business::ChatItem& item : model_->items()) {
        renderItem(item);
    }
    scrollToBottom();
}

void ChatPage::clearView() {
    bubbles_.clear();
    playingBubble_ = nullptr;
    stickToBottom_ = true;
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
    pinHeight(row);
}

void ChatPage::scrollToBottom() {
    pendingScroll_ = true;
    stickToBottom_ = true;

    // QScrollArea 在 widgetResizable 下按 sizeHint 决定内容高度，而带 stretch 的
    // QVBoxLayout 在布局未结算时 sizeHint 偏小（自绘控件更明显），结果滚动条
    // maximum 恒为 0——消息加进去了却滚不到。这里直接累加每个子控件的实际高度，
    // 不依赖 sizeHint，绕开这个坑。
    if (canvas_ != nullptr && chatLayout_ != nullptr) {
        chatLayout_->activate();

        const QMargins margins = chatLayout_->contentsMargins();
        int contentHeight = margins.top() + margins.bottom();
        int visibleCount = 0;

        for (int i = 0; i < chatLayout_->count(); ++i) {
            QLayoutItem* entry = chatLayout_->itemAt(i);
            if (entry == nullptr) continue;

            if (QWidget* w = entry->widget()) {
                const int h = (w->height() > 0) ? w->height() : w->sizeHint().height();
                contentHeight += h;
                ++visibleCount;
            }
        }
        if (visibleCount > 1) {
            contentHeight += chatLayout_->spacing() * (visibleCount - 1);
        }

        if (contentHeight > 0 && canvas_->minimumHeight() != contentHeight) {
            canvas_->setMinimumHeight(contentHeight);
        }
    }

    // 布局是异步结算的：addWidget 之后要等一轮事件循环，viewport 高度才更新，
    // 语音条这类自绘控件更晚。所以分多个时间点各贴一次底，
    // 而不是只依赖单次 setValue（那样会停在中间）。
    for (int delayMs : {0, 60, 200}) {
        QTimer::singleShot(delayMs, this, [this] {
            if (!pendingScroll_) return;

            // 优先让最后一个控件可见：QScrollArea 会自己完成必要的布局结算。
            // 注意不要再叠加 setValue(maximum())——此时 maximum() 可能仍是旧值，
            // 会把刚滚下去的位置又拉回来，表现就是「停在中间看不到新消息」。
            if (chatLayout_ != nullptr && chatLayout_->count() > 1) {
                QLayoutItem* last = chatLayout_->itemAt(chatLayout_->count() - 1);
                if (last != nullptr && last->widget() != nullptr) {
                    scroll_->ensureWidgetVisible(last->widget(), 0, 24);
                }
            }
            // 兜底：按内容坐标滚到最底，覆盖 ensureWidgetVisible 判定「已可见」的情况
            if (canvas_ != nullptr) {
                scroll_->ensureVisible(0, qMax(0, canvas_->height() - 1), 0, 0);
            }
            forceScrollToBottom();
        });
    }

    if (scrollSettleTimer_ == nullptr) {
        scrollSettleTimer_ = new QTimer(this);
        scrollSettleTimer_->setSingleShot(true);
        connect(scrollSettleTimer_, &QTimer::timeout, this, [this] {
            forceScrollToBottom();
            pendingScroll_ = false;   // 结算完成，之后不再强制贴底
        });
    }
    scrollSettleTimer_->start(450);
}

void ChatPage::pinHeight(QWidget* widget) {
    if (widget == nullptr) return;
    // 必须在 addWidget 之后调用：没有 parent 时 sizeHint 还没按可用宽度换行计算，
    // 拿到的高度会偏小，钉死之后内容就被裁掉。
    if (QLayout* layout = widget->layout()) layout->activate();
    const int height = widget->sizeHint().height();
    if (height > 0) {
        widget->setMinimumHeight(height);
        widget->updateGeometry();
    }
}

void ChatPage::forceScrollToBottom() {
    // 必须先把布局激活：刚 addWidget 时 maximum() 还是旧值，
    // 直接 setValue 会停在中间（语音条这类高度延迟确定的控件尤其明显）。
    if (chatLayout_ != nullptr) chatLayout_->activate();

    QScrollBar* bar = scroll_->verticalScrollBar();
    bar->setValue(bar->maximum());
}

// ---------------------------------------------------------------- 交互

void ChatPage::sendCurrentText() {
    if (!composer_->isEnabled()) return;

    const QString text = composer_->toPlainText().trimmed();
    if (text.isEmpty()) return;

    emit sendRequested(text);
    composer_->clear();
    composer_->setFocus();
}

void ChatPage::toggleRecording() {
    if (voice_ == nullptr) return;

    if (voice_->isRecording()) {
        voice_->stopRecording();
        return;
    }

    // 语音消息：录完直接把原始音频发出去，不再走「识别成文字填进输入框」
    voice_->startRecording(VoiceRecorder::Purpose::Send);
}

void ChatPage::onVoiceRecorded(const QByteArray& pcm, int durationMs) {
    if (pcm.isEmpty()) return;
    micBtn_->setRecording(false);
    controller_->sendVoice(pcm, durationMs);
}

void ChatPage::setOnline(bool online) {
    composer_->setEnabled(online);
    sendBtn_->setEnabled(online);
    micBtn_->setEnabled(online && voiceAvailable_);
    updateComposerPlaceholder();
}

void ChatPage::syncComposerState() {
    if (controller_ == nullptr) return;
    controller_->setComposerEmpty(composer_->toPlainText().trimmed().isEmpty());
}

// ---------------------------------------------------------------- 状态提示

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
        error
            ? QStringLiteral("color:#E5484D; background:#FDF2F2; padding:6px 18px; font-size:9pt;")
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
    const int count = (model_ != nullptr) ? model_->memberCount() : 0;
    roomSubtitle_->setText(QStringLiteral("%1 位成员").arg(count));
    memberBtn_->setText(QStringLiteral("成员 %1").arg(count));
    if (memberTitle_ != nullptr) {
        memberTitle_->setText(QStringLiteral("成员（%1）").arg(count));
    }
}

void ChatPage::rebuildMembers() {
    if (model_ == nullptr || memberListLayout_ == nullptr) return;

    while (memberListLayout_->count() > 1) {
        QLayoutItem* item = memberListLayout_->takeAt(0);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }

    const QMap<qint64, QString>& members = model_->members();
    for (auto it = members.constBegin(); it != members.constEnd(); ++it) {
        memberListLayout_->insertWidget(memberListLayout_->count() - 1,
                                        createMemberRow(it.key(), it.value()));
    }

    updateHeader();
}

QWidget* ChatPage::createMemberRow(qint64 userId, const QString& name) {
    auto* row = new QWidget();
    row->setObjectName("MemberRow");

    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(6, 6, 6, 6);
    rowLayout->setSpacing(8);

    auto* avatar = new Avatar(row);
    avatar->setFixedSize(28, 28);
    avatar->setFontPx(12);
    avatar->setText(name);
    avatar->setColor(Avatar::colorForId(userId));

    auto* nameLabel = new QLabel(name, row);
    nameLabel->setStyleSheet(QStringLiteral("font-size: 10pt;"));
    if (model_->isSelf(userId)) {
        nameLabel->setText(name + QStringLiteral("（我）"));
    }

    rowLayout->addWidget(avatar);
    rowLayout->addWidget(nameLabel, 1);
    return row;
}

void ChatPage::showMemberDialog() {
    if (model_ == nullptr) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("成员（%1）").arg(model_->memberCount()));
    // QDialog 是顶层窗口，全局 QSS 里的 QDialog 规则管不到它，必须实例上设，
    // 否则在无窗口管理器的环境下会渲染成一块纯黑。
    dialog.setStyleSheet(QStringLiteral("QDialog { background: #FFFFFF; }"));
    dialog.setMinimumWidth(qBound(260, width() - 48, 360));

    auto* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(10);

    auto* scroll = new QScrollArea(&dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* canvas = new QWidget();
    auto* listLayout = new QVBoxLayout(canvas);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(2);

    const QMap<qint64, QString>& members = model_->members();
    for (auto it = members.constBegin(); it != members.constEnd(); ++it) {
        listLayout->addWidget(createMemberRow(it.key(), it.value()));
    }
    listLayout->addStretch(1);

    scroll->setWidget(canvas);
    layout->addWidget(scroll, 1);

    auto* closeBtn = new QPushButton(QStringLiteral("关闭"), &dialog);
    closeBtn->setObjectName("PrimaryButton");
    closeBtn->setMinimumHeight(core::ui::kTouchTarget);
    closeBtn->setCursor(Qt::PointingHandCursor);
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeBtn);

    dialog.exec();
}

void ChatPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateComposerPlaceholder();

    // 窄屏不再需要侧栏，宽屏恢复时也保持一致
    if (memberPanel_ != nullptr && core::ui::isCompact(width())) {
        memberPanel_->setVisible(false);
    }
}

void ChatPage::updateComposerPlaceholder() {
    if (composer_ == nullptr) return;

    if (!composer_->isEnabled()) {
        composer_->setPlaceholderText(QStringLiteral("连接已断开，正在重连…"));
        return;
    }

    // 窄屏放不下长提示，会折行并被裁切——换成短文案
    composer_->setPlaceholderText(
        core::ui::isCompact(width()) ? QStringLiteral("输入消息…")
                                     : QString::fromUtf8(kComposerHint));
}
