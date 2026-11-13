// ChatPage.h
//
// Telegram 风格聊天界面：顶栏（返回 / 房间信息 / 成员）、消息区、输入区、成员侧栏。
//
// 消息采用「模型 + 视图」结构：model_ 保存全部已渲染条目，历史消息到达时
// 合并去重后整体重画（rebuildView），这样日期分隔、系统提示都能保持一致顺序。
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

class Avatar;
class LlmClient;
class MessageBubble;
class MicButton;
class QFrame;
class QLabel;
class QPushButton;
class QFileSystemWatcher;
class QScrollArea;
class QVBoxLayout;
class ReplyService;
class SendButton;
class SuggestionBar;
class TranslationCard;
class TranslationService;
class TtsService;
class VoiceInput;

/// 输入框：Enter 发送，Shift+Enter 换行。
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

    /// 首次进入房间：清空消息区。
    void enterRoom(const QString& room, bool created, const QVector<MemberInfo>& members,
                   qint64 selfId, const QString& selfName);

    /// 断线重连后回到同一房间：保留已有消息，只刷新成员与连接状态。
    void resumeRoom(const QString& room, const QVector<MemberInfo>& members,
                    qint64 selfId, const QString& selfName);

    void appendChat(qint64 seq, qint64 from, const QString& name,
                    const QString& text, qint64 ts);
    /// 收到语音消息（含自己发的回显）。
    void appendVoice(qint64 seq, qint64 from, const QString& name,
                     const QByteArray& audio, int durationMs, qint64 ts);
    void mergeHistory(const QVector<ChatMessage>& messages);
    void appendSystemText(const QString& text, qint64 ts);
    void onSystemEvent(const QString& event, qint64 userId, const QString& name,
                       const QString& oldName, const QString& text, qint64 ts);
    void setOnline(bool online);
    void clearRoom();

    QString room() const { return room_; }

signals:
    void backRequested();
    void sendRequested(const QString& text);
    /// 录音结束，请求把这段语音发出去。
    void sendVoiceRequested(const QByteArray& audio, int durationMs);
    void notice(const QString& text, bool error);

private:
    struct ChatItem {
        qint64  seq = 0;
        qint64  from = 0;
        qint64  ts = 0;
        QString name;
        QString text;
        bool    system = false;

        MessageKind kind = MessageKind::Text;
        QByteArray  audio;          // 语音消息的 DPV1 容器
        int         durationMs = 0;

        QString translation;        // 他人英文消息的中文译文
        int     translationState = 0;  // 0 无 / 1 翻译中 / 2 完成 / 3 失败

        QString transcript;         // 语音消息的本地转写（英文）
        int     transcriptState = 0;   // 0 无 / 1 转写中 / 2 完成 / 3 失败
    };

    void appendItem(const ChatItem& item);
    void renderItem(const ChatItem& item);
    void rebuildView();
    void clearView();
    void sortModel();
    void appendDateSeparatorIfNeeded(qint64 ts);
    void scrollToBottom();
    void forceScrollToBottom();
    void sendCurrentText();
    void toggleRecording();
    /// 录音结束：编码成 Opus 并发出去。
    void onVoiceRecorded(const QByteArray& pcm, int durationMs);
    /// 对一条语音做本地转写（用于显示原文并翻译）。
    void requestVoiceTranscript(qint64 seq);
    /// 从队列里取下一条待转写语音。
    void pumpTranscript();
    /// 转写结果回填（ok=false 表示失败）。
    void updateTranscript(qint64 seq, const QString& text, bool ok);
    void onInputDebounced();
    void requestSuggestions();
    void applyConfig(const AppConfig::Settings& settings, bool initial);
    void retryFailedTranslations();
    void insertVoiceText(const QString& text);
    void showVoiceStatus(const QString& text, bool error, int autoHideMs);
    void rebuildMembers();
    void updateHeader();

    QString room_;
    qint64  selfId_ = 0;
    QString selfName_;
    QMap<qint64, QString> members_;
    QVector<ChatItem>     model_;
    QSet<qint64>          seqs_;      // 已渲染过的消息序号，用于去重
    QSet<qint64>          selfIds_;   // 本人历次连接的 id（重连后 id 会变）
    QDate   lastDate_;
    bool    pendingScroll_ = false;

    Avatar*       roomAvatar_ = nullptr;
    QLabel*       roomTitle_ = nullptr;
    QLabel*       roomSubtitle_ = nullptr;
    QPushButton*  memberBtn_ = nullptr;

    QScrollArea*  scroll_ = nullptr;
    QWidget*      canvas_ = nullptr;
    QVBoxLayout*  chatLayout_ = nullptr;

    ComposerEdit* composer_ = nullptr;
    SendButton*   sendBtn_ = nullptr;
    MicButton*    micBtn_ = nullptr;
    QLabel*       voiceStatus_ = nullptr;
    VoiceInput*   voice_ = nullptr;
    TtsService*   tts_ = nullptr;
    TranslationCard* translationCard_ = nullptr;
    QTimer*       inputDebounce_ = nullptr;
    ReplyService* reply_ = nullptr;
    SuggestionBar* suggestionBar_ = nullptr;
    QTimer*       replyDebounce_ = nullptr;
    LlmClient*    llm_ = nullptr;            // 对话 / 智能建议
    LlmClient*    translateLlm_ = nullptr;   // 翻译（可独立配置，未配则与 llm_ 同参）
    TranslationService* translation_ = nullptr;
    QHash<qint64, MessageBubble*> bubbles_;   // seq -> 气泡，用于异步回填译文
    QPointer<MessageBubble> playingBubble_;   // 正在播放的气泡（用于切换播放/停止图标）
    QFileSystemWatcher* configWatcher_ = nullptr;
    QTimer*             configReloadTimer_ = nullptr;
    int                 historyTranslationBudget_ = 12;
    QString             configError_;
    QQueue<qint64>      transcriptQueue_;    // 待转写的语音 seq
    qint64              transcribingSeq_ = 0; // 正在转写的那条（0 = 空闲）
    int           voiceStatusToken_ = 0;
    bool          voiceAvailable_ = true;

    QFrame*       memberPanel_ = nullptr;
    QVBoxLayout*  memberListLayout_ = nullptr;
    QLabel*       memberTitle_ = nullptr;
};
