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
 * 功能描述:  构造 ComposerEdit 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QPlainTextEdit(。
 * return:    无返回值（构造函数或析构函数）。
 */
ComposerEdit::ComposerEdit(QWidget* pParent) : QPlainTextEdit(pParent) {
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
}

/*
 * 功能描述:  执行密钥 press 事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 QKeyEvent*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ComposerEdit::keyPressEvent(QKeyEvent* pEvent) {
    const bool nIsEnter = (pEvent->key() == Qt::Key_Return || pEvent->key() == Qt::Key_Enter);
    if (nIsEnter && !(pEvent->modifiers() & Qt::ShiftModifier)) {
        emit submit();
        pEvent->accept();
        return;
    }
    QPlainTextEdit::keyPressEvent(pEvent);
}

/* ---------------------------------------------------------------- ChatPage */

namespace {

const char* kComposerHint = "输入消息，Enter 发送，Shift+Enter 换行";

/*
 * 功能描述:  获取formattime。
 * 作者:      JaTin
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString formatTime(qint64 lTs) {
    return QDateTime::fromMSecsSinceEpoch(lTs).toString(QStringLiteral("HH:mm"));
}

/*
 * 功能描述:  获取formatdate。
 * 作者:      JaTin
 * param [in]   stDate           输入数据，类型 const QDate&。
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString formatDate(const QDate& stDate) {
    const QDate stToday = QDate::currentDate();
    if (stDate == stToday) return QStringLiteral("今天");
    if (stDate == stToday.addDays(-1)) return QStringLiteral("昨天");
    if (stDate.year() == stToday.year()) return stDate.toString(QStringLiteral("M月d日"));
    return stDate.toString(QStringLiteral("yyyy年M月d日"));
}

/* 模型路径优先级：QSettings 覆盖 > 编译期默认（源码树 models/ 目录） */
/*
 * 功能描述:  获取resolve 模型名文件路径；实现上读写本地持久化设置。
 * 作者:      JaTin
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString resolveModelPath() {
    const QString strConfigured = QSettings().value(QStringLiteral("voice/modelPath")).toString();
    if (!strConfigured.isEmpty()) return strConfigured;
#ifdef DP_DEFAULT_MODEL_PATH
    return QString::fromUtf8(DP_DEFAULT_MODEL_PATH);
#else
    return QString();
#endif
}

/* namespace */
}

/*
 * 功能描述:  构造 ChatPage 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QWidget(。
 * return:    无返回值（构造函数或析构函数）。
 */
ChatPage::ChatPage(QWidget* pParent) : QWidget(pParent) {
    setObjectName("AppRoot");

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QHBoxLayout(this);
    pRoot->setContentsMargins(0, 0, 0, 0);
    pRoot->setSpacing(0);

    /* ---- 左侧：聊天列 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pChatColumn = new QWidget(this);
    auto* pChatColumnLayout = new QVBoxLayout(pChatColumn);
    pChatColumnLayout->setContentsMargins(0, 0, 0, 0);
    pChatColumnLayout->setSpacing(0);

    /* 顶栏 */
    auto* pHeader = new QFrame(pChatColumn);
    pHeader->setObjectName("ChatHeader");
    pHeader->setFixedHeight(64);
    auto* pHeaderLayout = new QHBoxLayout(pHeader);
    pHeaderLayout->setContentsMargins(12, 0, 16, 0);
    pHeaderLayout->setSpacing(10);

    auto* pBackBtn = new QPushButton(QStringLiteral("←"), pHeader);
    pBackBtn->setObjectName("LinkButton");
    pBackBtn->setCursor(Qt::PointingHandCursor);
    pBackBtn->setFixedSize(34, 34);
    pBackBtn->setStyleSheet(QStringLiteral("font-size: 17pt; color: #3390EC;"));
    pBackBtn->setToolTip(QStringLiteral("返回并离开房间"));
    pHeaderLayout->addWidget(pBackBtn);

    m_pRoomAvatar = new CAvatar(pHeader);
    m_pRoomAvatar->setFixedSize(40, 40);
    m_pRoomAvatar->setFontPx(18);
    m_pRoomAvatar->setText(QStringLiteral("#"));
    m_pRoomAvatar->setColor(theme::accent);
    pHeaderLayout->addWidget(m_pRoomAvatar);

    auto* pTitleColumn = new QVBoxLayout();
    pTitleColumn->setSpacing(1);
    pTitleColumn->setContentsMargins(0, 0, 0, 0);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomTitle = new QLabel(pHeader);
    m_pRoomTitle->setStyleSheet(QStringLiteral("font-size: 11.5pt; font-weight: 700;"));
    pTitleColumn->addWidget(m_pRoomTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pRoomSubtitle = new QLabel(pHeader);
    m_pRoomSubtitle->setObjectName("HintLabel");
    pTitleColumn->addWidget(m_pRoomSubtitle);

    pHeaderLayout->addLayout(pTitleColumn, 1);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberBtn = new QPushButton(pHeader);
    m_pMemberBtn->setObjectName("LinkButton");
    m_pMemberBtn->setCursor(Qt::PointingHandCursor);
    pHeaderLayout->addWidget(m_pMemberBtn);

    pChatColumnLayout->addWidget(pHeader);

    /* 消息区 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pScroll = new QScrollArea(pChatColumn);
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
    pChatColumnLayout->addWidget(m_pScroll, 1);

    /* 输入区 */
    m_pSuggestionBar = new CSuggestionBar(pChatColumn);
    pChatColumnLayout->addWidget(m_pSuggestionBar);

    m_pTranslationCard = new CTranslationCard(pChatColumn);
    pChatColumnLayout->addWidget(m_pTranslationCard);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pVoiceStatus = new QLabel(pChatColumn);
    m_pVoiceStatus->setObjectName("VoiceStatus");
    m_pVoiceStatus->setVisible(false);
    pChatColumnLayout->addWidget(m_pVoiceStatus);

    auto* pComposerFrame = new QFrame(pChatColumn);
    pComposerFrame->setObjectName("Composer");
    auto* pComposerLayout = new QHBoxLayout(pComposerFrame);
    pComposerLayout->setContentsMargins(16, 10, 16, 10);
    pComposerLayout->setSpacing(10);

    m_pMicBtn = new CMicButton(pComposerFrame);
    pComposerLayout->addWidget(m_pMicBtn, 0, Qt::AlignVCenter);

    m_pComposer = new ComposerEdit(pComposerFrame);
    m_pComposer->setObjectName("ComposerEdit");
    m_pComposer->setPlaceholderText(QString::fromUtf8(kComposerHint));
    m_pComposer->setFixedHeight(46);
    pComposerLayout->addWidget(m_pComposer, 1);

    m_pSendBtn = new CSendButton(pComposerFrame);
    pComposerLayout->addWidget(m_pSendBtn, 0, Qt::AlignVCenter);

    pChatColumnLayout->addWidget(pComposerFrame);

    pRoot->addWidget(pChatColumn, 1);

    /* ---- 右侧：成员面板 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberPanel = new QFrame(this);
    m_pMemberPanel->setObjectName("MemberPanel");
    m_pMemberPanel->setFixedWidth(220);
    m_pMemberPanel->setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pMemberPanelLayout = new QVBoxLayout(m_pMemberPanel);
    pMemberPanelLayout->setContentsMargins(12, 14, 12, 14);
    pMemberPanelLayout->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberTitle = new QLabel(m_pMemberPanel);
    m_pMemberTitle->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: 700;"));
    pMemberPanelLayout->addWidget(m_pMemberTitle);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pMemberScroll = new QScrollArea(m_pMemberPanel);
    pMemberScroll->setWidgetResizable(true);
    pMemberScroll->setFrameShape(QFrame::NoFrame);
    pMemberScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pMemberScroll->setStyleSheet(QStringLiteral("background: transparent;"));

    auto* pMemberCanvas = new QWidget();
    pMemberCanvas->setStyleSheet(QStringLiteral("background: transparent;"));
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberListLayout = new QVBoxLayout(pMemberCanvas);
    m_pMemberListLayout->setContentsMargins(0, 0, 0, 0);
    m_pMemberListLayout->setSpacing(4);
    m_pMemberListLayout->addStretch(1);
    pMemberScroll->setWidget(pMemberCanvas);

    pMemberPanelLayout->addWidget(pMemberScroll, 1);
    pRoot->addWidget(m_pMemberPanel);

    /* ---- 连接 ---- */
    connect(pBackBtn, &QPushButton::clicked, this, &ChatPage::backRequested);
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

    connect(m_pVoice, &CVoiceInput::modelLoaded, this, [this](bool nOk, const QString& message) {
        m_nVoiceAvailable = nOk;
        if (!nOk) {
            m_pMicBtn->setEnabled(false);
            m_pMicBtn->setToolTip(message);
            showVoiceStatus(message, true, 6000);
        }
    });
    connect(m_pVoice, &CVoiceInput::pcmReady, this, &ChatPage::onVoiceRecorded);

    /* 语音播放：状态同步到对应气泡的语音条 */
    connect(CVoicePlayer::instance(), &CVoicePlayer::playingChanged, this, [this](bool playing) {
        const QByteArray byCurrent = CVoicePlayer::instance()->current();
        for (const ChatItem_S& stItem : model_) {
            if (stItem.kind != MessageKind_EN::Voice || stItem.seq <= 0) continue;
            CMessageBubble* pBubble = m_uBubbles.value(stItem.seq, nullptr);
            if (pBubble == nullptr) continue;
            pBubble->setPlaying(playing && !byCurrent.isEmpty() && stItem.audio == byCurrent);
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
    connect(m_pVoice, &CVoiceInput::textReady, this, [this](const QString& strText, qint64 elapsedMs) {
        m_pMicBtn->setRecording(false);

        /* 语音消息的转写结果：回填到气泡，不再插进输入框 */
        if (m_uTranscribingSeq > 0) {
            const qint64 lSeq = m_uTranscribingSeq;
            m_uTranscribingSeq = 0;
            updateTranscript(lSeq, strText, true);
            /* 清掉「识别中…」 */
            showVoiceStatus(QString(), false, 0);
            pumpTranscript();
            return;
        }

        /* 兜底路径（--voice-file 诊断等） */
        insertVoiceText(strText);
        showVoiceStatus(QStringLiteral("已识别（%1 ms），可直接编辑后发送").arg(elapsedMs),
                        false, 2500);
    });
    connect(m_pVoice, &CVoiceInput::failed, this, [this](const QString& message) {
        m_pMicBtn->setRecording(false);

        if (m_uTranscribingSeq > 0) {
            const qint64 lSeq = m_uTranscribingSeq;
            m_uTranscribingSeq = 0;
            updateTranscript(lSeq, QString(), false);
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

    const QString strConfigPath = AppConfig::resolvePath();
    /* 首次运行生成一份可编辑的模板 */
    AppConfig::ensureTemplate(strConfigPath);
    applyConfig(AppConfig::load(), true);

    /* 配置文件热重载：改完保存就生效，不用重启 */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pConfigWatcher = new QFileSystemWatcher(this);
    if (!strConfigPath.isEmpty()) {
        m_pConfigWatcher->addPath(strConfigPath);
        m_pConfigWatcher->addPath(QFileInfo(strConfigPath).absolutePath());
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
            [this](qint64 lSeq, const QString& chinese) {
                for (ChatItem_S& stItem : model_) {
                    if (stItem.seq == lSeq) {
                        stItem.translation = chinese;
                        stItem.translationState = 2;
                        break;
                    }
                }
                if (CMessageBubble* pBubble = m_uBubbles.value(lSeq, nullptr)) {
                    pBubble->setTranslation(chinese);
                }
            });
    connect(m_pTranslation, &CTranslationService::inputTranslated, this,
            [this](const QString& stSource, const QString& english) {
                /* 用户已经改了输入内容就丢弃这次结果 */
                if (stSource != m_pComposer->toPlainText().trimmed()) return;
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
            [this](const QString& strText) { m_pTts->speak(strText); });
    connect(m_pTranslationCard, &CTranslationCard::useRequested, this, [this](const QString& strText) {
        m_pComposer->setPlainText(strText);
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
    connect(m_pSuggestionBar, &CSuggestionBar::useRequested, this, [this](const QString& strText) {
        m_pComposer->setPlainText(strText);
        m_pComposer->moveCursor(QTextCursor::End);
        m_pComposer->setFocus();
        m_pSuggestionBar->hideBar();
    });
    connect(m_pSuggestionBar, &CSuggestionBar::playRequested, this,
            [this](const QString& strText) { m_pTts->speak(strText); });

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
            [this](qint64 lSeq, const QString& reason) {
                if (lSeq <= 0) {
                    /* 输入框的中文翻译失败：卡片不能一直停在「翻译中…」 */
                    m_pTranslationCard->hideCard();
                    showVoiceStatus(reason, true, 3000);
                    return;
                }
                for (ChatItem_S& stItem : model_) {
                    if (stItem.seq == lSeq) {
                        stItem.translationState = 3;
                        break;
                    }
                }
                if (CMessageBubble* pBubble = m_uBubbles.value(lSeq, nullptr)) {
                    pBubble->setTranslationFailed();
                }
            });
}

/*
 * 功能描述:  进入房间。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   nCreated         输入数据，类型 bool。
 * param [in]   aMembers         初始成员列表，类型 const QVector<MemberInfo_S>&。
 * param [in]   lSelfId          本人会话编号，类型 qint64。
 * param [in]   strSelfName      本人昵称，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */

void ChatPage::enterRoom(const QString& strRoom, bool nCreated, const QVector<MemberInfo_S>& aMembers,
                         qint64 lSelfId, const QString& strSelfName) {
    m_strRoom = strRoom;
    m_uSelfId = lSelfId;
    m_strSelfName = strSelfName;
    m_uSelfIds.insert(lSelfId);

    m_uMembers.clear();
    for (const MemberInfo_S& stM : aMembers) {
        m_uMembers.insert(stM.id, stM.name);
    }

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(strRoom));

    model_.clear();
    m_uSeqs.clear();
    clearView();
    m_stLastDate = QDate();

    appendSystemText(nCreated ? QStringLiteral("你创建了房间 %1").arg(strRoom)
                             : QStringLiteral("你加入了房间 %1").arg(strRoom),
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

/*
 * 功能描述:  恢复房间。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   aMembers         初始成员列表，类型 const QVector<MemberInfo_S>&。
 * param [in]   lSelfId          本人会话编号，类型 qint64。
 * param [in]   strSelfName      本人昵称，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::resumeRoom(const QString& strRoom, const QVector<MemberInfo_S>& aMembers,
                          qint64 lSelfId, const QString& strSelfName) {
    m_strRoom = strRoom;
    m_uSelfId = lSelfId;
    m_strSelfName = strSelfName;
    /* 重连后服务端会分配新 id，旧消息仍应显示为自己发的 */
    m_uSelfIds.insert(lSelfId);

    m_uMembers.clear();
    for (const MemberInfo_S& stM : aMembers) {
        m_uMembers.insert(stM.id, stM.name);
    }

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(strRoom));
    appendSystemText(QStringLiteral("已重新连接"), QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    rebuildMembers();
    setOnline(true);
    m_pComposer->setFocus();
}

/*
 * 功能描述:  清空房间。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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

/*
 * 功能描述:  追加chat。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   lFrom            输入数据，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */

void ChatPage::appendChat(qint64 lSeq, qint64 lFrom, const QString& strName,
                          const QString& strText, qint64 lTs) {
    if (lTs <= 0) lTs = QDateTime::currentMSecsSinceEpoch();

    if (lSeq > 0) {
        /* 历史补发与实时推送可能重叠 */
        if (m_uSeqs.contains(lSeq)) return;
        m_uSeqs.insert(lSeq);
    }

    if (lFrom != 0 && !m_uMembers.contains(lFrom)) {
        m_uMembers.insert(lFrom, strName);
        rebuildMembers();
    }

    ChatItem_S stItem;
    stItem.seq = lSeq;
    stItem.from = lFrom;
    stItem.ts = lTs;
    stItem.name = strName;
    stItem.text = strText;
    stItem.system = false;

    const bool nNeedTranslation = lFrom != 0 && !m_uSelfIds.contains(lFrom)
                                 && m_pTranslation != nullptr && m_pTranslation->isEnabled()
                                 && CTranslationService::looksEnglish(strText);
    if (nNeedTranslation) stItem.translationState = 1;

    appendItem(stItem);
    scrollToBottom();

    if (nNeedTranslation) m_pTranslation->requestMessage(lSeq, strText);

    /* 他人消息到达且输入框为空 -> 稍后生成 3 条建议 */
    const bool nIncoming = (lFrom != 0) && !m_uSelfIds.contains(lFrom);
    if (nIncoming && m_pReply != nullptr && m_pReply->isEnabled()
        && m_pComposer->toPlainText().trimmed().isEmpty()) {
        m_pReplyDebounce->start();
    }
}

/*
 * 功能描述:  合并历史消息。
 * 作者:      JaTin
 * param [in]   aMessages        消息记录，类型 const QVector<ChatMessage_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::mergeHistory(const QVector<ChatMessage_S>& aMessages) {
    if (aMessages.isEmpty()) return;

    bool nChanged = false;
    for (const ChatMessage_S& stM : aMessages) {
        if (stM.seq > 0 && m_uSeqs.contains(stM.seq)) continue;
        if (stM.seq > 0) m_uSeqs.insert(stM.seq);

        /* 历史里「同名、且已不在房间」的 id 视为本人旧会话： */
        /* 服务端每次连接都会分配新 userId，重启客户端后旧消息不该显示成别人的。 */
        if (stM.from != 0 && !m_uSelfIds.contains(stM.from)
            && stM.name == m_strSelfName && !m_uMembers.contains(stM.from)) {
            m_uSelfIds.insert(stM.from);
        }

        ChatItem_S stItem;
        stItem.seq = stM.seq;
        stItem.from = stM.from;
        stItem.ts = stM.ts > 0 ? stM.ts : QDateTime::currentMSecsSinceEpoch();
        stItem.name = stM.name;
        stItem.system = false;
        stItem.kind = stM.kind;
        if (stM.kind == MessageKind_EN::Voice) {
            stItem.audio = stM.audio;
            stItem.durationMs = stM.durationMs;
        } else {
            stItem.text = stM.text;
        }

        model_.push_back(stItem);
        if (stM.from != 0 && !m_uMembers.contains(stM.from)) {
            m_uMembers.insert(stM.from, stM.name);
        }
        nChanged = true;
    }

    if (!nChanged) return;

    sortModel();

    /* 历史里只翻最近 12 条英文：一进房就排队几十个请求会拖慢首屏 */
    if (m_pTranslation != nullptr && m_pTranslation->isEnabled()) {
        int nBudget = m_nHistoryTranslationBudget;
        for (int i = model_.size() - 1; i >= 0 && nBudget > 0; --i) {
            ChatItem_S& stItem = model_[i];
            if (stItem.system || stItem.translationState != 0) continue;
            if (stItem.from == 0 || m_uSelfIds.contains(stItem.from)) continue;
            if (!CTranslationService::looksEnglish(stItem.text)) continue;
            stItem.translationState = 1;
            --nBudget;
        }
    }

    rebuildView();
    rebuildMembers();
    scrollToBottom();

    if (m_pTranslation != nullptr && m_pTranslation->isEnabled()) {
        for (const ChatItem_S& stItem : model_) {
            if (stItem.translationState == 1) {
                m_pTranslation->requestMessage(stItem.seq, stItem.text);
            }
        }
    }

    /* 历史里的语音也补转写：只处理最近 3 条，避免进房时一次跑太多 whisper */
    int nVoiceBudget = 3;
    for (int i = model_.size() - 1; i >= 0 && nVoiceBudget > 0; --i) {
        const ChatItem_S& stItem = model_.at(i);
        if (stItem.kind != MessageKind_EN::Voice || stItem.transcriptState != 0) continue;
        requestVoiceTranscript(stItem.seq);
        --nVoiceBudget;
    }
}

/*
 * 功能描述:  追加系统提示词文本。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::appendSystemText(const QString& strText, qint64 lTs) {
    if (strText.isEmpty()) return;
    if (lTs <= 0) lTs = QDateTime::currentMSecsSinceEpoch();

    ChatItem_S stItem;
    stItem.ts = lTs;
    stItem.text = strText;
    stItem.system = true;

    appendItem(stItem);
    scrollToBottom();
}

/*
 * 功能描述:  执行on 系统提示词事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 const QString&。
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::onSystemEvent(const QString& pEvent, qint64 lUserId, const QString& strName,
                             const QString& /*oldName*/, const QString& strText, qint64 lTs) {
    if (pEvent == QStringLiteral("welcome") || pEvent == QStringLiteral("left")) {
        return;
    }

    if (pEvent == QStringLiteral("join")) {
        if (!m_uMembers.contains(lUserId)) {
            m_uMembers.insert(lUserId, strName);
            rebuildMembers();
        }
    } else if (pEvent == QStringLiteral("leave")) {
        m_uMembers.remove(lUserId);
        rebuildMembers();
    } else if (pEvent == QStringLiteral("rename")) {
        if (m_uMembers.contains(lUserId)) {
            m_uMembers[lUserId] = strName;
            rebuildMembers();
        }
    }

    appendSystemText(strText, lTs);
}

/*
 * 功能描述:  追加条目。
 * 作者:      JaTin
 * param [in]   stItem           条目，类型 const ChatItem_S&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::appendItem(const ChatItem_S& stItem) {
    model_.push_back(stItem);
    renderItem(stItem);
}

/*
 * 功能描述:  绘制条目；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   stItem           条目，类型 const ChatItem_S&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::renderItem(const ChatItem_S& stItem) {
    appendDateSeparatorIfNeeded(stItem.ts);

    if (stItem.system) {
        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        auto* pRow = new QWidget(m_pCanvas);
        auto* pRowLayout = new QHBoxLayout(pRow);
        pRowLayout->setContentsMargins(0, 2, 0, 2);

        auto* pPill = new QLabel(stItem.text, pRow);
        pPill->setObjectName("SystemPill");
        pPill->setWordWrap(true);
        pPill->setAlignment(Qt::AlignCenter);

        pPill->ensurePolished();
        const int nMaxPillWidth = 460;
        const QRect stNeeded = pPill->fontMetrics().boundingRect(
            QRect(0, 0, nMaxPillWidth - 30, 100000), Qt::TextWordWrap | Qt::AlignCenter, stItem.text);
        pPill->setFixedWidth(qBound(60, stNeeded.width() + 30, nMaxPillWidth));

        pRowLayout->addStretch(1);
        pRowLayout->addWidget(pPill, 0);
        pRowLayout->addStretch(1);

        m_pChatLayout->addWidget(pRow);
        return;
    }

    const bool nOutgoing = m_uSelfIds.contains(stItem.from);

    /* ---- 语音消息 ---- */
    if (stItem.kind == MessageKind_EN::Voice) {
        auto* pVoiceBubble = new CMessageBubble(stItem.name, QString(), formatTime(stItem.ts),
                                              nOutgoing, stItem.from, false, m_pCanvas);
        pVoiceBubble->setVoice(stItem.audio, stItem.durationMs);
        connect(pVoiceBubble, &CMessageBubble::voicePlayRequested, this,
                [](const QByteArray& byAudio) { CVoicePlayer::instance()->play(byAudio); });

        if (stItem.transcriptState == 1) {
            pVoiceBubble->setTranscriptPending();
        } else if (stItem.transcriptState == 2) {
            pVoiceBubble->setTranscript(stItem.transcript);
        } else if (stItem.transcriptState == 3) {
            pVoiceBubble->setTranscript(QStringLiteral("（未能识别语音内容）"));
        }

        if (stItem.translationState == 1) {
            pVoiceBubble->setTranslationPending();
        } else if (stItem.translationState == 2) {
            pVoiceBubble->setTranslation(stItem.translation);
        } else if (stItem.translationState == 3) {
            pVoiceBubble->setTranslationFailed();
        }

        if (stItem.seq > 0) m_uBubbles.insert(stItem.seq, pVoiceBubble);
        m_pChatLayout->addWidget(pVoiceBubble);
        return;
    }

    const bool nPlayable = !nOutgoing && CTranslationService::looksEnglish(stItem.text);
    auto* pBubble = new CMessageBubble(stItem.name, stItem.text, formatTime(stItem.ts),
                                     nOutgoing, stItem.from, nPlayable, m_pCanvas);
    connect(pBubble, &CMessageBubble::playRequested, this,
            [this, pBubble](const QString& strText) {
                if (m_stPlayingBubble != nullptr && m_stPlayingBubble != pBubble) {
                    m_stPlayingBubble->setPlaying(false);
                }
                m_stPlayingBubble = pBubble;
                m_pTts->speak(strText);
            });
    if (stItem.translationState == 1) {
        pBubble->setTranslationPending();
    } else if (stItem.translationState == 2) {
        pBubble->setTranslation(stItem.translation);
    } else if (stItem.translationState == 3) {
        pBubble->setTranslationFailed();
    }
    if (stItem.seq > 0) m_uBubbles.insert(stItem.seq, pBubble);
    m_pChatLayout->addWidget(pBubble);
}

/*
 * 功能描述:  执行sort 模型名。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::sortModel() {
    std::stable_sort(model_.begin(), model_.end(), [](const ChatItem_S& a, const ChatItem_S& b) {
        if (a.ts != b.ts) return a.ts < b.ts;
        return a.seq < b.seq;
    });
}

/*
 * 功能描述:  执行rebuild 视图。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::rebuildView() {
    clearView();
    m_stLastDate = QDate();
    for (const ChatItem_S& stItem : model_) {
        renderItem(stItem);
    }
}

/*
 * 功能描述:  清空视图。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::clearView() {
    m_uBubbles.clear();
    m_stPlayingBubble = nullptr;
    /* 索引 0 是弹簧，必须保留 */
    while (m_pChatLayout->count() > 1) {
        QLayoutItem* stItem = m_pChatLayout->takeAt(m_pChatLayout->count() - 1);
        if (stItem->widget() != nullptr) stItem->widget()->deleteLater();
        delete stItem;
    }
}

/*
 * 功能描述:  追加dateseparatorifneeded；实现上创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::appendDateSeparatorIfNeeded(qint64 lTs) {
    const QDate stDate = QDateTime::fromMSecsSinceEpoch(lTs).date();
    if (stDate == m_stLastDate) return;
    m_stLastDate = stDate;

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRow = new QWidget(m_pCanvas);
    auto* pRowLayout = new QHBoxLayout(pRow);
    pRowLayout->setContentsMargins(0, 6, 0, 2);

    auto* pPill = new QLabel(formatDate(stDate), pRow);
    pPill->setObjectName("DatePill");
    pPill->setAlignment(Qt::AlignCenter);

    pRowLayout->addStretch(1);
    pRowLayout->addWidget(pPill, 0);
    pRowLayout->addStretch(1);

    m_pChatLayout->addWidget(pRow);
}

/*
 * 功能描述:  转换为scrollbottom。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::scrollToBottom() {
    m_nPendingScroll = true;
    QTimer::singleShot(0, this, [this] { forceScrollToBottom(); });
    /* 布局是异步结算的：内容高度变化后 rangeChanged 会再触发一次贴底 */
    QTimer::singleShot(150, this, [this] { m_nPendingScroll = false; });
}

/*
 * 功能描述:  转换为forcescrollbottom。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::forceScrollToBottom() {
    QScrollBar* pBar = m_pScroll->verticalScrollBar();
    pBar->setValue(pBar->maximum());
}

/*
 * 功能描述:  发送current 文本。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::sendCurrentText() {
    if (!m_pComposer->isEnabled()) return;
    const QString strText = m_pComposer->toPlainText().trimmed();
    if (strText.isEmpty()) return;
    emit sendRequested(strText);
    m_pComposer->clear();
    m_pComposer->setFocus();
}

/*
 * 功能描述:  设置online。
 * 作者:      JaTin
 * param [in]   nOnline          输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::setOnline(bool nOnline) {
    m_pComposer->setEnabled(nOnline);
    m_pSendBtn->setEnabled(nOnline);
    m_pMicBtn->setEnabled(nOnline && m_nVoiceAvailable);
    m_pComposer->setPlaceholderText(
        nOnline ? QString::fromUtf8(kComposerHint)
               : QStringLiteral("连接已断开，正在重连…"));
}

/* ---------------------------------------------------------------- 语音输入 */

/*
 * 功能描述:  请求智能建议列表。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::requestSuggestions() {
    if (m_pReply == nullptr || !m_pReply->isEnabled()) return;

    /* 取最近 6 条聊天（跳过系统提示）作为上下文 */
    QVector<ChatMessage_S> aContext;
    for (int i = model_.size() - 1; i >= 0 && aContext.size() < 6; --i) {
        const ChatItem_S& stItem = model_.at(i);
        if (stItem.system) continue;
        ChatMessage_S stMessage;
        stMessage.seq = stItem.seq;
        stMessage.from = stItem.from;
        stMessage.name = stItem.name;
        stMessage.text = stItem.text;
        stMessage.ts = stItem.ts;
        aContext.prepend(stMessage);
    }
    if (aContext.isEmpty()) return;

    m_pSuggestionBar->showPending();
    m_pReply->request(aContext);
}

/*
 * 功能描述:  执行on 输入 debounced。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::onInputDebounced() {
    const QString strText = m_pComposer->toPlainText().trimmed();
    if (strText.isEmpty() || !CTranslationService::looksChinese(strText)) {
        m_pTranslationCard->hideCard();
        return;
    }
    if (m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    m_pSuggestionBar->hideBar();
    m_pTranslationCard->showPending();
    m_pTranslation->requestInput(strText);
}

/*
 * 功能描述:  切换语音录制状态：空闲时开始采集，录制中则停止并送出音频。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
 * 功能描述:  执行on 语音消息 recorded。
 * 作者:      JaTin
 * param [in]   byPcm            输入数据，类型 const QByteArray&。
 * param [in]   nDurationMs      语音时长，单位毫秒，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::onVoiceRecorded(const QByteArray& byPcm, int nDurationMs) {
    if (byPcm.isEmpty()) return;

    const QByteArray byContainer = CVoiceCodec::encode(byPcm);
    if (byContainer.isEmpty()) {
        showVoiceStatus(QStringLiteral("语音编码失败：%1").arg(CVoiceCodec::lastError()), true, 4000);
        return;
    }
    if (byContainer.size() > static_cast<int>(dp::kMaxVoiceBytes)) {
        showVoiceStatus(QStringLiteral("语音太长（%1 秒），请控制在 %2 秒内")
                            .arg(nDurationMs / 1000)
                            .arg(dp::kMaxVoiceDurationMs / 1000),
                        true, 4000);
        return;
    }

    emit sendVoiceRequested(byContainer, nDurationMs);
    showVoiceStatus(QStringLiteral("已发送语音（%1 秒）").arg(nDurationMs / 1000.0, 0, 'f', 1),
                    false, 2000);
}

/*
 * 功能描述:  请求语音消息转写文本。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::requestVoiceTranscript(qint64 lSeq) {
    if (lSeq <= 0) return;

    for (ChatItem_S& stItem : model_) {
        if (stItem.seq != lSeq) continue;
        if (stItem.kind != MessageKind_EN::Voice) return;
        /* 已在转写或已完成 */
        if (stItem.transcriptState != 0) return;
        stItem.transcriptState = 1;
        break;
    }

    m_uTranscriptQueue.enqueue(lSeq);
    pumpTranscript();
}

/*
 * 功能描述:  执行pump 转写文本。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::pumpTranscript() {
    if (m_uTranscribingSeq > 0 || m_uTranscriptQueue.isEmpty()) return;

    if (m_pVoice == nullptr || !m_pVoice->modelReady()) {
        /* whisper 还在加载：过一会儿再试，别把队列丢了 */
        QTimer::singleShot(1500, this, &ChatPage::pumpTranscript);
        return;
    }

    while (!m_uTranscriptQueue.isEmpty()) {
        const qint64 lSeq = m_uTranscriptQueue.dequeue();

        const ChatItem_S* pTarget = nullptr;
        for (const ChatItem_S& stItem : model_) {
            if (stItem.seq == lSeq) {
                pTarget = &stItem;
                break;
            }
        }
        if (pTarget == nullptr || pTarget->transcriptState != 1) continue;

        const QByteArray byPcm = CVoiceCodec::decode(pTarget->audio);
        if (byPcm.isEmpty()) {
            updateTranscript(lSeq, QString(), false);
            continue;
        }

        const QString strDir =
            QStandardPaths::writableLocation(QStandardPaths::CacheLocation)
            + QStringLiteral("/voice");
        QDir().mkpath(strDir);
        const QString strWav = QStringLiteral("%1/seq_%2.wav").arg(strDir).arg(lSeq);

        QFile stFile(strWav);
        if (!stFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            updateTranscript(lSeq, QString(), false);
            continue;
        }
        stFile.write(CVoiceCodec::toWav(byPcm));
        stFile.close();

        m_uTranscribingSeq = lSeq;
        m_pVoice->transcribeFile(strWav);
        /* 一次只跑一条，textReady 里再取下一条 */
        return;
    }
}

