/**
 * @file       ChatPage.h
 * @brief      聊天界面（纯视图）。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

#pragma once

#include <QByteArray>
#include <QDate>
#include <QHash>
#include <QPlainTextEdit>
#include <QPointer>
#include <QString>
#include <QWidget>

#include "business/ChatTypes.h"

class CAvatar;
class CMessageBubble;
class CMicButton;
class QFrame;
class QLabel;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QTimer;
class QVBoxLayout;
class CSendButton;
class CSuggestionBar;
class CTranslationCard;
class CTtsService;
class CVoiceRecorder;

namespace business {
class ChatController;
class ChatModel;
struct ChatItem_S;
/**
 * @brief  namespace business。
 */
}

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
     * @brief      用户按下 Enter（未按 Shift）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit();

protected:
    /**
     * @brief      拦截 Enter，其余键交给基类处理。
     * @param[in] event          事件；类型 QKeyEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void keyPressEvent(QKeyEvent* event) override;
};

/**
 * @brief   聊天页。
 * @details 布局分三块：顶栏（返回 / 房间信息 / 成员按钮）、消息滚动区、 底部输入区（建议条、译文卡、状态条、输入框）。 右侧是可选成员面板。
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
    ~ChatPage() override;

    /**
     * @brief  ---- 由 CMainWindow 驱动的生命周期与消息投递 ----。
     * @details 首次进入房间：清空消息区并显示入房提示。
     */
    void enterRoom(const QString& room, bool created, const QVector<MemberInfo_S>& members,
                   qint64 selfId, const QString& selfName);

    /**
     * @brief 断线重连后回到原房间：保留已有消息，只刷新成员与连接状态。
     * @param room     房间号。
     * @param members  最新成员列表。
     * @param selfId   新分配的会话 id。
     * @param selfName 本人昵称。
     */
    void resumeRoom(const QString& room, const QVector<MemberInfo_S>& members,
                    qint64 selfId, const QString& selfName);

    /**
     * @brief      追加一条文本消息。
     * @param[in] seq            消息序号；类型 qint64。
     * @param[in] from           发送者 id；类型 qint64。
     * @param[in] name           发送者昵称；类型 const QString&。
     * @param[in] text           正文；类型 const QString&。
     * @param[in] ts             时间戳；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void appendChat(qint64 seq, qint64 from, const QString& name, const QString& text, qint64 ts);

    /**
     * @brief 追加一条语音消息。
     * @param seq        消息序号。
     * @param from       发送者 id。
     * @param name       发送者昵称。
     * @param audio      DPV1 容器。
     * @param durationMs 时长。
     * @param ts         时间戳。
     */
    void appendVoice(qint64 seq, qint64 from, const QString& name, const QByteArray& audio,
                     int durationMs, qint64 ts);

    /**
     * @brief      批量合并历史消息。
     * @param[in] messages       历史消息列表；类型 const QVector<ChatMessage_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void mergeHistory(const QVector<ChatMessage_S>& messages);

    /**
     * @brief      追加一条系统提示。
     * @param[in] text           提示文本；类型 const QString&。
     * @param[in] ts             时间戳；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void appendSystemText(const QString& text, qint64 ts);

    /**
     * @brief 应用一条系统事件并刷新成员列表。
     * @param event   事件名。
     * @param userId  相关用户 id。
     * @param name    用户昵称。
     * @param oldName 旧昵称（rename 事件用，可为空）。
     * @param text    提示文本。
     * @param ts      时间戳。
     */
    void onSystemEvent(const QString& event, qint64 userId, const QString& name,
                       const QString& oldName, const QString& text, qint64 ts);

    /**
     * @brief      切换在线状态。
     * @param[in] online         false 时禁用输入并提示正在重连；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setOnline(bool online);

    /**
     * @brief      清空房间数据（离开房间或登出）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearRoom();

    /**
     * @brief      应用服务端下发的 LLM 配置。
     * @details    由 CMainWindow 在登录成功后转发；内部交给 ChatController 处理。
     * @param[in] json           服务端随 LoginResp 下发的原始 JSON；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void applyRemoteConfig(const QByteArray& json);

    /**
     * @brief      当前房间号，空串表示不在房间。
     * @note       本函数无输入参数。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    QString room() const;

signals:
    /**
     * @brief      用户点击返回箭头。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void backRequested();

    /**
     * @brief      用户要发送文本。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendRequested(const QString& text);

    /**
     * @brief      用户录完一段语音，请求发送。
     * @param[in] audio          Opus 编码的音频数据；类型 const QByteArray&。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendVoiceRequested(const QByteArray& audio, int durationMs);

    /**
     * @brief      需要主窗口弹出提示条。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] error          错误信息；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void notice(const QString& text, bool error);

protected:
    /**
     * @brief      宽度变化时刷新响应式元素（占位文案等）。
     * @param[in] event          事件；类型 QResizeEvent*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void resizeEvent(QResizeEvent* event) override;

private:
    /**
     * @brief      ---- 构建 ----。
     * @param[in] columnLayout   输入参数；类型 QVBoxLayout*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void buildHeader(QVBoxLayout* columnLayout);
    /**
     * @brief      构建消息内容 area。
     * @param[in] columnLayout   输入参数；类型 QVBoxLayout*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void buildMessageArea(QVBoxLayout* columnLayout);
    /**
     * @brief      构建 composer。
     * @param[in] columnLayout   输入参数；类型 QVBoxLayout*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void buildComposer(QVBoxLayout* columnLayout);
    /**
     * @brief      构建成员 panel。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void buildMemberPanel();
    /**
     * @brief      执行 wiresignals相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void wireSignals();

    /**
     * @brief  ---- 渲染 ----。
     * @details 渲染一条消息（含日期分隔）。
     */
    void renderItem(const business::ChatItem_S& item);

    /**
     * @brief      清空视图后按模型全量重画。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void rebuildView();

    /**
     * @brief      删除所有消息控件（保留底部弹簧）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearView();

    /**
     * @brief      某条消息状态变化时刷新对应气泡。
     * @param[in] seq            消息序号；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void refreshBubble(qint64 seq);

    /**
     * @brief      时间戳跨天时插入日期分隔条。
     * @param[in] ts             时间戳；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void appendDateSeparatorIfNeeded(qint64 ts);

    /**
     * @brief      滚到底部（布局结算后可能再触发一次）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void scrollToBottom();

    /**
     * @brief      立即滚到底部。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void forceScrollToBottom();

    /**
     * @brief      把控件高度钉死为它的 sizeHint。
     * @details    QVBoxLayout 会把子控件压进可用高度，导致 canvas 不增长、 滚动条 maximum 恒为 0。钉死高度后 canvas 只能随内容变高。
     * @param[in] widget         刚加入消息区布局的控件；类型 QWidget*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    static void pinHeight(QWidget* widget);

    /**
     * @brief  ---- 交互 ----。
     * @details 读取输入框并发出 sendRequested。
     */
    void sendCurrentText();

    /**
     * @brief      开始或结束录音。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void toggleRecording();

    /**
     * @brief      录音结束，交给编排器编码发送。
     * @param[in] pcm            原始音频；类型 const QByteArray&。
     * @param[in] durationMs     时长；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onVoiceRecorded(const QByteArray& pcm, int durationMs);

    /**
     * @brief      显示或清除底部状态提示。
     * @param[in] text           文本，空串表示清除；类型 const QString&。
     * @param[in] error          是否错误样式；类型 bool。
     * @param[in] autoHideMs     自动隐藏毫秒数，0 表示不自动隐藏；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showVoiceStatus(const QString& text, bool error, int autoHideMs);

    /**
     * @brief      重建右侧成员列表。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void rebuildMembers();

    /**
     * @brief      窄屏下的成员列表弹窗。
     * @details    手机竖屏没有横向空间放 220px 侧栏，改成模态弹窗。 与侧栏共用 `createMemberRow()`，避免两处渲染逻辑漂移。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void showMemberDialog();

    /**
     * @brief      构造一行成员（头像 + 昵称 + 「我」标记）。
     * @param[in] userId         成员 id；类型 qint64。
     * @param[in] name           昵称；类型 const QString&。
     * @return     未挂到任何布局的新控件，调用方负责加入布局。
     * @author     JaTin
     */
    QWidget* createMemberRow(qint64 userId, const QString& name);

    /**
     * @brief      刷新顶栏的成员计数。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void updateHeader();

    /**
     * @brief      通知编排器输入框当前是否为空。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void syncComposerState();

    /**
     * @brief      按当前宽度更新输入框占位文案。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void updateComposerPlaceholder();

    /**
     * @brief  ---- 数据 ----。
     * @details 消息模型（持有）。
     */
    business::ChatModel*      model_ = nullptr;
    /**
     * @brief  业务编排（持有）
     * @note   类型 business::ChatController*；类/结构体成员变量。
     */
    business::ChatController* m_pController = nullptr;

    /**
     * @brief  ---- 顶栏 ----
     * @note   类型 CAvatar*；类/结构体成员变量。
     */
    CAvatar*      m_pRoomAvatar = nullptr;
    /**
     * @brief  房间Title。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pRoomTitle = nullptr;
    /**
     * @brief  房间Subtitle。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pRoomSubtitle = nullptr;
    /**
     * @brief  成员按钮。 初值 = nullptr。
     * @note   类型 QPushButton*；类/结构体成员变量。
     */
    QPushButton* m_pMemberBtn = nullptr;

    /**
     * @brief  ---- 消息区 ----
     * @note   类型 QScrollArea*；类/结构体成员变量。
     */
    QScrollArea* m_pScroll = nullptr;
    /**
     * @brief  Canvas。 初值 = nullptr。
     * @note   类型 QWidget*；类/结构体成员变量。
     */
    QWidget*     m_pCanvas = nullptr;
    /**
     * @brief  Chat布局。 初值 = nullptr。
     * @note   类型 QVBoxLayout*；类/结构体成员变量。
     */
    QVBoxLayout* m_pChatLayout = nullptr;
    /**
     * @brief  seq -> 气泡，用于局部刷新
     * @note   类型 QHash<qint64, CMessageBubble*>；类/结构体成员变量。
     */
    QHash<qint64, CMessageBubble*> m_uBubbles;
    /**
     * @brief  正在播放 TTS 的气泡
     * @note   类型 QPointer<CMessageBubble>；类/结构体成员变量。
     */
    QPointer<CMessageBubble> m_stPlayingBubble;
    /**
     * @brief  最后渲染的日期，用于分隔条
     * @note   类型 QDate；类/结构体成员变量。
     */
    QDate        m_stLastDate;
    /**
     * @brief  是否仍处于「贴底」状态
     * @note   类型 bool；类/结构体成员变量。
     */
    bool         m_nPendingScroll = false;
    /**
     * @brief  用户是否停在底部（滚动条到底）
     * @note   类型 bool；类/结构体成员变量。
     */
    bool         m_nStickToBottom = true;
    /**
     * @brief  贴底状态的结算计时（每次滚动都重置）
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*      m_pScrollSettleTimer = nullptr;

    /**
     * @brief  ---- 输入区 ----
     * @note   类型 ComposerEdit*；类/结构体成员变量。
     */
    ComposerEdit*    m_pComposer = nullptr;
    /**
     * @brief  Send按钮。 初值 = nullptr。
     * @note   类型 CSendButton*；类/结构体成员变量。
     */
    CSendButton*      m_pSendBtn = nullptr;
    /**
     * @brief  Mic按钮。 初值 = nullptr。
     * @note   类型 CMicButton*；类/结构体成员变量。
     */
    CMicButton*       m_pMicBtn = nullptr;
    /**
     * @brief  语音消息状态。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*          m_pVoiceStatus = nullptr;
    /**
     * @brief  智能建议工具条。 初值 = nullptr。
     * @note   类型 CSuggestionBar*；类/结构体成员变量。
     */
    CSuggestionBar*   m_pSuggestionBar = nullptr;
    /**
     * @brief  译文卡片。 初值 = nullptr。
     * @note   类型 CTranslationCard*；类/结构体成员变量。
     */
    CTranslationCard* m_pTranslationCard = nullptr;
    /**
     * @brief  防止旧的自动隐藏计时误关新提示
     * @note   类型 int；类/结构体成员变量。
     */
    int              m_nVoiceStatusToken = 0;

    /**
     * @brief  ---- 服务 ----。
     * @details 录音与本地识别。
     */
    CVoiceRecorder* m_pVoice = nullptr;
    /**
     * @brief  语音合成
     * @note   类型 CTtsService*；类/结构体成员变量。
     */
    CTtsService*    m_pTts = nullptr;
    /**
     * @brief  识别模型是否可用
     * @note   类型 bool；类/结构体成员变量。
     */
    bool           m_nVoiceAvailable = true;

    /**
     * @brief  ---- 成员面板 ----
     * @note   类型 QFrame*；类/结构体成员变量。
     */
    QFrame*      m_pMemberPanel = nullptr;
    /**
     * @brief  成员列表布局。 初值 = nullptr。
     * @note   类型 QVBoxLayout*；类/结构体成员变量。
     */
    QVBoxLayout* m_pMemberListLayout = nullptr;
    /**
     * @brief  成员Title。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pMemberTitle = nullptr;
};
