/**
 * @file       ChatPage.h
 * @brief      ChatPage 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  ChatPage.h。
 */

/**
 * @brief  Telegram 风格聊天界面：顶栏（返回 / 房间信息 / 成员）、消息区、输入区、成员侧栏。
 */

/**
 * @brief  消息采用「模型 + 视图」结构：model_ 保存全部已渲染条目，历史消息到达时 合并去重后整体重画（rebuildView），这样日期分隔、系统提示都能保持一致顺序。
 */
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

/**
 * @brief   输入框：Enter 发送，Shift+Enter 换行。
 * @author  JaTin
 */
class ComposerEdit : public QPlainTextEdit {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit ComposerEdit(QWidget* parent = nullptr);

signals:
    /**
     * @brief      提交 submit。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit();

protected:
    /**
     * @brief      执行密钥 press 事件。
     * @param[in] event          事件；类型 QKeyEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void keyPressEvent(QKeyEvent* event) override;
};

/**
 * @brief   Chat页面。
 * @author  JaTin
 */
class ChatPage : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QWidget*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit ChatPage(QWidget* parent = nullptr);

    /**
     * @brief  首次进入房间：清空消息区。
     */
    void enterRoom(const QString& room, bool created, const QVector<MemberInfo_S>& members,
                   qint64 selfId, const QString& selfName);

    /**
     * @brief  断线重连后回到同一房间：保留已有消息，只刷新成员与连接状态。
     */
    void resumeRoom(const QString& room, const QVector<MemberInfo_S>& members,
                    qint64 selfId, const QString& selfName);

    void appendChat(qint64 seq, qint64 from, const QString& name,
                    const QString& text, qint64 ts);
    /**
     * @brief  收到语音消息（含自己发的回显）。
     */
    void appendVoice(qint64 seq, qint64 from, const QString& name,
                     const QByteArray& audio, int durationMs, qint64 ts);
    /**
     * @brief      合并历史消息。
     * @param[in] messages       消息记录；类型 const QVector<ChatMessage_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void mergeHistory(const QVector<ChatMessage_S>& messages);
    /**
     * @brief      追加系统提示词文本。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] ts             Unix 毫秒时间戳；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void appendSystemText(const QString& text, qint64 ts);
    void onSystemEvent(const QString& event, qint64 userId, const QString& name,
                       const QString& oldName, const QString& text, qint64 ts);
    /**
     * @brief      设置 online相关的数据并返回处理结果。
     * @param[in] online         输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setOnline(bool online);
    /**
     * @brief      清空房间。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearRoom();

    QString room() const { return m_strRoom; }

signals:
    /**
     * @brief      执行 backrequested相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void backRequested();
    /**
     * @brief      发送 requested。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendRequested(const QString& text);
    /**
     * @brief      录音结束，请求把这段语音发出去。
     * @param[in] audio          Opus 编码的音频数据；类型 const QByteArray&。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendVoiceRequested(const QByteArray& audio, int durationMs);
    /**
     * @brief      执行 notice相关的数据并返回处理结果。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] error          错误信息；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void notice(const QString& text, bool error);

private:
    struct ChatItem_S {
        /**
         * @brief  全局消息序号。 初值 = 0。
         * @note   类型 qint64；类/结构体成员变量。
         */
        qint64  seq = 0;
        /**
         * @brief  from。 初值 = 0。
         * @note   类型 qint64；类/结构体成员变量。
         */
        qint64  from = 0;
        /**
         * @brief  Unix 毫秒时间戳。 初值 = 0。
         * @note   类型 qint64；类/结构体成员变量。
         */
        qint64  ts = 0;
        /**
         * @brief  名称或昵称。
         * @note   类型 QString；类/结构体成员变量。
         */
        QString name;
        /**
         * @brief  消息文本。
         * @note   类型 QString；类/结构体成员变量。
         */
        QString text;
        /**
         * @brief  系统提示词。 初值 = false。
         * @note   类型 bool；类/结构体成员变量。
         */
        bool    system = false;

        /**
         * @brief  消息类型。 初值 = MessageKind_EN::Text。
         * @note   类型 MessageKind_EN；类/结构体成员变量。
         */
        MessageKind_EN kind = MessageKind_EN::Text;
        /**
         * @brief  语音消息的 DPV1 容器
         * @note   类型 QByteArray；类/结构体成员变量。
         */
        QByteArray  audio;
        /**
         * @brief  语音时长，单位毫秒。 初值 = 0。
         * @note   类型 int；类/结构体成员变量。
         */
        int         durationMs = 0;

        /**
         * @brief  他人英文消息的中文译文
         * @note   类型 QString；类/结构体成员变量。
         */
        QString translation;
        /**
         * @brief  0 无 / 1 翻译中 / 2 完成 / 3 失败
         * @note   类型 int；类/结构体成员变量。
         */
        int     translationState = 0;

