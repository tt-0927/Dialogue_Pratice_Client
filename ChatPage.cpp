/*
 * 文件名:    ChatPage.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  ChatPage 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* ChatPage.cpp */
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

/* ---------------------------------------------------------------- ChatPage */

namespace {

const char* kComposerHint = "输入消息，Enter 发送，Shift+Enter 换行";

/*
 * 功能描述:  执行 formatTime 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  ts  参数。
 * return:    返回值，含义见功能描述。
 */
QString formatTime(qint64 ts) {
    return QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("HH:mm"));
}

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

/* 模型路径优先级：QSettings 覆盖 > 编译期默认（源码树 models/ 目录） */
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

/*
 * 功能描述:  执行 ChatPage 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
ChatPage::ChatPage(QWidget* parent) : QWidget(parent) {
    setObjectName("AppRoot");

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* root = new QHBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    /* ---- 左侧：聊天列 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* chatColumn = new QWidget(this);
    auto* chatColumnLayout = new QVBoxLayout(chatColumn);
    chatColumnLayout->setContentsMargins(0, 0, 0, 0);
    chatColumnLayout->setSpacing(0);

    /* 顶栏 */
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

    chatColumnLayout->addWidget(header);

    /* 消息区 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pScroll = new QScrollArea(chatColumn);
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
    chatColumnLayout->addWidget(m_pScroll, 1);

    /* 输入区 */
    m_pSuggestionBar = new CSuggestionBar(chatColumn);
    chatColumnLayout->addWidget(m_pSuggestionBar);

    m_pTranslationCard = new CTranslationCard(chatColumn);
    chatColumnLayout->addWidget(m_pTranslationCard);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pVoiceStatus = new QLabel(chatColumn);
    m_pVoiceStatus->setObjectName("VoiceStatus");
    m_pVoiceStatus->setVisible(false);
    chatColumnLayout->addWidget(m_pVoiceStatus);

    auto* composerFrame = new QFrame(chatColumn);
    composerFrame->setObjectName("Composer");
    auto* composerLayout = new QHBoxLayout(composerFrame);
    composerLayout->setContentsMargins(16, 10, 16, 10);
    composerLayout->setSpacing(10);

    m_pMicBtn = new CMicButton(composerFrame);
    composerLayout->addWidget(m_pMicBtn, 0, Qt::AlignVCenter);

    m_pComposer = new ComposerEdit(composerFrame);
    m_pComposer->setObjectName("ComposerEdit");
    m_pComposer->setPlaceholderText(QString::fromUtf8(kComposerHint));
    m_pComposer->setFixedHeight(46);
    composerLayout->addWidget(m_pComposer, 1);

    m_pSendBtn = new CSendButton(composerFrame);
    composerLayout->addWidget(m_pSendBtn, 0, Qt::AlignVCenter);

    chatColumnLayout->addWidget(composerFrame);

    root->addWidget(chatColumn, 1);

    /* ---- 右侧：成员面板 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberPanel = new QFrame(this);
    m_pMemberPanel->setObjectName("MemberPanel");
    m_pMemberPanel->setFixedWidth(220);
    m_pMemberPanel->setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* memberPanelLayout = new QVBoxLayout(m_pMemberPanel);
    memberPanelLayout->setContentsMargins(12, 14, 12, 14);
    memberPanelLayout->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberTitle = new QLabel(m_pMemberPanel);
    m_pMemberTitle->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: 700;"));
    memberPanelLayout->addWidget(m_pMemberTitle);

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

    memberPanelLayout->addWidget(memberScroll, 1);
    root->addWidget(m_pMemberPanel);

    /* ---- 连接 ---- */
    connect(backBtn, &QPushButton::clicked, this, &ChatPage::backRequested);
    connect(m_pSendBtn, &CSendButton::clicked, this, &ChatPage::sendCurrentText);
    connect(m_pComposer, &ComposerEdit::submit, this, &ChatPage::sendCurrentText);
    connect(m_pMemberBtn, &QPushButton::clicked, this, [this] {
        m_pMemberPanel->setVisible(!m_pMemberPanel->isVisible());
    });

    /* 内容高度变化（气泡换行、历史批量插入）时，若仍在贴底状态就继续贴底 */
    connect(m_pScroll->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this](int, int) {
        if (m_nPendingScroll) forceScrollToBottom();
    });

    /* ---- 语音输入 ---- */
    m_pVoice = new CVoiceInput(this);
    m_pVoice->setModelPath(resolveModelPath());

    connect(m_pMicBtn, &QAbstractButton::clicked, this, &ChatPage::toggleRecording);

    connect(m_pVoice, &CVoiceInput::modelLoaded, this, [this](bool ok, const QString& message) {
        m_nVoiceAvailable = ok;
        if (!ok) {
            m_pMicBtn->setEnabled(false);
            m_pMicBtn->setToolTip(message);
            showVoiceStatus(message, true, 6000);
        }
    });
    connect(m_pVoice, &CVoiceInput::pcmReady, this, &ChatPage::onVoiceRecorded);

    /* 语音播放：状态同步到对应气泡的语音条 */
    connect(CVoicePlayer::instance(), &CVoicePlayer::playingChanged, this, [this](bool playing) {
        const QByteArray current = CVoicePlayer::instance()->current();
        for (const ChatItem_S& item : model_) {
            if (item.kind != MessageKind_EN::Voice || item.seq <= 0) continue;
            CMessageBubble* bubble = m_uBubbles.value(item.seq, nullptr);
            if (bubble == nullptr) continue;
            bubble->setPlaying(playing && !current.isEmpty() && item.audio == current);
        }
    });
    connect(CVoicePlayer::instance(), &CVoicePlayer::failed, this, [this](const QString& reason) {
        showVoiceStatus(reason, true, 3000);
    });

    connect(m_pVoice, &CVoiceInput::recordingStarted, this, [this] {
        m_pMicBtn->setRecording(true);
        showVoiceStatus(QStringLiteral("● 正在录音 0:00 · 再点一次结束"), false, 0);
    });
    connect(m_pVoice, &CVoiceInput::recordingTick, this, [this](int seconds, int /*level*/) {
        showVoiceStatus(QStringLiteral("● 正在录音 0:%1 · 再点一次结束")
                            .arg(seconds, 2, 10, QLatin1Char('0')),
                        false, 0);
    });
    connect(m_pVoice, &CVoiceInput::recognizing, this, [this] {
        m_pMicBtn->setRecording(false);
        showVoiceStatus(QStringLiteral("识别中…"), false, 0);
    });
    connect(m_pVoice, &CVoiceInput::textReady, this, [this](const QString& text, qint64 elapsedMs) {
        m_pMicBtn->setRecording(false);

        /* 语音消息的转写结果：回填到气泡，不再插进输入框 */
        if (m_uTranscribingSeq > 0) {
            const qint64 seq = m_uTranscribingSeq;
            m_uTranscribingSeq = 0;
            updateTranscript(seq, text, true);
            /* 清掉「识别中…」 */
            showVoiceStatus(QString(), false, 0);
            pumpTranscript();
            return;
        }

        /* 兜底路径（--voice-file 诊断等） */
        insertVoiceText(text);
        showVoiceStatus(QStringLiteral("已识别（%1 ms），可直接编辑后发送").arg(elapsedMs),
                        false, 2500);
    });
    connect(m_pVoice, &CVoiceInput::failed, this, [this](const QString& message) {
        m_pMicBtn->setRecording(false);

        if (m_uTranscribingSeq > 0) {
            const qint64 seq = m_uTranscribingSeq;
            m_uTranscribingSeq = 0;
            updateTranscript(seq, QString(), false);
            showVoiceStatus(QString(), false, 0);
            pumpTranscript();
        }

        showVoiceStatus(message, true, 3500);
    });

    /* 模型在后台线程加载，不阻塞界面 */
    m_pVoice->start();

    /* ---- 翻译（他人英文消息 -> 中文），配置来自 config/llm.json ---- */
    /* 两个独立客户端：翻译可以单独指定专用模型（如 Hunyuan-MT）， */
    /* 对话/智能建议用通用模型（如 Qwen），各发挥优势。 */
    m_pLlm = new CLlmClient(this);
    m_pTranslateLlm = new CLlmClient(this);
    m_pTranslation = new CTranslationService(this);
    m_pTranslation->setClient(m_pTranslateLlm);

    const QString configPath = AppConfig::resolvePath();
    /* 首次运行生成一份可编辑的模板 */
    AppConfig::ensureTemplate(configPath);
    applyConfig(AppConfig::load(), true);

    /* 配置文件热重载：改完保存就生效，不用重启 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pConfigWatcher = new QFileSystemWatcher(this);
    if (!configPath.isEmpty()) {
        m_pConfigWatcher->addPath(configPath);
        m_pConfigWatcher->addPath(QFileInfo(configPath).absolutePath());
    }
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pConfigReloadTimer = new QTimer(this);
    m_pConfigReloadTimer->setSingleShot(true);
    m_pConfigReloadTimer->setInterval(400);
    connect(m_pConfigReloadTimer, &QTimer::timeout, this, [this] {
        applyConfig(AppConfig::load(), false);
    });
    connect(m_pConfigWatcher, &QFileSystemWatcher::fileChanged, this,
            [this] { m_pConfigReloadTimer->start(); });
    connect(m_pConfigWatcher, &QFileSystemWatcher::directoryChanged, this,
            [this] { m_pConfigReloadTimer->start(); });

    connect(m_pTranslation, &CTranslationService::messageTranslated, this,
            [this](qint64 seq, const QString& chinese) {
                for (ChatItem_S& item : model_) {
                    if (item.seq == seq) {
                        item.translation = chinese;
                        item.translationState = 2;
                        break;
                    }
                }
                if (CMessageBubble* bubble = m_uBubbles.value(seq, nullptr)) {
                    bubble->setTranslation(chinese);
                }
            });
    connect(m_pTranslation, &CTranslationService::inputTranslated, this,
            [this](const QString& source, const QString& english) {
                /* 用户已经改了输入内容就丢弃这次结果 */
                if (source != m_pComposer->toPlainText().trimmed()) return;
                m_pTranslationCard->showTranslation(english);
            });
    /* ---- 语音合成（Piper）---- */
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

    /* ---- 中文输入 -> 英文译文卡 ---- */
    connect(m_pTranslationCard, &CTranslationCard::playRequested, this,
            [this](const QString& text) { m_pTts->speak(text); });
    connect(m_pTranslationCard, &CTranslationCard::useRequested, this, [this](const QString& text) {
        m_pComposer->setPlainText(text);
        m_pComposer->moveCursor(QTextCursor::End);
        m_pComposer->setFocus();
    });

    /* ---- 智能建议回复 ---- */
    m_pReply = new CReplyService(this);
    m_pReply->setClient(m_pLlm);
    connect(m_pReply, &CReplyService::suggestionsReady, this, [this](const QVector<Suggestion_S>& items) {
        /* 用户已经在打字了 */
        if (!m_pComposer->toPlainText().trimmed().isEmpty()) return;
        m_pTranslationCard->hideCard();
        m_pSuggestionBar->showSuggestions(items);
    });
    connect(m_pReply, &CReplyService::failed, this, [this](const QString&) {
        m_pSuggestionBar->hideBar();
    });
    connect(m_pSuggestionBar, &CSuggestionBar::useRequested, this, [this](const QString& text) {
        m_pComposer->setPlainText(text);
        m_pComposer->moveCursor(QTextCursor::End);
        m_pComposer->setFocus();
        m_pSuggestionBar->hideBar();
    });
    connect(m_pSuggestionBar, &CSuggestionBar::playRequested, this,
            [this](const QString& text) { m_pTts->speak(text); });

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pReplyDebounce = new QTimer(this);
    m_pReplyDebounce->setSingleShot(true);
    m_pReplyDebounce->setInterval(800);
    connect(m_pReplyDebounce, &QTimer::timeout, this, &ChatPage::requestSuggestions);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pInputDebounce = new QTimer(this);
    m_pInputDebounce->setSingleShot(true);
    m_pInputDebounce->setInterval(500);
    connect(m_pInputDebounce, &QTimer::timeout, this, &ChatPage::onInputDebounced);
    connect(m_pComposer, &QPlainTextEdit::textChanged, this, [this] {
        if (m_pComposer->toPlainText().trimmed().isEmpty()) {
            m_pTranslationCard->hideCard();
            return;
        }
        /* 开始打字就把建议收起来，避免抢位置 */
        m_pSuggestionBar->hideBar();
        m_pInputDebounce->start();
    });

    connect(m_pTranslation, &CTranslationService::failed, this,
            [this](qint64 seq, const QString& reason) {
                if (seq <= 0) {
                    /* 输入框的中文翻译失败：卡片不能一直停在「翻译中…」 */
                    m_pTranslationCard->hideCard();
                    showVoiceStatus(reason, true, 3000);
                    return;
                }
                for (ChatItem_S& item : model_) {
                    if (item.seq == seq) {
                        item.translationState = 3;
                        break;
                    }
                }
                if (CMessageBubble* bubble = m_uBubbles.value(seq, nullptr)) {
                    bubble->setTranslationFailed();
                }
            });
}

