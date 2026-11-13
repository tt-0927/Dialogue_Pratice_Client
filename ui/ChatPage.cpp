/*
 * 文件名:    ChatPage.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  聊天界面的实现：控件构建、信号接线与渲染。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
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

/* 把时间戳格式化成 `HH:mm`。 */
/*
 * 功能描述:  执行 formatTime 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  ts  参数。
 * return:    返回值，含义见功能描述。
 */
QString formatTime(qint64 ts) {
    return QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("HH:mm"));
}

/* 把日期格式化成「今天 / 昨天 / M月d日」。 */
/*
 * 功能描述:  执行 formatDate 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  date  参数。
 * return:    返回值，含义见功能描述。
 */
QString formatDate(const QDate& date) {
    const QDate today = QDate::currentDate();
    if (date == today) return QStringLiteral("今天");
    if (date == today.addDays(-1)) return QStringLiteral("昨天");
    if (date.year() == today.year()) return date.toString(QStringLiteral("M月d日"));
    return date.toString(QStringLiteral("yyyy年M月d日"));
}

/* 模型路径优先级：QSettings 覆盖 > 编译期默认（源码树 models/ 目录）。 */
/*
 * 功能描述:  执行 resolveModelPath 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
QString resolveModelPath() {
    const QString configured = QSettings().value(QStringLiteral("voice/modelPath")).toString();
    if (!configured.isEmpty()) return configured;
#ifdef DP_DEFAULT_MODEL_PATH
    return QString::fromUtf8(DP_DEFAULT_MODEL_PATH);
#else
    return QString();
#endif
}

/* namespace */
}

/* ---------------------------------------------------------------- ComposerEdit */

/*
 * 功能描述:  执行 ComposerEdit 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
ComposerEdit::ComposerEdit(QWidget* parent) : QPlainTextEdit(parent) {
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

/*
 * 功能描述:  执行 keyPressEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void ComposerEdit::keyPressEvent(QKeyEvent* event) {
    const bool isEnter = (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter);
    if (isEnter && !(event->modifiers() & Qt::ShiftModifier)) {
        emit submit();
        event->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

/* ---------------------------------------------------------------- 构造 */

/*
 * 功能描述:  执行 ChatPage 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
ChatPage::ChatPage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    model_ = new business::ChatModel(this);
    m_pController = new business::ChatController(model_, this);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* ---- 左侧：聊天列 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* chatColumn = new QWidget(this);
    auto* columnLayout = new QVBoxLayout(chatColumn);
    columnLayout->setContentsMargins(0, 0, 0, 0);
    columnLayout->setSpacing(0);

    buildHeader(columnLayout);
    buildMessageArea(columnLayout);
    buildComposer(columnLayout);

    root->addWidget(chatColumn, 1);

    buildMemberPanel();
    root->addWidget(m_pMemberPanel);

    wireSignals();

    /* 模型在后台线程加载，不阻塞界面 */
    m_pVoice->start();
}

ChatPage::~ChatPage() = default;

/*
 * 功能描述:  执行 buildHeader 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  columnLayout  参数。
 * return:    void
 */