/*
 * 功能描述:  更新转写文本。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   nOk              输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::updateTranscript(qint64 lSeq, const QString& strText, bool nOk) {
    for (ChatItem_S& stItem : model_) {
        if (stItem.seq != lSeq) continue;
        if (nOk && !strText.isEmpty()) {
            stItem.transcript = strText;
            stItem.transcriptState = 2;
            /* 转写出来的是英文，接着走翻译 */
            if (m_pTranslation != nullptr && m_pTranslation->isEnabled()
                && CTranslationService::looksEnglish(strText)) {
                stItem.translationState = 1;
                m_pTranslation->requestMessage(lSeq, strText);
            }
        } else {
            stItem.transcriptState = 3;
        }
        break;
    }

    if (CMessageBubble* bubble = m_uBubbles.value(lSeq, nullptr)) {
        if (nOk && !strText.isEmpty()) {
            bubble->setTranscript(strText);
            if (m_pTranslation != nullptr && m_pTranslation->isEnabled()
                && CTranslationService::looksEnglish(strText)) {
                bubble->setTranslationPending();
            }
        } else {
            bubble->setTranscript(QStringLiteral("（未能识别语音内容）"));
        }
    }
}

/*
 * 功能描述:  追加语音消息。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   lFrom            输入数据，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   byAudio          Opus 编码的音频数据，类型 const QByteArray&。
 * param [in]   nDurationMs      语音时长，单位毫秒，类型 int。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::appendVoice(qint64 lSeq, qint64 lFrom, const QString& strName,
                           const QByteArray& byAudio, int nDurationMs, qint64 lTs) {
    if (byAudio.isEmpty()) return;
    if (lTs <= 0) lTs = QDateTime::currentMSecsSinceEpoch();

    if (lSeq > 0) {
        if (m_uSeqs.contains(lSeq)) return;
        m_uSeqs.insert(lSeq);
    }

    if (lFrom != 0 && !m_uMembers.contains(lFrom)) {
        m_uMembers.insert(lFrom, strName);
        rebuildMembers();
    }

    ChatItem_S stItem;
    stItem.seq = lSeq;
    stItem.from = lFrom;
    stItem.ts = lTs;
    stItem.name = strName;
    stItem.kind = MessageKind_EN::Voice;
    stItem.audio = byAudio;
    stItem.durationMs = nDurationMs;
    stItem.system = false;

    appendItem(stItem);
    scrollToBottom();

    /* 语音也要能看懂：本地 whisper 转写成英文，再翻译成中文 */
    requestVoiceTranscript(lSeq);

    const bool nIncoming = (lFrom != 0) && !m_uSelfIds.contains(lFrom);
    if (nIncoming && m_pReply != nullptr && m_pReply->isEnabled()
        && m_pComposer->toPlainText().trimmed().isEmpty()) {
        m_pReplyDebounce->start();
    }
}