/* ---------------------------------------------------------------- 进出房间 */

void ChatPage::enterRoom(const QString& room, bool created, const QVector<MemberInfo_S>& members,
                         qint64 selfId, const QString& selfName) {
    m_strRoom = room;
    m_uSelfId = selfId;
    m_strSelfName = selfName;
    m_uSelfIds.insert(selfId);

    m_uMembers.clear();
    for (const MemberInfo_S& m : members) {
        m_uMembers.insert(m.id, m.name);
    }

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(room));

    model_.clear();
    m_uSeqs.clear();
    clearView();
    m_stLastDate = QDate();

    appendSystemText(created ? QStringLiteral("你创建了房间 %1").arg(room)
                             : QStringLiteral("你加入了房间 %1").arg(room),
                     QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    rebuildMembers();
    setOnline(true);

    m_pComposer->clear();
    m_pTranslationCard->hideCard();
    m_pSuggestionBar->hideBar();
    m_pComposer->setFocus();

    /* 配置有问题时，进房再提醒一次（启动时的那条容易被错过）。 */
    /* 延后 1.5s：先让「已加入房间」的提示显示完，避免互相覆盖。 */
    if (!m_strConfigError.isEmpty()) {
        QTimer::singleShot(1500, this, [this] { emit notice(m_strConfigError, true); });
    }
}