void ChatPage::buildHeader(QVBoxLayout* columnLayout) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
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

    m_pRoomAvatar = new CAvatar(header);
    m_pRoomAvatar->setFixedSize(40, 40);
    m_pRoomAvatar->setFontPx(18);
    m_pRoomAvatar->setText(QStringLiteral("#"));
    m_pRoomAvatar->setColor(theme::accent);
    headerLayout->addWidget(m_pRoomAvatar);

    auto* titleColumn = new QVBoxLayout();
    titleColumn->setSpacing(1);
    titleColumn->setContentsMargins(0, 0, 0, 0);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomTitle = new QLabel(header);
    m_pRoomTitle->setStyleSheet(QStringLiteral("font-size: 11.5pt; font-weight: 700;"));
    titleColumn->addWidget(m_pRoomTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomSubtitle = new QLabel(header);
    m_pRoomSubtitle->setObjectName("HintLabel");
    titleColumn->addWidget(m_pRoomSubtitle);

    headerLayout->addLayout(titleColumn, 1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberBtn = new QPushButton(header);
    m_pMemberBtn->setObjectName("LinkButton");
    m_pMemberBtn->setCursor(Qt::PointingHandCursor);
    headerLayout->addWidget(m_pMemberBtn);

    columnLayout->addWidget(header);

    connect(backBtn, &QPushButton::clicked, this, &ChatPage::backRequested);
    connect(m_pMemberBtn, &QPushButton::clicked, this, [this] {
        /* 窄屏没有空间放侧栏，改成弹窗；宽屏保持原来的侧栏切换 */
        if (core::ui::isCompact(width())) {
            showMemberDialog();
            return;
        }
        m_pMemberPanel->setVisible(!m_pMemberPanel->isVisible());
    });
}

/*
 * 功能描述:  执行 buildMessageArea 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  columnLayout  参数。
 * return:    void
 */
void ChatPage::buildMessageArea(QVBoxLayout* columnLayout) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pScroll = new QScrollArea(this);
    m_pScroll->setObjectName("ChatScroll");
    m_pScroll->setWidgetResizable(true);
    m_pScroll->setFrameShape(QFrame::NoFrame);
    m_pScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pCanvas = new QWidget();
    m_pCanvas->setObjectName("ChatCanvas");
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pChatLayout = new QVBoxLayout(m_pCanvas);
    m_pChatLayout->setContentsMargins(20, 16, 20, 16);
    m_pChatLayout->setSpacing(10);
    /* 索引 0 固定为弹簧：消息贴底 */
    m_pChatLayout->addStretch(1);


    m_pScroll->setWidget(m_pCanvas);
    columnLayout->addWidget(m_pScroll, 1);

    QScrollBar* bar = m_pScroll->verticalScrollBar();

    /* 内容或视口高度变化时决定要不要跟着贴底。 */
    
    /* 关键场景：建议条/译文卡弹出会把消息区变矮，maximum 随之增大。 */
    /* 此时若用户本来就在底部，必须继续贴底，否则位置不变就会「停在中间」—— */
    /* 表现为新到的消息明明渲染了却看不见，必须手动滚一下。 */
    connect(bar, &QScrollBar::rangeChanged, this, [this](int, int max) {
        if (m_nPendingScroll || m_nStickToBottom) {
            m_pScroll->verticalScrollBar()->setValue(max);
        }
    });

    /* 用户手动滚动时记录是否停在底部（留 4px 容差，避免浮点/整型误差） */
    connect(bar, &QScrollBar::valueChanged, this, [this](int value) {
        m_nStickToBottom = (value >= m_pScroll->verticalScrollBar()->maximum() - 4);
    });
}

/*
 * 功能描述:  执行 buildComposer 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  columnLayout  参数。
 * return:    void
 */
void ChatPage::buildComposer(QVBoxLayout* columnLayout) {
    m_pSuggestionBar = new CSuggestionBar(this);
    columnLayout->addWidget(m_pSuggestionBar);

    m_pTranslationCard = new CTranslationCard(this);
    columnLayout->addWidget(m_pTranslationCard);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pVoiceStatus = new QLabel(this);
    m_pVoiceStatus->setObjectName("VoiceStatus");
    m_pVoiceStatus->setVisible(false);
    columnLayout->addWidget(m_pVoiceStatus);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* composerFrame = new QFrame(this);
    composerFrame->setObjectName("Composer");
    auto* composerLayout = new QHBoxLayout(composerFrame);
    composerLayout->setContentsMargins(16, 10, 16, 10);
    composerLayout->setSpacing(10);

    m_pMicBtn = new CMicButton(composerFrame);
    composerLayout->addWidget(m_pMicBtn, 0, Qt::AlignVCenter);

    m_pComposer = new ComposerEdit(composerFrame);
    m_pComposer->setObjectName("ComposerEdit");
    updateComposerPlaceholder();
    m_pComposer->setFixedHeight(46);
    composerLayout->addWidget(m_pComposer, 1);

    m_pSendBtn = new CSendButton(composerFrame);
    composerLayout->addWidget(m_pSendBtn, 0, Qt::AlignVCenter);

    columnLayout->addWidget(composerFrame);

    connect(m_pSendBtn, &CSendButton::clicked, this, &ChatPage::sendCurrentText);
    connect(m_pComposer, &ComposerEdit::submit, this, &ChatPage::sendCurrentText);
}

