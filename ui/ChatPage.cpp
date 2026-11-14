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
 * 功能描述:  获取formattime。
 * 作者:      JaTin
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString formatTime(qint64 lTs) {
    return QDateTime::fromMSecsSinceEpoch(lTs).toString(QStringLiteral("HH:mm"));
}

/* 把日期格式化成「今天 / 昨天 / M月d日」。 */
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

/* 模型路径优先级：QSettings 覆盖 > 编译期默认（源码树 models/ 目录）。 */
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

/* ---------------------------------------------------------------- 构造 */

/*
 * 功能描述:  构造 ChatPage 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QWidget* parent) : QWidget(。
 * return:    无返回值（构造函数或析构函数）。
 */
ChatPage::ChatPage(QWidget* pParent) : QWidget(pParent) {
    setObjectName("AppRoot");

    model_ = new business::ChatModel(this);
    m_pController = new business::ChatController(model_, this);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pRoot = new QHBoxLayout(this);
    pRoot->setContentsMargins(0, 0, 0, 0);
    pRoot->setSpacing(0);

    /* ---- 左侧：聊天列 ---- */
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pChatColumn = new QWidget(this);
    auto* pColumnLayout = new QVBoxLayout(pChatColumn);
    pColumnLayout->setContentsMargins(0, 0, 0, 0);
    pColumnLayout->setSpacing(0);

    buildHeader(pColumnLayout);
    buildMessageArea(pColumnLayout);
    buildComposer(pColumnLayout);

    pRoot->addWidget(pChatColumn, 1);

    buildMemberPanel();
    pRoot->addWidget(m_pMemberPanel);

    wireSignals();

    /* 模型在后台线程加载，不阻塞界面 */
    m_pVoice->start();
}

ChatPage::~ChatPage() = default;

/*
 * 功能描述:  构建头部；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   pColumnLayout    输入数据，类型 QVBoxLayout*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::buildHeader(QVBoxLayout* pColumnLayout) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pHeader = new QFrame(this);
    pHeader->setObjectName("ChatHeader");
    pHeader->setFixedHeight(64);

    auto* pHeaderLayout = new QHBoxLayout(pHeader);
    pHeaderLayout->setContentsMargins(12, 0, 16, 0);
    pHeaderLayout->setSpacing(10);

    auto* pBackBtn = new QPushButton(QStringLiteral("←"), pHeader);
    pBackBtn->setObjectName("LinkButton");
    pBackBtn->setCursor(Qt::PointingHandCursor);
    pBackBtn->setFixedSize(core::ui::kTouchTarget, core::ui::kTouchTarget);
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

    pColumnLayout->addWidget(pHeader);

    connect(pBackBtn, &QPushButton::clicked, this, &ChatPage::backRequested);
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
 * 功能描述:  构建消息内容 area；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   pColumnLayout    输入数据，类型 QVBoxLayout*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::buildMessageArea(QVBoxLayout* pColumnLayout) {
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
    pColumnLayout->addWidget(m_pScroll, 1);

    QScrollBar* pBar = m_pScroll->verticalScrollBar();

    /* 内容或视口高度变化时决定要不要跟着贴底。 */
    
    /* 关键场景：建议条/译文卡弹出会把消息区变矮，maximum 随之增大。 */
    /* 此时若用户本来就在底部，必须继续贴底，否则位置不变就会「停在中间」—— */
    /* 表现为新到的消息明明渲染了却看不见，必须手动滚一下。 */
    connect(pBar, &QScrollBar::rangeChanged, this, [this](int, int max) {
        if (m_nPendingScroll || m_nStickToBottom) {
            m_pScroll->verticalScrollBar()->setValue(max);
        }
    });

    /* 用户手动滚动时记录是否停在底部（留 4px 容差，避免浮点/整型误差） */
    connect(pBar, &QScrollBar::valueChanged, this, [this](int value) {
        m_nStickToBottom = (value >= m_pScroll->verticalScrollBar()->maximum() - 4);
    });
}