void ChatPage::resumeRoom(const QString& room, const QVector<MemberInfo_S>& members,
                          qint64 selfId, const QString& selfName) {
    m_strRoom = room;
    m_uSelfId = selfId;
    m_strSelfName = selfName;
    /* 重连后服务端会分配新 id，旧消息仍应显示为自己发的 */
    m_uSelfIds.insert(selfId);

    m_uMembers.clear();
    for (const MemberInfo_S& m : members) {
        m_uMembers.insert(m.id, m.name);
    }

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(room));
    appendSystemText(QStringLiteral("已重新连接"), QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    rebuildMembers();
    setOnline(true);
    m_pComposer->setFocus();
}

/*
 * 功能描述:  执行 clearRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::clearRoom() {
    m_strRoom.clear();
    m_uMembers.clear();
    model_.clear();
    m_uSeqs.clear();
    m_uSelfIds.clear();
    m_stLastDate = QDate();

    /* 语音相关的运行态一并复位，避免播到一半切房间 */
    CVoicePlayer::instance()->stop();
    m_uTranscriptQueue.clear();
    m_uTranscribingSeq = 0;
    clearView();
    m_pComposer->clear();
    m_pTranslationCard->hideCard();
    m_pSuggestionBar->hideBar();
    if (m_pTts != nullptr) m_pTts->stop();
    m_pMemberPanel->setVisible(false);
    setOnline(true);
}