/*
 * 功能描述:  执行 buildMemberPanel 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::buildMemberPanel() {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberPanel = new QFrame(this);
    m_pMemberPanel->setObjectName("MemberPanel");
    m_pMemberPanel->setFixedWidth(220);
    m_pMemberPanel->setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* panelLayout = new QVBoxLayout(m_pMemberPanel);
    panelLayout->setContentsMargins(12, 14, 12, 14);
    panelLayout->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberTitle = new QLabel(m_pMemberPanel);
    m_pMemberTitle->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: 700;"));
    panelLayout->addWidget(m_pMemberTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* memberScroll = new QScrollArea(m_pMemberPanel);
    memberScroll->setWidgetResizable(true);
    memberScroll->setFrameShape(QFrame::NoFrame);
    memberScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    memberScroll->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* memberCanvas = new QWidget();
    memberCanvas->setStyleSheet(QStringLiteral("background: transparent;"));
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberListLayout = new QVBoxLayout(memberCanvas);
    m_pMemberListLayout->setContentsMargins(0, 0, 0, 0);
    m_pMemberListLayout->setSpacing(4);
    m_pMemberListLayout->addStretch(1);
    memberScroll->setWidget(memberCanvas);

    panelLayout->addWidget(memberScroll, 1);
}

/* ---------------------------------------------------------------- 接线 */

