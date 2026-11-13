/*
 * 文件名:    ChatPage.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  ChatPage 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* ChatPage.h */

/* Telegram 风格聊天界面：顶栏（返回 / 房间信息 / 成员）、消息区、输入区、成员侧栏。 */

/* 消息采用「模型 + 视图」结构：model_ 保存全部已渲染条目，历史消息到达时 */
/* 合并去重后整体重画（rebuildView），这样日期分隔、系统提示都能保持一致顺序。 */
#pragma once

#include <QDate>
#include <QHash>
#include <QPointer>
#include <QQueue>
#include <QMap>
#include <QPlainTextEdit>
#include <QSet>
#include <QVector>
#include <QWidget>

#include "AppConfig.h"
#include "MessageBubble.h"
#include "ProtocolBridge.h"

class CAvatar;
class CLlmClient;
class CMessageBubble;
class CMicButton;
class QFrame;
class QLabel;
class QPushButton;
class QFileSystemWatcher;
class QScrollArea;
class QVBoxLayout;
class CReplyService;
class CSendButton;
class CSuggestionBar;
class CTranslationCard;
class CTranslationService;
class CTtsService;
class CVoiceInput;

/* 输入框：Enter 发送，Shift+Enter 换行。 */
class ComposerEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    explicit ComposerEdit(QWidget* parent = nullptr);

signals:
    void submit();

protected:
    void keyPressEvent(QKeyEvent* event) override;
};

class ChatPage : public QWidget {
    Q_OBJECT

public:
    explicit ChatPage(QWidget* parent = nullptr);

    /* 首次进入房间：清空消息区。 */
    void enterRoom(const QString& room, bool created, const QVector<MemberInfo_S>& members,
                   qint64 selfId, const QString& selfName);

    /* 断线重连后回到同一房间：保留已有消息，只刷新成员与连接状态。 */
    void resumeRoom(const QString& room, const QVector<MemberInfo_S>& members,
                    qint64 selfId, const QString& selfName);

    void appendChat(qint64 seq, qint64 from, const QString& name,
                    const QString& text, qint64 ts);
    /* 收到语音消息（含自己发的回显）。 */
    void appendVoice(qint64 seq, qint64 from, const QString& name,
                     const QByteArray& audio, int durationMs, qint64 ts);
    void mergeHistory(const QVector<ChatMessage_S>& messages);
    void appendSystemText(const QString& text, qint64 ts);
    void onSystemEvent(const QString& event, qint64 userId, const QString& name,
                       const QString& oldName, const QString& text, qint64 ts);
    void setOnline(bool online);
    void clearRoom();

    QString room() const { return m_strRoom; }

signals:
    void backRequested();
    void sendRequested(const QString& text);
    /* 录音结束，请求把这段语音发出去。 */
    void sendVoiceRequested(const QByteArray& audio, int durationMs);
    void notice(const QString& text, bool error);

private:
    struct ChatItem_S {
        qint64  seq = 0;
        qint64  from = 0;
        qint64  ts = 0;
        QString name;
        QString text;
        bool    system = false;

        MessageKind_EN kind = MessageKind_EN::Text;
        /* 语音消息的 DPV1 容器 */
        QByteArray  audio;
        int         durationMs = 0;

        /* 他人英文消息的中文译文 */
        QString translation;
        /* 0 无 / 1 翻译中 / 2 完成 / 3 失败 */
        int     translationState = 0;

        /* 语音消息的本地转写（英文） */
        QString transcript;
        /* 0 无 / 1 转写中 / 2 完成 / 3 失败 */
        int     transcriptState = 0;
    };

    void appendItem(const ChatItem_S& item);
    void renderItem(const ChatItem_S& item);
    void rebuildView();
    void clearView();
    void sortModel();
    void appendDateSeparatorIfNeeded(qint64 ts);
    void scrollToBottom();
    void forceScrollToBottom();
    void sendCurrentText();
    void toggleRecording();
    /* 录音结束：编码成 Opus 并发出去。 */
    void onVoiceRecorded(const QByteArray& pcm, int durationMs);
    /* 对一条语音做本地转写（用于显示原文并翻译）。 */
    void requestVoiceTranscript(qint64 seq);
    /* 从队列里取下一条待转写语音。 */
    void pumpTranscript();
    /* 转写结果回填（ok=false 表示失败）。 */
    void updateTranscript(qint64 seq, const QString& text, bool ok);
    void onInputDebounced();
    void requestSuggestions();
    void applyConfig(const AppConfig::Settings_S& settings, bool initial);
    void retryFailedTranslations();
    void insertVoiceText(const QString& text);
    void showVoiceStatus(const QString& text, bool error, int autoHideMs);
    void rebuildMembers();
    void updateHeader();

    QString m_strRoom;
    qint64  m_uSelfId = 0;
    QString m_strSelfName;
    QMap<qint64, QString> m_uMembers;
    QVector<ChatItem_S>     model_;
    /* 已渲染过的消息序号，用于去重 */
    QSet<qint64>          m_uSeqs;
    /* 本人历次连接的 id（重连后 id 会变） */
    QSet<qint64>          m_uSelfIds;
    QDate   m_stLastDate;
    bool    m_nPendingScroll = false;

    CAvatar*       m_pRoomAvatar = nullptr;
    QLabel*       m_pRoomTitle = nullptr;
    QLabel*       m_pRoomSubtitle = nullptr;
    QPushButton*  m_pMemberBtn = nullptr;

    QScrollArea*  m_pScroll = nullptr;
    QWidget*      m_pCanvas = nullptr;
    QVBoxLayout*  m_pChatLayout = nullptr;

    ComposerEdit* m_pComposer = nullptr;
    CSendButton*   m_pSendBtn = nullptr;
    CMicButton*    m_pMicBtn = nullptr;
    QLabel*       m_pVoiceStatus = nullptr;
    CVoiceInput*   m_pVoice = nullptr;
    CTtsService*   m_pTts = nullptr;
    CTranslationCard* m_pTranslationCard = nullptr;
    QTimer*       m_pInputDebounce = nullptr;
    CReplyService* m_pReply = nullptr;
    CSuggestionBar* m_pSuggestionBar = nullptr;
    QTimer*       m_pReplyDebounce = nullptr;
    /* 对话 / 智能建议 */
    CLlmClient*    m_pLlm = nullptr;
    /* 翻译（可独立配置，未配则与 llm_ 同参） */
    CLlmClient*    m_pTranslateLlm = nullptr;
    CTranslationService* m_pTranslation = nullptr;
    /* seq -> 气泡，用于异步回填译文 */
    QHash<qint64, CMessageBubble*> m_uBubbles;
    /* 正在播放的气泡（用于切换播放/停止图标） */
    QPointer<CMessageBubble> m_stPlayingBubble;
    QFileSystemWatcher* m_pConfigWatcher = nullptr;
    QTimer*             m_pConfigReloadTimer = nullptr;
    int                 m_nHistoryTranslationBudget = 12;
    QString             m_strConfigError;
    /* 待转写的语音 seq */
    QQueue<qint64>      m_uTranscriptQueue;
    /* 正在转写的那条（0 = 空闲） */
    qint64              m_uTranscribingSeq = 0;
    int           m_nVoiceStatusToken = 0;
    bool          m_nVoiceAvailable = true;

    QFrame*       m_pMemberPanel = nullptr;
    QVBoxLayout*  m_pMemberListLayout = nullptr;
    QLabel*       m_pMemberTitle = nullptr;
};