/* ---------------------------------------------------------------- 消息 */

void ChatPage::appendChat(qint64 seq, qint64 from, const QString& name,
                          const QString& text, qint64 ts) {
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    if (seq > 0) {
        /* 历史补发与实时推送可能重叠 */
        if (m_uSeqs.contains(seq)) return;
        m_uSeqs.insert(seq);
    }

    if (from != 0 && !m_uMembers.contains(from)) {
        m_uMembers.insert(from, name);
        rebuildMembers();
    }

    ChatItem_S item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.text = text;
    item.system = false;

    const bool needTranslation = from != 0 && !m_uSelfIds.contains(from)
                                 && m_pTranslation != nullptr && m_pTranslation->isEnabled()
                                 && CTranslationService::looksEnglish(text);
    if (needTranslation) item.translationState = 1;

    appendItem(item);
    scrollToBottom();

    if (needTranslation) m_pTranslation->requestMessage(seq, text);

    /* 他人消息到达且输入框为空 -> 稍后生成 3 条建议 */
    const bool incoming = (from != 0) && !m_uSelfIds.contains(from);
    if (incoming && m_pReply != nullptr && m_pReply->isEnabled()
        && m_pComposer->toPlainText().trimmed().isEmpty()) {
        m_pReplyDebounce->start();
    }
}

/*
 * 功能描述:  执行 mergeHistory 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  messages  参数。
 * return:    void
 */