/*
 * 功能描述:  执行insert 语音消息文本。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::insertVoiceText(const QString& strText) {
    if (strText.isEmpty()) return;

    QString strPayload = strText;
    const QString strExisting = m_pComposer->toPlainText();
    if (!strExisting.isEmpty()
        && !strExisting.endsWith(QLatin1Char(' '))
        && !strExisting.endsWith(QLatin1Char('\n'))) {
        strPayload.prepend(QLatin1Char(' '));
    }

    QTextCursor stCursor = m_pComposer->textCursor();
    stCursor.insertText(strPayload);
    m_pComposer->setTextCursor(stCursor);
    m_pComposer->setFocus();
}

namespace {
/*
 * 功能描述:  转换为客户端配置。
 * 作者:      JaTin
 * param [in]   stSource         配置来源路径，类型 const AppConfig::LlmSettings_S&。
 * return:    返回 CLlmClient::Config_S 类型的处理结果，具体含义见功能描述。
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
 * 功能描述:  应用配置。
 * 作者:      JaTin
 * param [in]   stSettings       配置集合，类型 const AppConfig::Settings_S&。
 * param [in]   nInitial         输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::applyConfig(const AppConfig::Settings_S& stSettings, bool nInitial) {
    /* 对话/建议用主模型 */
    m_pLlm->setConfig(toClientConfig(stSettings.llm));

    /* 翻译：配了专用模型就用它，否则沿用主模型参数 */
    const bool nSeparate = stSettings.translation.hasOwnLlm;
    m_pTranslateLlm->setConfig(toClientConfig(nSeparate ? stSettings.translation.llm : stSettings.llm));

    m_nHistoryTranslationBudget = qMax(0, stSettings.translation.maxHistoryMessages);
    m_strConfigError = stSettings.error;

    if (!stSettings.error.isEmpty()) {
        if (nInitial) {
            /* 构造期发出会丢（CMainWindow 尚未 connect），推到事件循环第一轮 */
            const QString strMessage = stSettings.error;
            QTimer::singleShot(0, this, [this, strMessage] { emit notice(strMessage, true); });
        } else {
            emit notice(stSettings.error, true);
        }
    } else if (!nInitial) {
        const QString strTranslateModel = nSeparate ? stSettings.translation.llm.model
                                                : stSettings.llm.model;
        emit notice(QStringLiteral("配置已重新加载：对话 %1 / 翻译 %2")
                        .arg(stSettings.llm.model, strTranslateModel),
                    false);
    }

    /* 之前因为没配 Key 而失败的条目，填好之后自动补翻 */
    if (m_pTranslateLlm->isEnabled()) retryFailedTranslations();
}