        /**
         * @brief  语音消息的本地转写（英文）
         * @note   类型 QString；类/结构体成员变量。
         */
        QString transcript;
        /**
         * @brief  0 无 / 1 转写中 / 2 完成 / 3 失败
         * @note   类型 int；类/结构体成员变量。
         */
        int     transcriptState = 0;
    };

    /**
     * @brief      追加条目。
     * @param[in] item           条目；类型 const ChatItem_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void appendItem(const ChatItem_S& item);
    /**
     * @brief      绘制条目。
     * @param[in] item           条目；类型 const ChatItem_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void renderItem(const ChatItem_S& item);
    /**
     * @brief      执行 rebuild 视图。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void rebuildView();
    /**
     * @brief      清空视图。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearView();
    /**
     * @brief      执行 sort 模型名。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sortModel();
    /**
     * @brief      追加 dateseparatorifneeded。
     * @param[in] ts             Unix 毫秒时间戳；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void appendDateSeparatorIfNeeded(qint64 ts);
    /**
     * @brief      转换为 scrollbottom。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void scrollToBottom();
    /**
     * @brief      转换为 forcescrollbottom。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void forceScrollToBottom();
    /**
     * @brief      发送 current 文本。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendCurrentText();
    /**
     * @brief      切换语音录制状态：空闲时开始采集，录制中则停止并送出音频。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void toggleRecording();
    /**
     * @brief      录音结束：编码成 Opus 并发出去。
     * @param[in] pcm            输入参数；类型 const QByteArray&。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onVoiceRecorded(const QByteArray& pcm, int durationMs);
    /**
     * @brief      对一条语音做本地转写（用于显示原文并翻译）。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestVoiceTranscript(qint64 seq);
    /**
     * @brief      从队列里取下一条待转写语音。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void pumpTranscript();
    /**
     * @brief      转写结果回填（ok=false 表示失败）。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] ok             输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void updateTranscript(qint64 seq, const QString& text, bool ok);
    /**
     * @brief      执行 on 输入 debounced。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onInputDebounced();
    /**
     * @brief      请求智能建议列表。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestSuggestions();
    /**
     * @brief      应用配置。
     * @param[in] settings       配置集合；类型 const AppConfig::Settings_S&。
     * @param[in] initial        输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void applyConfig(const AppConfig::Settings_S& settings, bool initial);
    /**
     * @brief      重试 failedtranslations。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void retryFailedTranslations();
    /**
     * @brief      执行 insert 语音消息文本。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void insertVoiceText(const QString& text);
    /**
     * @brief      显示语音消息状态。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] error          错误信息；类型 bool。
     * @param[in] autoHideMs     输入参数；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showVoiceStatus(const QString& text, bool error, int autoHideMs);
    /**
     * @brief      执行 rebuild 成员列表。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void rebuildMembers();
    /**
     * @brief      更新头部。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void updateHeader();

    /**
     * @brief  房间号。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strRoom;
    /**
     * @brief  本人会话编号。 初值 = 0。
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64  m_uSelfId = 0;
    /**
     * @brief  本人昵称。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strSelfName;
    /**
     * @brief  初始成员列表。
     * @note   类型 QMap<qint64, QString>；类/结构体成员变量。
     */
    QMap<qint64, QString> m_uMembers;
    /**
     * @brief  模型名。
     * @note   类型 QVector<ChatItem_S>；类/结构体成员变量。
     */
    QVector<ChatItem_S>     model_;
    /**
     * @brief  已渲染过的消息序号，用于去重
     * @note   类型 QSet<qint64>；类/结构体成员变量。
     */
    QSet<qint64>          m_uSeqs;
    /**
     * @brief  本人历次连接的 id（重连后 id 会变）
     * @note   类型 QSet<qint64>；类/结构体成员变量。
     */
    QSet<qint64>          m_uSelfIds;
    /**
     * @brief  LastDate。
     * @note   类型 QDate；类/结构体成员变量。
     */
    QDate   m_stLastDate;
    /**
     * @brief  PendingScroll。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    m_nPendingScroll = false;

    /**
     * @brief  房间头像。 初值 = nullptr。
     * @note   类型 CAvatar*；类/结构体成员变量。
     */
    CAvatar*       m_pRoomAvatar = nullptr;
    /**
     * @brief  房间Title。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*       m_pRoomTitle = nullptr;
    /**
     * @brief  房间Subtitle。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*       m_pRoomSubtitle = nullptr;
    /**
     * @brief  成员按钮。 初值 = nullptr。
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton*  m_pMemberBtn = nullptr;

    /**
     * @brief  Scroll。 初值 = nullptr。
     * @note   类型 QScrollArea*；类/结构体成员变量。
     */
    QScrollArea*  m_pScroll = nullptr;
    /**
     * @brief  Canvas。 初值 = nullptr。
     * @note   类型 QWidget*；类/结构体成员变量。
     */
    QWidget*      m_pCanvas = nullptr;
    /**
     * @brief  Chat布局。 初值 = nullptr。
     * @note   类型 QVBoxLayout*；类/结构体成员变量。
     */
    QVBoxLayout*  m_pChatLayout = nullptr;

    /**
     * @brief  Composer。 初值 = nullptr。
     * @note   类型 ComposerEdit*；类/结构体成员变量。
     */
    ComposerEdit* m_pComposer = nullptr;
    /**
     * @brief  Send按钮。 初值 = nullptr。
     * @note   类型 CSendButton*；类/结构体成员变量。
     */
    CSendButton*   m_pSendBtn = nullptr;
    /**
     * @brief  Mic按钮。 初值 = nullptr。
     * @note   类型 CMicButton*；类/结构体成员变量。
     */
    CMicButton*    m_pMicBtn = nullptr;
    /**
     * @brief  语音消息状态。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*       m_pVoiceStatus = nullptr;
    /**
     * @brief  语音消息。 初值 = nullptr。
     * @note   类型 CVoiceInput*；类/结构体成员变量。
     */
    CVoiceInput*   m_pVoice = nullptr;
    /**
     * @brief  语音合成。 初值 = nullptr。
     * @note   类型 CTtsService*；类/结构体成员变量。
     */
    CTtsService*   m_pTts = nullptr;
    /**
     * @brief  译文卡片。 初值 = nullptr。
     * @note   类型 CTranslationCard*；类/结构体成员变量。
     */
    CTranslationCard* m_pTranslationCard = nullptr;
    /**
     * @brief  输入Debounce。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*       m_pInputDebounce = nullptr;
    /**
     * @brief  响应对象。 初值 = nullptr。
     * @note   类型 CReplyService*；类/结构体成员变量。
     */
    CReplyService* m_pReply = nullptr;
    /**
     * @brief  智能建议工具条。 初值 = nullptr。
     * @note   类型 CSuggestionBar*；类/结构体成员变量。
     */
    CSuggestionBar* m_pSuggestionBar = nullptr;
    /**
     * @brief  回复Debounce。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*       m_pReplyDebounce = nullptr;
    /**
     * @brief  对话 / 智能建议
     * @note   类型 CLlmClient*；类/结构体成员变量。
     */
    CLlmClient*    m_pLlm = nullptr;
    /**
     * @brief  翻译（可独立配置，未配则与 llm_ 同参）
     * @note   类型 CLlmClient*；类/结构体成员变量。
     */
    CLlmClient*    m_pTranslateLlm = nullptr;
    /**
     * @brief  译文。 初值 = nullptr。
     * @note   类型 CTranslationService*；类/结构体成员变量。
     */
    CTranslationService* m_pTranslation = nullptr;
    /**
     * @brief  seq -> 气泡，用于异步回填译文
     * @note   类型 QHash<qint64, CMessageBubble*>；类/结构体成员变量。
     */
    QHash<qint64, CMessageBubble*> m_uBubbles;
    /**
     * @brief  正在播放的气泡（用于切换播放/停止图标）
     * @note   类型 QPointer<CMessageBubble>；类/结构体成员变量。
     */
    QPointer<CMessageBubble> m_stPlayingBubble;
    /**
     * @brief  配置Watcher。 初值 = nullptr。
     * @note   类型 QFileSystemWatcher*；类/结构体成员变量。
     */
    QFileSystemWatcher* m_pConfigWatcher = nullptr;
    /**
     * @brief  配置Reload定时器。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*             m_pConfigReloadTimer = nullptr;
    /**
     * @brief  历史消息译文Budget。 初值 = 12。
     * @note   类型 int；类/结构体成员变量。
     */
    int                 m_nHistoryTranslationBudget = 12;
    /**
     * @brief  配置错误。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString             m_strConfigError;
    /**
     * @brief  待转写的语音 seq
     * @note   类型 QQueue<qint64>；类/结构体成员变量。
     */
    QQueue<qint64>      m_uTranscriptQueue;
    /**
     * @brief  正在转写的那条（0 = 空闲）
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64              m_uTranscribingSeq = 0;
    /**
     * @brief  语音消息状态令牌。 初值 = 0。
     * @note   类型 int；类/结构体成员变量。
     */
    int           m_nVoiceStatusToken = 0;
    /**
     * @brief  语音消息Available。 初值 = true。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool          m_nVoiceAvailable = true;

    /**
     * @brief  成员Panel。 初值 = nullptr。
     * @note   类型 QFrame*；类/结构体成员变量。
     */
    QFrame*       m_pMemberPanel = nullptr;
    /**
     * @brief  成员列表布局。 初值 = nullptr。
     * @note   类型 QVBoxLayout*；类/结构体成员变量。
     */
    QVBoxLayout*  m_pMemberListLayout = nullptr;
    /**
     * @brief  成员Title。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*       m_pMemberTitle = nullptr;
};