void ChatPage::mergeHistory(const QVector<ChatMessage_S>& messages) {
    if (messages.isEmpty()) return;

    bool changed = false;
    for (const ChatMessage_S& m : messages) {
        if (m.seq > 0 && m_uSeqs.contains(m.seq)) continue;
        if (m.seq > 0) m_uSeqs.insert(m.seq);

        /* 历史里「同名、且已不在房间」的 id 视为本人旧会话： */
        /* 服务端每次连接都会分配新 userId，重启客户端后旧消息不该显示成别人的。 */
        if (m.from != 0 && !m_uSelfIds.contains(m.from)
            && m.name == m_strSelfName && !m_uMembers.contains(m.from)) {
            m_uSelfIds.insert(m.from);
        }

        ChatItem_S item;
        item.seq = m.seq;
        item.from = m.from;
        item.ts = m.ts > 0 ? m.ts : QDateTime::currentMSecsSinceEpoch();
        item.name = m.name;
        item.system = false;
        item.kind = m.kind;
        if (m.kind == MessageKind_EN::Voice) {
            item.audio = m.audio;
            item.durationMs = m.durationMs;
        } else {
            item.text = m.text;
        }

        model_.push_back(item);
        if (m.from != 0 && !m_uMembers.contains(m.from)) {
            m_uMembers.insert(m.from, m.name);
        }
        changed = true;
    }

    if (!changed) return;

    sortModel();

    /* 历史里只翻最近 12 条英文：一进房就排队几十个请求会拖慢首屏 */
    if (m_pTranslation != nullptr && m_pTranslation->isEnabled()) {
        int budget = m_nHistoryTranslationBudget;
        for (int i = model_.size() - 1; i >= 0 && budget > 0; --i) {
            ChatItem_S& item = model_[i];
            if (item.system || item.translationState != 0) continue;
            if (item.from == 0 || m_uSelfIds.contains(item.from)) continue;
            if (!CTranslationService::looksEnglish(item.text)) continue;
            item.translationState = 1;
            --budget;
        }
    }

    rebuildView();
    rebuildMembers();
    scrollToBottom();

    if (m_pTranslation != nullptr && m_pTranslation->isEnabled()) {
        for (const ChatItem_S& item : model_) {
            if (item.translationState == 1) {
                m_pTranslation->requestMessage(item.seq, item.text);
            }
        }
    }

    /* 历史里的语音也补转写：只处理最近 3 条，避免进房时一次跑太多 whisper */
    int voiceBudget = 3;
    for (int i = model_.size() - 1; i >= 0 && voiceBudget > 0; --i) {
        const ChatItem_S& item = model_.at(i);
        if (item.kind != MessageKind_EN::Voice || item.transcriptState != 0) continue;
        requestVoiceTranscript(item.seq);
        --voiceBudget;
    }
}

/*
 * 功能描述:  执行 appendSystemText 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * param [in]  ts  参数。
 * return:    void
 */
void ChatPage::appendSystemText(const QString& text, qint64 ts) {
    if (text.isEmpty()) return;
    if (ts <= 0) ts = QDateTime::currentMSecsSinceEpoch();

    ChatItem_S item;
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
        if (!m_uMembers.contains(userId)) {
            m_uMembers.insert(userId, name);
            rebuildMembers();
        }
    } else if (event == QStringLiteral("leave")) {
        m_uMembers.remove(userId);
        rebuildMembers();
    } else if (event == QStringLiteral("rename")) {
        if (m_uMembers.contains(userId)) {
            m_uMembers[userId] = name;
            rebuildMembers();
        }
    }

    appendSystemText(text, ts);
}

/*
 * 功能描述:  执行 appendItem 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  item  参数。
 * return:    void
 */
void ChatPage::appendItem(const ChatItem_S& item) {
    model_.push_back(item);
    renderItem(item);
}

/*
 * 功能描述:  执行 renderItem 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  item  参数。
 * return:    void
 */