/*
 * 功能描述:  重试failedtranslations。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::retryFailedTranslations() {
    if (m_pTranslation == nullptr || !m_pTranslation->isEnabled()) return;

    QVector<QPair<qint64, QString>> lRetry;
    int nBudget = m_nHistoryTranslationBudget;
    for (int i = model_.size() - 1; i >= 0 && nBudget > 0; --i) {
        ChatItem_S& stItem = model_[i];
        if (stItem.system) continue;
        /* 3 = 之前失败；0 = 当时翻译没启用，配置改好后一起补 */
        if (stItem.translationState != 3 && stItem.translationState != 0) continue;
        if (stItem.from == 0 || m_uSelfIds.contains(stItem.from)) continue;
        if (!CTranslationService::looksEnglish(stItem.text)) continue;
        stItem.translationState = 1;
        if (CMessageBubble* bubble = m_uBubbles.value(stItem.seq, nullptr)) {
            bubble->setTranslationPending();
        }
        lRetry.prepend(qMakePair(stItem.seq, stItem.text));
        --nBudget;
    }
    for (const auto& stPair : lRetry) {
        m_pTranslation->requestMessage(stPair.first, stPair.second);
    }
}

/*
 * 功能描述:  显示语音消息状态。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   nError           错误信息，类型 bool。
 * param [in]   nAutoHideMs      输入数据，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::showVoiceStatus(const QString& strText, bool nError, int nAutoHideMs) {
    if (m_pVoiceStatus == nullptr) return;

    ++m_nVoiceStatusToken;
    const int nToken = m_nVoiceStatusToken;

    /* 传空串表示清掉状态条 */
    if (strText.isEmpty()) {
        m_pVoiceStatus->clear();
        m_pVoiceStatus->setVisible(false);
        return;
    }

    m_pVoiceStatus->setText(strText);
    m_pVoiceStatus->setStyleSheet(
        nError ? QStringLiteral("color:#E5484D; background:#FDF2F2; padding:6px 18px; font-size:9pt;")
              : QStringLiteral("color:#5B6675; background:#F2F5F9; padding:6px 18px; font-size:9pt;"));
    m_pVoiceStatus->setVisible(true);

    if (nAutoHideMs > 0) {
        QTimer::singleShot(nAutoHideMs, this, [this, nToken] {
            if (nToken == m_nVoiceStatusToken) m_pVoiceStatus->setVisible(false);
        });
    }
}