/*
 * 功能描述:  执行 wireSignals 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::wireSignals() {
    /* ---- 模型 -> 视图 ---- */
    connect(model_, &business::ChatModel::itemAppended, this, [this](const business::ChatItem_S& item) {
        renderItem(item);
        scrollToBottom();
    });
    connect(model_, &business::ChatModel::modelReset, this, &ChatPage::rebuildView);
    connect(model_, &business::ChatModel::itemUpdated, this, &ChatPage::refreshBubble);
    connect(model_, &business::ChatModel::membersChanged, this, &ChatPage::rebuildMembers);

    /* ---- 编排器 -> 视图 ---- */
    connect(m_pController, &business::ChatController::notice, this, &ChatPage::notice);
    connect(m_pController, &business::ChatController::voiceStatus, this, &ChatPage::showVoiceStatus);
    connect(m_pController, &business::ChatController::suggestionsReady, this,
            [this](const QVector<Suggestion_S>& items) {
                m_pTranslationCard->hideCard();
                m_pSuggestionBar->showSuggestions(items);
            });
    connect(m_pController, &business::ChatController::suggestionsCleared, this,
            [this] { m_pSuggestionBar->hideBar(); });
    connect(m_pController, &business::ChatController::inputTranslationReady, this,
            [this](const QString& source, const QString& english) {
                /* 用户已经改了输入内容就丢弃这次结果 */
                if (source != m_pComposer->toPlainText().trimmed()) return;
                m_pTranslationCard->showTranslation(english);
            });
    connect(m_pController, &business::ChatController::inputTranslationFailed, this,
            [this](const QString& reason) {
                m_pTranslationCard->hideCard();
                showVoiceStatus(reason, true, 3000);
            });
    connect(m_pController, &business::ChatController::sendVoiceRequested, this,
            &ChatPage::sendVoiceRequested);

    /* ---- 语音输入 ---- */
    m_pVoice = new CVoiceRecorder(this);
    m_pVoice->setModelPath(resolveModelPath());
    m_pController->setRecorder(m_pVoice);

    connect(m_pMicBtn, &QAbstractButton::clicked, this, &ChatPage::toggleRecording);
    connect(m_pVoice, &CVoiceRecorder::modelLoaded, this, [this](bool ok, const QString& message) {
        m_nVoiceAvailable = ok;
        if (!ok) {
            m_pMicBtn->setEnabled(false);
            m_pMicBtn->setToolTip(message);
            showVoiceStatus(message, true, 6000);
        }
    });
    connect(m_pVoice, &CVoiceRecorder::pcmReady, this, &ChatPage::onVoiceRecorded);
    connect(m_pVoice, &CVoiceRecorder::recordingStarted, this, [this] {
        m_pMicBtn->setRecording(true);
        showVoiceStatus(QStringLiteral("● 正在录音 0:00 · 再点一次结束"), false, 0);
    });
    connect(m_pVoice, &CVoiceRecorder::recordingTick, this, [this](int seconds, int /*level*/) {
        showVoiceStatus(QStringLiteral("● 正在录音 0:%1 · 再点一次结束")
                            .arg(seconds, 2, 10, QLatin1Char('0')),
                        false, 0);
    });
    connect(m_pVoice, &CVoiceRecorder::recognizing, this, [this] {
        m_pMicBtn->setRecording(false);
        showVoiceStatus(QStringLiteral("识别中…"), false, 0);
    });
    connect(m_pVoice, &CVoiceRecorder::failed, this, [this](const QString& message) {
        m_pMicBtn->setRecording(false);
        showVoiceStatus(message, true, 3500);
    });

    /* ---- 语音消息播放 ---- */
    connect(CVoicePlayer::instance(), &CVoicePlayer::playingChanged, this, [this](bool playing) {
        const QByteArray current = CVoicePlayer::instance()->current();
        for (auto it = m_uBubbles.constBegin(); it != m_uBubbles.constEnd(); ++it) {
            const business::ChatItem_S* item = model_->find(it.key());
            if (item == nullptr || item->kind != MessageKind_EN::Voice) continue;
            it.value()->setPlaying(playing && !current.isEmpty() && item->audio == current);
        }
    });
    connect(CVoicePlayer::instance(), &CVoicePlayer::failed, this, [this](const QString& reason) {
        showVoiceStatus(reason, true, 3000);
    });

    /* ---- 语音合成 ---- */
    m_pTts = new CTtsService(this);
    m_pTts->setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
    connect(m_pTts, &CTtsService::speakingChanged, this, [this](bool speaking) {
        m_pTranslationCard->setPlaying(speaking);
        /* 气泡里的播放按钮也要跟着变成暂停图标，否则用户看不出在播 */
        if (!speaking && m_stPlayingBubble != nullptr) {
            m_stPlayingBubble->setPlaying(false);
            m_stPlayingBubble = nullptr;
        } else if (speaking && m_stPlayingBubble != nullptr) {
            m_stPlayingBubble->setPlaying(true);
        }
    });
    connect(m_pTts, &CTtsService::failed, this, [this](const QString& reason) {
        showVoiceStatus(reason, true, 3000);
    });

    /* ---- 译文卡 / 建议条 ---- */
    connect(m_pTranslationCard, &CTranslationCard::playRequested, this,
            [this](const QString& text) { m_pTts->speak(text); });
    connect(m_pTranslationCard, &CTranslationCard::useRequested, this, [this](const QString& text) {
        m_pComposer->setPlainText(text);
        m_pComposer->moveCursor(QTextCursor::End);
        m_pComposer->setFocus();
    });
    connect(m_pSuggestionBar, &CSuggestionBar::useRequested, this, [this](const QString& text) {
        m_pComposer->setPlainText(text);
        m_pComposer->moveCursor(QTextCursor::End);
        m_pComposer->setFocus();
        m_pSuggestionBar->hideBar();
    });
    connect(m_pSuggestionBar, &CSuggestionBar::playRequested, this,
            [this](const QString& text) { m_pTts->speak(text); });

    /* ---- 输入框 ---- */
    connect(m_pComposer, &QPlainTextEdit::textChanged, this, [this] {
        syncComposerState();

        const QString text = m_pComposer->toPlainText().trimmed();
        if (text.isEmpty()) {
            m_pTranslationCard->hideCard();
            return;
        }
        /* 开始打字就把建议收起来，避免抢位置 */
        m_pSuggestionBar->hideBar();

        if (CTranslationService::looksChinese(text)) {
            m_pTranslationCard->showPending();
            m_pController->requestInputTranslation(text);
        } else {
            m_pTranslationCard->hideCard();
        }
    });
}

/* ---------------------------------------------------------------- 生命周期 */

void ChatPage::enterRoom(const QString& room, bool created, const QVector<MemberInfo_S>& members,
                         qint64 selfId, const QString& selfName) {
    model_->enterRoom(room, selfId, selfName, members);

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(room));
    updateHeader();

    appendSystemText(created ? QStringLiteral("你创建了房间 %1").arg(room)
                             : QStringLiteral("你加入了房间 %1").arg(room),
                     QDateTime::currentMSecsSinceEpoch());

    m_pComposer->clear();
    m_pTranslationCard->hideCard();
    m_pSuggestionBar->hideBar();
    m_pComposer->setFocus();
    setOnline(true);
    syncComposerState();
}