void ChatPage::renderItem(const ChatItem_S& item) {
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
        return;
    }

    const bool outgoing = m_uSelfIds.contains(item.from);

    /* ---- 语音消息 ---- */
    if (item.kind == MessageKind_EN::Voice) {
        auto* voiceBubble = new CMessageBubble(item.name, QString(), formatTime(item.ts),
                                              outgoing, item.from, false, m_pCanvas);
        voiceBubble->setVoice(item.audio, item.durationMs);
        connect(voiceBubble, &CMessageBubble::voicePlayRequested, this,
                [](const QByteArray& audio) { CVoicePlayer::instance()->play(audio); });

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

        if (item.seq > 0) m_uBubbles.insert(item.seq, voiceBubble);
        m_pChatLayout->addWidget(voiceBubble);
        return;
    }

    const bool playable = !outgoing && CTranslationService::looksEnglish(item.text);
    auto* bubble = new CMessageBubble(item.name, item.text, formatTime(item.ts),
                                     outgoing, item.from, playable, m_pCanvas);
    connect(bubble, &CMessageBubble::playRequested, this,
            [this, bubble](const QString& text) {
                if (m_stPlayingBubble != nullptr && m_stPlayingBubble != bubble) {
                    m_stPlayingBubble->setPlaying(false);
                }
                m_stPlayingBubble = bubble;
                m_pTts->speak(text);
            });
    if (item.translationState == 1) {
        bubble->setTranslationPending();
    } else if (item.translationState == 2) {
        bubble->setTranslation(item.translation);
    } else if (item.translationState == 3) {
        bubble->setTranslationFailed();
    }
    if (item.seq > 0) m_uBubbles.insert(item.seq, bubble);
    m_pChatLayout->addWidget(bubble);
}

/*
 * 功能描述:  执行 sortModel 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::sortModel() {
    /*
     * 功能描述:  执行 stable_sort 相关的处理，完成参数校验、核心逻辑与结果返回。
     * 作者:      JaTin
     * param [in]  begin  参数。
     * return:    返回值，含义见功能描述。
     */
    std::stable_sort(model_.begin(), model_.end(), [](const ChatItem_S& a, const ChatItem_S& b) {
        if (a.ts != b.ts) return a.ts < b.ts;
        return a.seq < b.seq;
    });
}

/*
 * 功能描述:  执行 rebuildView 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::rebuildView() {
    clearView();
    m_stLastDate = QDate();
    for (const ChatItem_S& item : model_) {
        renderItem(item);
    }
}

/*
 * 功能描述:  执行 clearView 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::clearView() {
    m_uBubbles.clear();
    m_stPlayingBubble = nullptr;
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
}

/*
 * 功能描述:  执行 scrollToBottom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::scrollToBottom() {
    m_nPendingScroll = true;
    QTimer::singleShot(0, this, [this] { forceScrollToBottom(); });
    /* 布局是异步结算的：内容高度变化后 rangeChanged 会再触发一次贴底 */
    QTimer::singleShot(150, this, [this] { m_nPendingScroll = false; });
}

/*
 * 功能描述:  执行 forceScrollToBottom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::forceScrollToBottom() {
    QScrollBar* bar = m_pScroll->verticalScrollBar();
    bar->setValue(bar->maximum());
}

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
 * 功能描述:  执行 setOnline 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  online  参数。
 * return:    void
 */
void ChatPage::setOnline(bool online) {
    m_pComposer->setEnabled(online);
    m_pSendBtn->setEnabled(online);
    m_pMicBtn->setEnabled(online && m_nVoiceAvailable);
    m_pComposer->setPlaceholderText(
        online ? QString::fromUtf8(kComposerHint)
               : QStringLiteral("连接已断开，正在重连…"));
}

/* ---------------------------------------------------------------- 语音输入 */

/*
 * 功能描述:  执行 requestSuggestions 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::requestSuggestions() {
    if (m_pReply == nullptr || !m_pReply->isEnabled()) return;

    /* 取最近 6 条聊天（跳过系统提示）作为上下文 */
    QVector<ChatMessage_S> context;
    for (int i = model_.size() - 1; i >= 0 && context.size() < 6; --i) {
        const ChatItem_S& item = model_.at(i);
        if (item.system) continue;
        ChatMessage_S message;
        message.seq = item.seq;
        message.from = item.from;
        message.name = item.name;
        message.text = item.text;
        message.ts = item.ts;
        context.prepend(message);
    }
    if (context.isEmpty()) return;

    m_pSuggestionBar->showPending();
    m_pReply->request(context);
}

