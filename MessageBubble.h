/**
 * @file       MessageBubble.h
 * @brief      CMessageBubble 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CMessageBubble.h。
 */

/**
 * @brief  单条聊天气泡：自己发的靠右、绿色；别人的靠左、白色带昵称与头像。 他人的英文消息会在正文下方追加一行中文译文。
 */
#pragma once

#include <QByteArray>
#include <QColor>
#include <QString>
#include <QWidget>

class CPlayButton;
class QFrame;
class QLabel;
class QVBoxLayout;
class CVoiceBar;

/**
 * @brief   C消息内容消息气泡。
 * @author  JaTin
 */
class CMessageBubble : public QWidget {
    Q_OBJECT

public:
    CMessageBubble(const QString& senderName,
                  const QString& text,
                  const QString& timeText,
                  bool outgoing,
                  qint64 senderId,
                  bool playable = false,
                  /**
                   * @brief  Qt 父对象指针，用于对象树所有权管理。 初值 = nullptr)。
                   * @note   类型 QWidget*；类/结构体成员变量。
                   */
                  QWidget* parent = nullptr);

    /**
     * @brief      显示译文（空串等同于清除）。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranslation(const QString& text);
    /**
     * @brief      语音消息的英文转写（显示在中文译文上方）。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranscript(const QString& text);
    /**
     * @brief      转写占位（识别中…）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranscriptPending();
    /**
     * @brief      显示「翻译中…」占位。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranslationPending();
    /**
     * @brief      翻译失败占位。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setTranslationFailed();
    /**
     * @brief      清空译文。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearTranslation();

    /**
     * @brief      同步播放按钮的播放/停止外观。
     * @param[in] playing        输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setPlaying(bool playing);

    /**
     * @brief      把气泡正文换成语音条（用于语音消息）。
     * @param[in] audio          Opus 编码的音频数据；类型 const QByteArray&。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setVoice(const QByteArray& audio, int durationMs);

signals:
    /**
     * @brief      点击气泡里的播放按钮，把原文交给 TTS 朗读（文本消息）。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void playRequested(const QString& text);
    /**
     * @brief      点击语音条，请求播放这段语音。
     * @param[in] audio          Opus 编码的音频数据；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void voicePlayRequested(const QByteArray& audio);

private:
    /**
     * @brief  消息气泡。 初值 = nullptr。
     * @note   类型 QFrame*；类/结构体成员变量。
     */
    QFrame*      m_pBubble = nullptr;
    /**
     * @brief  消息气泡布局。 初值 = nullptr。
     * @note   类型 QVBoxLayout*；类/结构体成员变量。
     */
    QVBoxLayout* m_pBubbleLayout = nullptr;
    /**
     * @brief  正文。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel*      m_pBody = nullptr;
    /**
     * @brief  语音消息工具条。 初值 = nullptr。
     * @note   类型 CVoiceBar*；类/结构体成员变量。
     */
    CVoiceBar*    m_pVoiceBar = nullptr;
    /**
     * @brief  语音消息音频数据。
     * @note   类型 QByteArray；类/结构体成员变量。
     */
    QByteArray   m_byVoiceAudio;
    /**
     * @brief  转写文本标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel* m_pTranscriptLabel = nullptr;
    /**
     * @brief  译文分隔线。 初值 = nullptr。
     * @note   类型 QFrame*；类/结构体成员变量。
     */
    QFrame* m_pTranslationDivider = nullptr;
    /**
     * @brief  译文标签。 初值 = nullptr。
     * @note   类型 QLabel*；类/结构体成员变量。
     */
    QLabel* m_pTranslationLabel = nullptr;
    /**
     * @brief  Play按钮。 初值 = nullptr。
     * @note   类型 CPlayButton*；类/结构体成员变量。
     */
    CPlayButton* m_pPlayBtn = nullptr;
};