/*
 * 功能描述:  构建composer；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   pColumnLayout    输入数据，类型 QVBoxLayout*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::buildComposer(QVBoxLayout* pColumnLayout) {
    m_pSuggestionBar = new CSuggestionBar(this);
    pColumnLayout->addWidget(m_pSuggestionBar);

    m_pTranslationCard = new CTranslationCard(this);
    pColumnLayout->addWidget(m_pTranslationCard);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pVoiceStatus = new QLabel(this);
    m_pVoiceStatus->setObjectName("VoiceStatus");
    m_pVoiceStatus->setVisible(false);
    pColumnLayout->addWidget(m_pVoiceStatus);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pComposerFrame = new QFrame(this);
    pComposerFrame->setObjectName("Composer");
    auto* pComposerLayout = new QHBoxLayout(pComposerFrame);
    pComposerLayout->setContentsMargins(16, 10, 16, 10);
    pComposerLayout->setSpacing(10);

    m_pMicBtn = new CMicButton(pComposerFrame);
    pComposerLayout->addWidget(m_pMicBtn, 0, Qt::AlignVCenter);

    m_pComposer = new ComposerEdit(pComposerFrame);
    m_pComposer->setObjectName("ComposerEdit");
    updateComposerPlaceholder();
    m_pComposer->setFixedHeight(46);
    pComposerLayout->addWidget(m_pComposer, 1);

    m_pSendBtn = new CSendButton(pComposerFrame);
    pComposerLayout->addWidget(m_pSendBtn, 0, Qt::AlignVCenter);

    pColumnLayout->addWidget(pComposerFrame);

    connect(m_pSendBtn, &CSendButton::clicked, this, &ChatPage::sendCurrentText);
    connect(m_pComposer, &ComposerEdit::submit, this, &ChatPage::sendCurrentText);
}

/*
 * 功能描述:  构建成员 panel；实现上创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::buildMemberPanel() {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberPanel = new QFrame(this);
    m_pMemberPanel->setObjectName("MemberPanel");
    m_pMemberPanel->setFixedWidth(220);
    m_pMemberPanel->setVisible(false);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    auto* pPanelLayout = new QVBoxLayout(m_pMemberPanel);
    pPanelLayout->setContentsMargins(12, 14, 12, 14);
    pPanelLayout->setSpacing(8);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pMemberTitle = new QLabel(m_pMemberPanel);
    m_pMemberTitle->setStyleSheet(QStringLiteral("font-size: 11pt; font-weight: 700;"));
    pPanelLayout->addWidget(m_pMemberTitle);

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

    pPanelLayout->addWidget(pMemberScroll, 1);
}

/* ---------------------------------------------------------------- 接线 */