/* ---------------------------------------------------------------- 成员 */

/*
 * 功能描述:  更新头部。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::updateHeader() {
    m_pRoomSubtitle->setText(QStringLiteral("%1 位成员").arg(m_uMembers.size()));
    m_pMemberBtn->setText(QStringLiteral("成员 %1").arg(m_uMembers.size()));
    if (m_pMemberTitle != nullptr) {
        m_pMemberTitle->setText(QStringLiteral("成员（%1）").arg(m_uMembers.size()));
    }
}

/*
 * 功能描述:  执行rebuild 成员列表；实现上创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::rebuildMembers() {
    while (m_pMemberListLayout->count() > 1) {
        QLayoutItem* stItem = m_pMemberListLayout->takeAt(0);
        if (stItem->widget() != nullptr) stItem->widget()->deleteLater();
        delete stItem;
    }

    for (auto it = m_uMembers.constBegin(); it != m_uMembers.constEnd(); ++it) {
        auto* pRow = new QWidget();
        pRow->setObjectName("MemberRow");
        auto* pRowLayout = new QHBoxLayout(pRow);
        pRowLayout->setContentsMargins(6, 4, 6, 4);
        pRowLayout->setSpacing(8);

        auto* pAvatar = new CAvatar(pRow);
        pAvatar->setFixedSize(28, 28);
        pAvatar->setFontPx(12);
        pAvatar->setText(it.value());
        pAvatar->setColor(CAvatar::colorForId(it.key()));

        auto* pNameLabel = new QLabel(it.value(), pRow);
        pNameLabel->setStyleSheet(QStringLiteral("font-size: 9.5pt;"));
        if (m_uSelfIds.contains(it.key())) {
            pNameLabel->setText(it.value() + QStringLiteral("（我）"));
        }

        pRowLayout->addWidget(pAvatar);
        pRowLayout->addWidget(pNameLabel, 1);

        m_pMemberListLayout->insertWidget(m_pMemberListLayout->count() - 1, pRow);
    }

    updateHeader();
}