void ChatPage::resumeRoom(const QString& room, const QVector<MemberInfo_S>& members,
                          qint64 selfId, const QString& selfName) {
    model_->resumeRoom(room, selfId, selfName, members);

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(room));
    appendSystemText(QStringLiteral("已重新连接"), QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    setOnline(true);
    m_pComposer->setFocus();
}

/*
 * 功能描述:  执行 clearRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::clearRoom() {
    model_->clear();

    /* 语音相关的运行态一并复位，避免播到一半切房间 */
    CVoicePlayer::instance()->stop();
    if (m_pTts != nullptr) m_pTts->stop();

    clearView();
    m_pComposer->clear();
    m_pTranslationCard->hideCard();
    m_pSuggestionBar->hideBar();
    m_pMemberPanel->setVisible(false);
    setOnline(true);
    syncComposerState();
}

/*
 * 功能描述:  执行 applyRemoteConfig 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  json  参数。
 * return:    void
 */
void ChatPage::applyRemoteConfig(const QByteArray& json) {
    if (m_pController != nullptr) m_pController->applyRemoteConfig(json);
}

/*
 * 功能描述:  执行 room 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
QString ChatPage::room() const {
    return model_ != nullptr ? model_->room() : QString();
}

/* ---------------------------------------------------------------- 消息投递 */

void ChatPage::appendChat(qint64 seq, qint64 from, const QString& name, const QString& text,
                          qint64 ts) {
    model_->appendChat(seq, from, name, text, ts);
}

void ChatPage::appendVoice(qint64 seq, qint64 from, const QString& name,
                           const QByteArray& audio, int durationMs, qint64 ts) {
    model_->appendVoice(seq, from, name, audio, durationMs, ts);
}

/*
 * 功能描述:  执行 mergeHistory 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  messages  参数。
 * return:    void
 */
void ChatPage::mergeHistory(const QVector<ChatMessage_S>& messages) {
    model_->mergeHistory(messages);
}

/*
 * 功能描述:  执行 appendSystemText 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * param [in]  ts  参数。
 * return:    void
 */
void ChatPage::appendSystemText(const QString& text, qint64 ts) {
    model_->appendSystem(text, ts);
}

void ChatPage::onSystemEvent(const QString& event, qint64 userId, const QString& name,
                             const QString& /*oldName*/, const QString& text, qint64 ts) {
    model_->applySystemEvent(event, userId, name, text, ts);
}

/* ---------------------------------------------------------------- 渲染 */

/*
 * 功能描述:  执行 renderItem 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  item  参数。
 * return:    void
 */
void ChatPage::renderItem(const business::ChatItem_S& item) {
    appendDateSeparatorIfNeeded(item.ts);

    if (item.system) {
        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        auto* row = new QWidget(m_pCanvas);
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

        m_pChatLayout->addWidget(row);
        pinHeight(row);
        return;
    }

    const bool outgoing = model_->isSelf(item.from);
    /* 气泡宽度跟着消息区走：窄屏按比例收缩，宽屏保持 520 上限 */
    const int bubbleWidth = core::ui::bubbleMaxWidth(m_pCanvas->width());

    /* ---- 语音消息 ---- */
    if (item.kind == MessageKind_EN::Voice) {
        auto* bubble = new CMessageBubble(item.name, QString(), formatTime(item.ts),
                                         outgoing, item.from, false, bubbleWidth, m_pCanvas);
        bubble->setVoice(item.audio, item.durationMs);
        connect(bubble, &CMessageBubble::voicePlayRequested, this,
                [](const QByteArray& audio) { CVoicePlayer::instance()->play(audio); });

        if (item.seq > 0) m_uBubbles.insert(item.seq, bubble);
        m_pChatLayout->addWidget(bubble);
        pinHeight(bubble);
        refreshBubble(item.seq);
        return;
    }

    /* ---- 文本消息 ---- */
    const bool playable = !outgoing && CTranslationService::looksEnglish(item.text);
    auto* bubble = new CMessageBubble(item.name, item.text, formatTime(item.ts),
                                     outgoing, item.from, playable, bubbleWidth, m_pCanvas);
    connect(bubble, &CMessageBubble::playRequested, this, [this, bubble](const QString& text) {
        if (m_stPlayingBubble != nullptr && m_stPlayingBubble != bubble) {
            m_stPlayingBubble->setPlaying(false);
        }
        m_stPlayingBubble = bubble;
        m_pTts->speak(text);
    });

    if (item.seq > 0) m_uBubbles.insert(item.seq, bubble);
    m_pChatLayout->addWidget(bubble);
    pinHeight(bubble);
    refreshBubble(item.seq);
}