/*
 * 功能描述:  执行 onInputDebounced 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::onInputDebounced() {
    const QString text = m_pComposer->toPlainText().trimmed();
    if (text.isEmpty() || !CTranslationService::looksChinese(text)) {
        m_pTranslationCard->hideCard();
        return;
    }
    if (m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    m_pSuggestionBar->hideBar();
    m_pTranslationCard->showPending();
    m_pTranslation->requestInput(text);
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
    m_pVoice->startRecording(CVoiceInput::Purpose_EN::Send);
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

    const QByteArray container = CVoiceCodec::encode(pcm);
    if (container.isEmpty()) {
        showVoiceStatus(QStringLiteral("语音编码失败：%1").arg(CVoiceCodec::lastError()), true, 4000);
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

/*
 * 功能描述:  执行 requestVoiceTranscript 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * return:    void
 */
void ChatPage::requestVoiceTranscript(qint64 seq) {
    if (seq <= 0) return;

    for (ChatItem_S& item : model_) {
        if (item.seq != seq) continue;
        if (item.kind != MessageKind_EN::Voice) return;
        /* 已在转写或已完成 */
        if (item.transcriptState != 0) return;
        item.transcriptState = 1;
        break;
    }

    m_uTranscriptQueue.enqueue(seq);
    pumpTranscript();
}

/*
 * 功能描述:  执行 pumpTranscript 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::pumpTranscript() {
    if (m_uTranscribingSeq > 0 || m_uTranscriptQueue.isEmpty()) return;

    if (m_pVoice == nullptr || !m_pVoice->modelReady()) {
        /* whisper 还在加载：过一会儿再试，别把队列丢了 */
        QTimer::singleShot(1500, this, &ChatPage::pumpTranscript);
        return;
    }

    while (!m_uTranscriptQueue.isEmpty()) {
        const qint64 seq = m_uTranscriptQueue.dequeue();

        const ChatItem_S* target = nullptr;
        for (const ChatItem_S& item : model_) {
            if (item.seq == seq) {
                target = &item;
                break;
            }
        }
        if (target == nullptr || target->transcriptState != 1) continue;

        const QByteArray pcm = CVoiceCodec::decode(target->audio);
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
        file.write(CVoiceCodec::toWav(pcm));
        file.close();

        m_uTranscribingSeq = seq;
        m_pVoice->transcribeFile(wav);
        /* 一次只跑一条，textReady 里再取下一条 */
        return;
    }
}

/*
 * 功能描述:  执行 updateTranscript 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * param [in]  text  参数。
 * param [in]  ok  参数。
 * return:    void
 */