/*
 * 功能描述:  执行wiresignals；实现上连接 Qt 信号与槽。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::wireSignals() {
    /* ---- 模型 -> 视图 ---- */
    connect(model_, &business::ChatModel::itemAppended, this, [this](const business::ChatItem_S& stItem) {
        renderItem(stItem);
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
        const QByteArray byCurrent = CVoicePlayer::instance()->current();
        for (auto it = m_uBubbles.constBegin(); it != m_uBubbles.constEnd(); ++it) {
            const business::ChatItem_S* stItem = model_->find(it.key());
            if (stItem == nullptr || stItem->kind != MessageKind_EN::Voice) continue;
            it.value()->setPlaying(playing && !byCurrent.isEmpty() && stItem->audio == byCurrent);
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
            [this](const QString& strText) { m_pTts->speak(strText); });
    connect(m_pTranslationCard, &CTranslationCard::useRequested, this, [this](const QString& strText) {
        m_pComposer->setPlainText(strText);
        m_pComposer->moveCursor(QTextCursor::End);
        m_pComposer->setFocus();
    });
    connect(m_pSuggestionBar, &CSuggestionBar::useRequested, this, [this](const QString& strText) {
        m_pComposer->setPlainText(strText);
        m_pComposer->moveCursor(QTextCursor::End);
        m_pComposer->setFocus();
        m_pSuggestionBar->hideBar();
    });
    connect(m_pSuggestionBar, &CSuggestionBar::playRequested, this,
            [this](const QString& strText) { m_pTts->speak(strText); });

    /* ---- 输入框 ---- */
    connect(m_pComposer, &QPlainTextEdit::textChanged, this, [this] {
        syncComposerState();

        const QString strText = m_pComposer->toPlainText().trimmed();
        if (strText.isEmpty()) {
            m_pTranslationCard->hideCard();
            return;
        }
        /* 开始打字就把建议收起来，避免抢位置 */
        m_pSuggestionBar->hideBar();

        if (CTranslationService::looksChinese(strText)) {
            m_pTranslationCard->showPending();
            m_pController->requestInputTranslation(strText);
        } else {
            m_pTranslationCard->hideCard();
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
    model_->enterRoom(strRoom, lSelfId, strSelfName, aMembers);

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(strRoom));
    updateHeader();

    appendSystemText(nCreated ? QStringLiteral("你创建了房间 %1").arg(strRoom)
                             : QStringLiteral("你加入了房间 %1").arg(strRoom),
                     QDateTime::currentMSecsSinceEpoch());

    m_pComposer->clear();
    m_pTranslationCard->hideCard();
    m_pSuggestionBar->hideBar();
    m_pComposer->setFocus();
    setOnline(true);
    syncComposerState();
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
    model_->resumeRoom(strRoom, lSelfId, strSelfName, aMembers);

    m_pRoomTitle->setText(QStringLiteral("房间 %1").arg(strRoom));
    appendSystemText(QStringLiteral("已重新连接"), QDateTime::currentMSecsSinceEpoch());

    updateHeader();
    setOnline(true);
    m_pComposer->setFocus();
}

/*
 * 功能描述:  清空房间。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
 * 功能描述:  应用remote 配置。
 * 作者:      JaTin
 * param [in]   byJson           JSON 对象，类型 const QByteArray&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::applyRemoteConfig(const QByteArray& byJson) {
    if (m_pController != nullptr) m_pController->applyRemoteConfig(byJson);
}

/*
 * 功能描述:  获取房间。
 * 作者:      JaTin
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString ChatPage::room() const {
    return model_ != nullptr ? model_->room() : QString();
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

void ChatPage::appendChat(qint64 lSeq, qint64 lFrom, const QString& strName, const QString& strText,
                          qint64 lTs) {
    model_->appendChat(lSeq, lFrom, strName, strText, lTs);
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
    model_->appendVoice(lSeq, lFrom, strName, byAudio, nDurationMs, lTs);
}

/*
 * 功能描述:  合并历史消息。
 * 作者:      JaTin
 * param [in]   aMessages        消息记录，类型 const QVector<ChatMessage_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::mergeHistory(const QVector<ChatMessage_S>& aMessages) {
    model_->mergeHistory(aMessages);
}

/*
 * 功能描述:  追加系统提示词文本。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * param [in]   lTs              Unix 毫秒时间戳，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::appendSystemText(const QString& strText, qint64 lTs) {
    model_->appendSystem(strText, lTs);
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
    model_->applySystemEvent(pEvent, lUserId, strName, strText, lTs);
}

/* ---------------------------------------------------------------- 渲染 */

/*
 * 功能描述:  绘制条目；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   stItem           条目，类型 const business::ChatItem_S&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::renderItem(const business::ChatItem_S& stItem) {
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
        pinHeight(pRow);
        return;
    }

    const bool nOutgoing = model_->isSelf(stItem.from);
    /* 气泡宽度跟着消息区走：窄屏按比例收缩，宽屏保持 520 上限 */
    const int nBubbleWidth = core::ui::bubbleMaxWidth(m_pCanvas->width());

    /* ---- 语音消息 ---- */
    if (stItem.kind == MessageKind_EN::Voice) {
        auto* pBubble = new CMessageBubble(stItem.name, QString(), formatTime(stItem.ts),
                                         nOutgoing, stItem.from, false, nBubbleWidth, m_pCanvas);
        pBubble->setVoice(stItem.audio, stItem.durationMs);
        connect(pBubble, &CMessageBubble::voicePlayRequested, this,
                [](const QByteArray& byAudio) { CVoicePlayer::instance()->play(byAudio); });

        if (stItem.seq > 0) m_uBubbles.insert(stItem.seq, pBubble);
        m_pChatLayout->addWidget(pBubble);
        pinHeight(pBubble);
        refreshBubble(stItem.seq);
        return;
    }

    /* ---- 文本消息 ---- */
    const bool nPlayable = !nOutgoing && CTranslationService::looksEnglish(stItem.text);
    auto* pBubble = new CMessageBubble(stItem.name, stItem.text, formatTime(stItem.ts),
                                     nOutgoing, stItem.from, nPlayable, nBubbleWidth, m_pCanvas);
    connect(pBubble, &CMessageBubble::playRequested, this, [this, pBubble](const QString& strText) {
        if (m_stPlayingBubble != nullptr && m_stPlayingBubble != pBubble) {
            m_stPlayingBubble->setPlaying(false);
        }
        m_stPlayingBubble = pBubble;
        m_pTts->speak(strText);
    });

    if (stItem.seq > 0) m_uBubbles.insert(stItem.seq, pBubble);
    m_pChatLayout->addWidget(pBubble);
    pinHeight(pBubble);
    refreshBubble(stItem.seq);
}

/*
 * 功能描述:  刷新消息气泡。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::refreshBubble(qint64 lSeq) {
    if (lSeq <= 0) return;

    CMessageBubble* pBubble = m_uBubbles.value(lSeq, nullptr);
    const business::ChatItem_S* stItem = model_->find(lSeq);
    if (pBubble == nullptr || stItem == nullptr) return;

    if (stItem->kind == MessageKind_EN::Voice) {
        switch (stItem->transcriptState) {
            case 1: pBubble->setTranscriptPending(); break;
            case 2: pBubble->setTranscript(stItem->transcript); break;
            case 3: pBubble->setTranscript(QStringLiteral("（未能识别语音内容）")); break;
            default: break;
        }
    }

    switch (stItem->translationState) {
        case 1: pBubble->setTranslationPending(); break;
        case 2: pBubble->setTranslation(stItem->translation); break;
        case 3: pBubble->setTranslationFailed(); break;
        default: break;
    }
}

/*
 * 功能描述:  执行rebuild 视图。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::rebuildView() {
    clearView();
    m_stLastDate = QDate();
    for (const business::ChatItem_S& stItem : model_->items()) {
        renderItem(stItem);
    }
    scrollToBottom();
}

/*
 * 功能描述:  清空视图。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::clearView() {
    m_uBubbles.clear();
    m_stPlayingBubble = nullptr;
    m_nStickToBottom = true;
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
    pinHeight(pRow);
}

/*
 * 功能描述:  转换为scrollbottom；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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

        const QMargins stMargins = m_pChatLayout->contentsMargins();
        int nContentHeight = stMargins.top() + stMargins.bottom();
        int nVisibleCount = 0;

        for (int i = 0; i < m_pChatLayout->count(); ++i) {
            QLayoutItem* pEntry = m_pChatLayout->itemAt(i);
            if (pEntry == nullptr) continue;

            if (QWidget* w = pEntry->widget()) {
                const int nH = (w->height() > 0) ? w->height() : w->sizeHint().height();
                nContentHeight += nH;
                ++nVisibleCount;
            }
        }
        if (nVisibleCount > 1) {
            nContentHeight += m_pChatLayout->spacing() * (nVisibleCount - 1);
        }

        if (nContentHeight > 0 && m_pCanvas->minimumHeight() != nContentHeight) {
            m_pCanvas->setMinimumHeight(nContentHeight);
        }
    }

    /* 布局是异步结算的：addWidget 之后要等一轮事件循环，viewport 高度才更新， */
    /* 语音条这类自绘控件更晚。所以分多个时间点各贴一次底， */
    /* 而不是只依赖单次 setValue（那样会停在中间）。 */
    for (int nDelayMs : {0, 60, 200}) {
        QTimer::singleShot(nDelayMs, this, [this] {
            if (!m_nPendingScroll) return;

            /* 优先让最后一个控件可见：QScrollArea 会自己完成必要的布局结算。 */
            /* 注意不要再叠加 setValue(maximum())——此时 maximum() 可能仍是旧值， */
            /* 会把刚滚下去的位置又拉回来，表现就是「停在中间看不到新消息」。 */
            if (m_pChatLayout != nullptr && m_pChatLayout->count() > 1) {
                QLayoutItem* pLast = m_pChatLayout->itemAt(m_pChatLayout->count() - 1);
                if (pLast != nullptr && pLast->widget() != nullptr) {
                    m_pScroll->ensureWidgetVisible(pLast->widget(), 0, 24);
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
 * 功能描述:  固定控件高度，避免内容变化时布局跳动。
 * 作者:      JaTin
 * param [in]   pWidget          控件，类型 QWidget*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::pinHeight(QWidget* pWidget) {
    if (pWidget == nullptr) return;
    /* 必须在 addWidget 之后调用：没有 parent 时 sizeHint 还没按可用宽度换行计算， */
    /* 拿到的高度会偏小，钉死之后内容就被裁掉。 */
    if (QLayout* layout = pWidget->layout()) layout->activate();
    const int nHeight = pWidget->sizeHint().height();
    if (nHeight > 0) {
        pWidget->setMinimumHeight(nHeight);
        pWidget->updateGeometry();
    }
}

/*
 * 功能描述:  转换为forcescrollbottom。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::forceScrollToBottom() {
    /* 必须先把布局激活：刚 addWidget 时 maximum() 还是旧值， */
    /* 直接 setValue 会停在中间（语音条这类高度延迟确定的控件尤其明显）。 */
    if (m_pChatLayout != nullptr) m_pChatLayout->activate();

    QScrollBar* pBar = m_pScroll->verticalScrollBar();
    pBar->setValue(pBar->maximum());
}

/* ---------------------------------------------------------------- 交互 */

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
    m_pVoice->startRecording(CVoiceRecorder::Purpose_EN::Send);
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
    m_pMicBtn->setRecording(false);
    m_pController->sendVoice(byPcm, nDurationMs);
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
    updateComposerPlaceholder();
}

/*
 * 功能描述:  同步composer 状态。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::syncComposerState() {
    if (m_pController == nullptr) return;
    m_pController->setComposerEmpty(m_pComposer->toPlainText().trimmed().isEmpty());
}

/* ---------------------------------------------------------------- 状态提示 */

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
        nError
            ? QStringLiteral("color:#E5484D; background:#FDF2F2; padding:6px 18px; font-size:9pt;")
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
    const int nCount = (model_ != nullptr) ? model_->memberCount() : 0;
    m_pRoomSubtitle->setText(QStringLiteral("%1 位成员").arg(nCount));
    m_pMemberBtn->setText(QStringLiteral("成员 %1").arg(nCount));
    if (m_pMemberTitle != nullptr) {
        m_pMemberTitle->setText(QStringLiteral("成员（%1）").arg(nCount));
    }
}

/*
 * 功能描述:  执行rebuild 成员列表。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::rebuildMembers() {
    if (model_ == nullptr || m_pMemberListLayout == nullptr) return;

    while (m_pMemberListLayout->count() > 1) {
        QLayoutItem* stItem = m_pMemberListLayout->takeAt(0);
        if (stItem->widget() != nullptr) stItem->widget()->deleteLater();
        delete stItem;
    }

    const QMap<qint64, QString>& aMembers = model_->members();
    for (auto it = aMembers.constBegin(); it != aMembers.constEnd(); ++it) {
        m_pMemberListLayout->insertWidget(m_pMemberListLayout->count() - 1,
                                        createMemberRow(it.key(), it.value()));
    }

    updateHeader();
}

/*
 * 功能描述:  创建成员 row；实现上创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * return:    成功返回对象指针；未找到或失败时返回 nullptr。
 */
QWidget* ChatPage::createMemberRow(qint64 lUserId, const QString& strName) {
    auto* pRow = new QWidget();
    pRow->setObjectName("MemberRow");

    auto* pRowLayout = new QHBoxLayout(pRow);
    pRowLayout->setContentsMargins(6, 6, 6, 6);
    pRowLayout->setSpacing(8);

    auto* pAvatar = new CAvatar(pRow);
    pAvatar->setFixedSize(28, 28);
    pAvatar->setFontPx(12);
    pAvatar->setText(strName);
    pAvatar->setColor(CAvatar::colorForId(lUserId));

    auto* pNameLabel = new QLabel(strName, pRow);
    pNameLabel->setStyleSheet(QStringLiteral("font-size: 10pt;"));
    if (model_->isSelf(lUserId)) {
        pNameLabel->setText(strName + QStringLiteral("（我）"));
    }

    pRowLayout->addWidget(pAvatar);
    pRowLayout->addWidget(pNameLabel, 1);
    return pRow;
}

/*
 * 功能描述:  显示成员对话框；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::showMemberDialog() {
    if (model_ == nullptr) return;

    QDialog stDialog(this);
    stDialog.setWindowTitle(QStringLiteral("成员（%1）").arg(model_->memberCount()));
    /* QDialog 是顶层窗口，全局 QSS 里的 QDialog 规则管不到它，必须实例上设， */
    /* 否则在无窗口管理器的环境下会渲染成一块纯黑。 */
    stDialog.setStyleSheet(QStringLiteral("QDialog { background: #FFFFFF; }"));
    stDialog.setMinimumWidth(qBound(260, width() - 48, 360));

    auto* pLayout = new QVBoxLayout(&stDialog);
    pLayout->setContentsMargins(12, 12, 12, 12);
    pLayout->setSpacing(10);

    auto* pScroll = new QScrollArea(&stDialog);
    pScroll->setWidgetResizable(true);
    pScroll->setFrameShape(QFrame::NoFrame);
    pScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* pCanvas = new QWidget();
    auto* pListLayout = new QVBoxLayout(pCanvas);
    pListLayout->setContentsMargins(0, 0, 0, 0);
    pListLayout->setSpacing(2);

    const QMap<qint64, QString>& aMembers = model_->members();
    for (auto it = aMembers.constBegin(); it != aMembers.constEnd(); ++it) {
        pListLayout->addWidget(createMemberRow(it.key(), it.value()));
    }
    pListLayout->addStretch(1);

    pScroll->setWidget(pCanvas);
    pLayout->addWidget(pScroll, 1);

    auto* pCloseBtn = new QPushButton(QStringLiteral("关闭"), &stDialog);
    pCloseBtn->setObjectName("PrimaryButton");
    pCloseBtn->setMinimumHeight(core::ui::kTouchTarget);
    pCloseBtn->setCursor(Qt::PointingHandCursor);
    connect(pCloseBtn, &QPushButton::clicked, &stDialog, &QDialog::accept);
    pLayout->addWidget(pCloseBtn);

    stDialog.exec();
}

/*
 * 功能描述:  重新计算尺寸事件。
 * 作者:      JaTin
 * param [in]   pEvent           事件，类型 QResizeEvent*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void ChatPage::resizeEvent(QResizeEvent* pEvent) {
    QWidget::resizeEvent(pEvent);
    updateComposerPlaceholder();

    /* 窄屏不再需要侧栏，宽屏恢复时也保持一致 */
    if (m_pMemberPanel != nullptr && core::ui::isCompact(width())) {
        m_pMemberPanel->setVisible(false);
    }
}

/*
 * 功能描述:  更新composerplaceholder。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
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