/*
 * 功能描述:  执行 refreshBubble 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * return:    void
 */
void ChatPage::refreshBubble(qint64 seq) {
    if (seq <= 0) return;

    CMessageBubble* bubble = m_uBubbles.value(seq, nullptr);
    const business::ChatItem_S* item = model_->find(seq);
    if (bubble == nullptr || item == nullptr) return;

    if (item->kind == MessageKind_EN::Voice) {
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

/*
 * 功能描述:  执行 rebuildView 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::rebuildView() {
    clearView();
    m_stLastDate = QDate();
    for (const business::ChatItem_S& item : model_->items()) {
        renderItem(item);
    }
    scrollToBottom();
}

/*
 * 功能描述:  执行 clearView 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::clearView() {
    m_uBubbles.clear();
    m_stPlayingBubble = nullptr;
    m_nStickToBottom = true;
    /* 索引 0 是弹簧，必须保留 */
    while (m_pChatLayout->count() > 1) {
        QLayoutItem* item = m_pChatLayout->takeAt(m_pChatLayout->count() - 1);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }
}

/*
 * 功能描述:  执行 appendDateSeparatorIfNeeded 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  ts  参数。
 * return:    void
 */
void ChatPage::appendDateSeparatorIfNeeded(qint64 ts) {
    const QDate date = QDateTime::fromMSecsSinceEpoch(ts).date();
    if (date == m_stLastDate) return;
    m_stLastDate = date;

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* row = new QWidget(m_pCanvas);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 6, 0, 2);

    auto* pill = new QLabel(formatDate(date), row);
    pill->setObjectName("DatePill");
    pill->setAlignment(Qt::AlignCenter);

    rowLayout->addStretch(1);
    rowLayout->addWidget(pill, 0);
    rowLayout->addStretch(1);

    m_pChatLayout->addWidget(row);
    pinHeight(row);
}

/*
 * 功能描述:  执行 scrollToBottom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::scrollToBottom() {
    m_nPendingScroll = true;
    m_nStickToBottom = true;

    /* QScrollArea 在 widgetResizable 下按 sizeHint 决定内容高度，而带 stretch 的 */
    /* QVBoxLayout 在布局未结算时 sizeHint 偏小（自绘控件更明显），结果滚动条 */
    /* maximum 恒为 0——消息加进去了却滚不到。这里直接累加每个子控件的实际高度， */
    /* 不依赖 sizeHint，绕开这个坑。 */
    if (m_pCanvas != nullptr && m_pChatLayout != nullptr) {
        m_pChatLayout->activate();

        const QMargins margins = m_pChatLayout->contentsMargins();
        int contentHeight = margins.top() + margins.bottom();
        int visibleCount = 0;

        for (int i = 0; i < m_pChatLayout->count(); ++i) {
            QLayoutItem* entry = m_pChatLayout->itemAt(i);
            if (entry == nullptr) continue;

            if (QWidget* w = entry->widget()) {
                const int h = (w->height() > 0) ? w->height() : w->sizeHint().height();
                contentHeight += h;
                ++visibleCount;
            }
        }
        if (visibleCount > 1) {
            contentHeight += m_pChatLayout->spacing() * (visibleCount - 1);
        }

        if (contentHeight > 0 && m_pCanvas->minimumHeight() != contentHeight) {
            m_pCanvas->setMinimumHeight(contentHeight);
        }
    }

    /* 布局是异步结算的：addWidget 之后要等一轮事件循环，viewport 高度才更新， */
    /* 语音条这类自绘控件更晚。所以分多个时间点各贴一次底， */
    /* 而不是只依赖单次 setValue（那样会停在中间）。 */
    for (int delayMs : {0, 60, 200}) {
        QTimer::singleShot(delayMs, this, [this] {
            if (!m_nPendingScroll) return;

            /* 优先让最后一个控件可见：QScrollArea 会自己完成必要的布局结算。 */
            /* 注意不要再叠加 setValue(maximum())——此时 maximum() 可能仍是旧值， */
            /* 会把刚滚下去的位置又拉回来，表现就是「停在中间看不到新消息」。 */
            if (m_pChatLayout != nullptr && m_pChatLayout->count() > 1) {
                QLayoutItem* last = m_pChatLayout->itemAt(m_pChatLayout->count() - 1);
                if (last != nullptr && last->widget() != nullptr) {
                    m_pScroll->ensureWidgetVisible(last->widget(), 0, 24);
                }
            }
            /* 兜底：按内容坐标滚到最底，覆盖 ensureWidgetVisible 判定「已可见」的情况 */
            if (m_pCanvas != nullptr) {
                m_pScroll->ensureVisible(0, qMax(0, m_pCanvas->height() - 1), 0, 0);
            }
            forceScrollToBottom();
        });
    }

    if (m_pScrollSettleTimer == nullptr) {
        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        m_pScrollSettleTimer = new QTimer(this);
        m_pScrollSettleTimer->setSingleShot(true);
        connect(m_pScrollSettleTimer, &QTimer::timeout, this, [this] {
            forceScrollToBottom();
            /* 结算完成，之后不再强制贴底 */
            m_nPendingScroll = false;
        });
    }
    m_pScrollSettleTimer->start(450);
}