void ChatPage::updateTranscript(qint64 seq, const QString& text, bool ok) {
    for (ChatItem_S& item : model_) {
        if (item.seq != seq) continue;
        if (ok && !text.isEmpty()) {
            item.transcript = text;
            item.transcriptState = 2;
            /* 转写出来的是英文，接着走翻译 */
            if (m_pTranslation != nullptr && m_pTranslation->isEnabled()
                && CTranslationService::looksEnglish(text)) {
                item.translationState = 1;
                m_pTranslation->requestMessage(seq, text);
            }
        } else {
            item.transcriptState = 3;
        }
        break;
    }

    if (CMessageBubble* bubble = m_uBubbles.value(seq, nullptr)) {
        if (ok && !text.isEmpty()) {
            bubble->setTranscript(text);
            if (m_pTranslation != nullptr && m_pTranslation->isEnabled()
                && CTranslationService::looksEnglish(text)) {
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
        if (m_uSeqs.contains(seq)) return;
        m_uSeqs.insert(seq);
    }

    if (from != 0 && !m_uMembers.contains(from)) {
        m_uMembers.insert(from, name);
        rebuildMembers();
    }

    ChatItem_S item;
    item.seq = seq;
    item.from = from;
    item.ts = ts;
    item.name = name;
    item.kind = MessageKind_EN::Voice;
    item.audio = audio;
    item.durationMs = durationMs;
    item.system = false;

    appendItem(item);
    scrollToBottom();

    /* 语音也要能看懂：本地 whisper 转写成英文，再翻译成中文 */
    requestVoiceTranscript(seq);

    const bool incoming = (from != 0) && !m_uSelfIds.contains(from);
    if (incoming && m_pReply != nullptr && m_pReply->isEnabled()
        && m_pComposer->toPlainText().trimmed().isEmpty()) {
        m_pReplyDebounce->start();
    }
}

/*
 * 功能描述:  执行 insertVoiceText 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void ChatPage::insertVoiceText(const QString& text) {
    if (text.isEmpty()) return;

    QString payload = text;
    const QString existing = m_pComposer->toPlainText();
    if (!existing.isEmpty()
        && !existing.endsWith(QLatin1Char(' '))
        && !existing.endsWith(QLatin1Char('\n'))) {
        payload.prepend(QLatin1Char(' '));
    }

    QTextCursor cursor = m_pComposer->textCursor();
    cursor.insertText(payload);
    m_pComposer->setTextCursor(cursor);
    m_pComposer->setFocus();
}

namespace {
/*
 * 功能描述:  执行 toClientConfig 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  source  参数。
 * return:    返回值，含义见功能描述。
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

/*
 * 功能描述:  执行 applyConfig 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  settings  参数。
 * param [in]  initial  参数。
 * return:    void
 */
void ChatPage::applyConfig(const AppConfig::Settings_S& settings, bool initial) {
    /* 对话/建议用主模型 */
    m_pLlm->setConfig(toClientConfig(settings.llm));

    /* 翻译：配了专用模型就用它，否则沿用主模型参数 */
    const bool separate = settings.translation.hasOwnLlm;
    m_pTranslateLlm->setConfig(toClientConfig(separate ? settings.translation.llm : settings.llm));

    m_nHistoryTranslationBudget = qMax(0, settings.translation.maxHistoryMessages);
    m_strConfigError = settings.error;

    if (!settings.error.isEmpty()) {
        if (initial) {
            /* 构造期发出会丢（CMainWindow 尚未 connect），推到事件循环第一轮 */
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

    /* 之前因为没配 Key 而失败的条目，填好之后自动补翻 */
    if (m_pTranslateLlm->isEnabled()) retryFailedTranslations();
}

/*
 * 功能描述:  执行 retryFailedTranslations 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::retryFailedTranslations() {
    if (m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    QVector<QPair<qint64, QString>> retry;
    int budget = m_nHistoryTranslationBudget;
    for (int i = model_.size() - 1; i >= 0 && budget > 0; --i) {
        ChatItem_S& item = model_[i];
        if (item.system) continue;
        /* 3 = 之前失败；0 = 当时翻译没启用，配置改好后一起补 */
        if (item.translationState != 3 && item.translationState != 0) continue;
        if (item.from == 0 || m_uSelfIds.contains(item.from)) continue;
        if (!CTranslationService::looksEnglish(item.text)) continue;
        item.translationState = 1;
        if (CMessageBubble* bubble = m_uBubbles.value(item.seq, nullptr)) {
            bubble->setTranslationPending();
        }
        retry.prepend(qMakePair(item.seq, item.text));
        --budget;
    }
    for (const auto& pair : retry) {
        m_pTranslation->requestMessage(pair.first, pair.second);
    }
}

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
        error ? QStringLiteral("color:#E5484D; background:#FDF2F2; padding:6px 18px; font-size:9pt;")
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
    m_pRoomSubtitle->setText(QStringLiteral("%1 位成员").arg(m_uMembers.size()));
    m_pMemberBtn->setText(QStringLiteral("成员 %1").arg(m_uMembers.size()));
    if (m_pMemberTitle != nullptr) {
        m_pMemberTitle->setText(QStringLiteral("成员（%1）").arg(m_uMembers.size()));
    }
}

/*
 * 功能描述:  执行 rebuildMembers 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void ChatPage::rebuildMembers() {
    while (m_pMemberListLayout->count() > 1) {
        QLayoutItem* item = m_pMemberListLayout->takeAt(0);
        if (item->widget() != nullptr) item->widget()->deleteLater();
        delete item;
    }

    for (auto it = m_uMembers.constBegin(); it != m_uMembers.constEnd(); ++it) {
        auto* row = new QWidget();
        row->setObjectName("MemberRow");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(6, 4, 6, 4);
        rowLayout->setSpacing(8);

        auto* avatar = new CAvatar(row);
        avatar->setFixedSize(28, 28);
        avatar->setFontPx(12);
        avatar->setText(it.value());
        avatar->setColor(CAvatar::colorForId(it.key()));

        auto* nameLabel = new QLabel(it.value(), row);
        nameLabel->setStyleSheet(QStringLiteral("font-size: 9.5pt;"));
        if (m_uSelfIds.contains(it.key())) {
            nameLabel->setText(it.value() + QStringLiteral("（我）"));
        }

        rowLayout->addWidget(avatar);
        rowLayout->addWidget(nameLabel, 1);

        m_pMemberListLayout->insertWidget(m_pMemberListLayout->count() - 1, row);
    }

    updateHeader();
}