/*
 * 功能描述:  执行 pinHeight 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  widget  参数。
 * return:    void
 */
void ChatPage::pinHeight(QWidget* widget) {
    if (widget == nullptr) return;
    /* 必须在 addWidget 之后调用：没有 parent 时 sizeHint 还没按可用宽度换行计算， */
    /* 拿到的高度会偏小，钉死之后内容就被裁掉。 */
    if (QLayout* layout = widget->layout()) layout->activate();
    const int height = widget->sizeHint().height();
    if (height > 0) {
        widget->setMinimumHeight(height);
        widget->updateGeometry();
    }
}

/*
 * 功能描述:  执行 forceScrollToBottom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::forceScrollToBottom() {
    /* 必须先把布局激活：刚 addWidget 时 maximum() 还是旧值， */
    /* 直接 setValue 会停在中间（语音条这类高度延迟确定的控件尤其明显）。 */
    if (m_pChatLayout != nullptr) m_pChatLayout->activate();

    QScrollBar* bar = m_pScroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

/* ---------------------------------------------------------------- 交互 */

/*
 * 功能描述:  执行 sendCurrentText 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::sendCurrentText() {
    if (!m_pComposer->isEnabled()) return;

    const QString text = m_pComposer->toPlainText().trimmed();
    if (text.isEmpty()) return;

    emit sendRequested(text);
    m_pComposer->clear();
    m_pComposer->setFocus();
}

/*
 * 功能描述:  执行 toggleRecording 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::toggleRecording() {
    if (m_pVoice == nullptr) return;

    if (m_pVoice->isRecording()) {
        m_pVoice->stopRecording();
        return;
    }

    /* 语音消息：录完直接把原始音频发出去，不再走「识别成文字填进输入框」 */
    m_pVoice->startRecording(CVoiceRecorder::Purpose_EN::Send);
}

/*
 * 功能描述:  执行 onVoiceRecorded 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  pcm  参数。
 * param [in]  durationMs  参数。
 * return:    void
 */
void ChatPage::onVoiceRecorded(const QByteArray& pcm, int durationMs) {
    if (pcm.isEmpty()) return;
    m_pMicBtn->setRecording(false);
    m_pController->sendVoice(pcm, durationMs);
}

/*
 * 功能描述:  执行 setOnline 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  online  参数。
 * return:    void
 */
void ChatPage::setOnline(bool online) {
    m_pComposer->setEnabled(online);
    m_pSendBtn->setEnabled(online);
    m_pMicBtn->setEnabled(online && m_nVoiceAvailable);
    updateComposerPlaceholder();
}

/*
 * 功能描述:  执行 syncComposerState 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::syncComposerState() {
    if (m_pController == nullptr) return;
    m_pController->setComposerEmpty(m_pComposer->toPlainText().trimmed().isEmpty());
}

/* ---------------------------------------------------------------- 状态提示 */

/*
 * 功能描述:  执行 showVoiceStatus 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * param [in]  error  参数。
 * param [in]  autoHideMs  参数。
 * return:    void
 */
void ChatPage::showVoiceStatus(const QString& text, bool error, int autoHideMs) {
    if (m_pVoiceStatus == nullptr) return;

    ++m_nVoiceStatusToken;
    const int token = m_nVoiceStatusToken;

    /* 传空串表示清掉状态条 */
    if (text.isEmpty()) {
        m_pVoiceStatus->clear();
        m_pVoiceStatus->setVisible(false);
        return;
    }

    m_pVoiceStatus->setText(text);
    m_pVoiceStatus->setStyleSheet(
        error
            ? QStringLiteral("color:#E5484D; background:#FDF2F2; padding:6px 18px; font-size:9pt;")
            : QStringLiteral("color:#5B6675; background:#F2F5F9; padding:6px 18px; font-size:9pt;"));
    m_pVoiceStatus->setVisible(true);

    if (autoHideMs > 0) {
        QTimer::singleShot(autoHideMs, this, [this, token] {
            if (token == m_nVoiceStatusToken) m_pVoiceStatus->setVisible(false);
        });
    }
}

/* ---------------------------------------------------------------- 成员 */

/*
 * 功能描述:  执行 updateHeader 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::updateHeader() {
    const int count = (model_ != nullptr) ? model_->memberCount() : 0;
    m_pRoomSubtitle->setText(QStringLiteral("%1 位成员").arg(count));
    m_pMemberBtn->setText(QStringLiteral("成员 %1").arg(count));
    if (m_pMemberTitle != nullptr) {
        m_pMemberTitle->setText(QStringLiteral("成员（%1）").arg(count));
    }
}

/*
 * 功能描述:  执行 rebuildMembers 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::rebuildMembers() {
    if (model_ == nullptr || m_pMemberListLayout == nullptr) return;

    while (m_pMemberListLayout->count() > 1) {
        QLayoutItem* item = m_pMemberListLayout->takeAt(0);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }

    const QMap<qint64, QString>& members = model_->members();
    for (auto it = members.constBegin(); it != members.constEnd(); ++it) {
        m_pMemberListLayout->insertWidget(m_pMemberListLayout->count() - 1,
                                        createMemberRow(it.key(), it.value()));
    }

    updateHeader();
}

/*
 * 功能描述:  执行 createMemberRow 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  userId  参数。
 * param [in]  name  参数。
 * return:    返回值，含义见功能描述。
 */
QWidget* ChatPage::createMemberRow(qint64 userId, const QString& name) {
    auto* row = new QWidget();
    row->setObjectName("MemberRow");

    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(6, 6, 6, 6);
    rowLayout->setSpacing(8);

    auto* avatar = new CAvatar(row);
    avatar->setFixedSize(28, 28);
    avatar->setFontPx(12);
    avatar->setText(name);
    avatar->setColor(CAvatar::colorForId(userId));

    auto* nameLabel = new QLabel(name, row);
    nameLabel->setStyleSheet(QStringLiteral("font-size: 10pt;"));
    if (model_->isSelf(userId)) {
        nameLabel->setText(name + QStringLiteral("（我）"));
    }

    rowLayout->addWidget(avatar);
    rowLayout->addWidget(nameLabel, 1);
    return row;
}

/*
 * 功能描述:  执行 showMemberDialog 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::showMemberDialog() {
    if (model_ == nullptr) return;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("成员（%1）").arg(model_->memberCount()));
    /* QDialog 是顶层窗口，全局 QSS 里的 QDialog 规则管不到它，必须实例上设， */
    /* 否则在无窗口管理器的环境下会渲染成一块纯黑。 */
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

/*
 * 功能描述:  执行 resizeEvent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  event  参数。
 * return:    void
 */
void ChatPage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateComposerPlaceholder();

    /* 窄屏不再需要侧栏，宽屏恢复时也保持一致 */
    if (m_pMemberPanel != nullptr && core::ui::isCompact(width())) {
        m_pMemberPanel->setVisible(false);
    }
}

/*
 * 功能描述:  执行 updateComposerPlaceholder 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::updateComposerPlaceholder() {
    if (m_pComposer == nullptr) return;

    if (!m_pComposer->isEnabled()) {
        m_pComposer->setPlaceholderText(QStringLiteral("连接已断开，正在重连…"));
        return;
    }

    /* 窄屏放不下长提示，会折行并被裁切——换成短文案 */
    m_pComposer->setPlaceholderText(
        core::ui::isCompact(width()) ? QStringLiteral("输入消息…")
                                     : QString::fromUtf8(kComposerHint));
}
