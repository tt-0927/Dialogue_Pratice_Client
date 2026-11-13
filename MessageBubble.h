/*
 * 文件名:    MessageBubble.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMessageBubble 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMessageBubble.h */

/* 单条聊天气泡：自己发的靠右、绿色；别人的靠左、白色带昵称与头像。 */
/* 他人的英文消息会在正文下方追加一行中文译文。 */
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

class CMessageBubble : public QWidget {
    Q_OBJECT

public:
    CMessageBubble(const QString& senderName,
                  const QString& text,
                  const QString& timeText,
                  bool outgoing,
                  qint64 senderId,
                  bool playable = false,
                  QWidget* parent = nullptr);

    /* 显示译文（空串等同于清除）。 */
    void setTranslation(const QString& text);
    /* 语音消息的英文转写（显示在中文译文上方）。 */
    void setTranscript(const QString& text);
    /* 转写占位（识别中…）。 */
    void setTranscriptPending();
    /* 显示「翻译中…」占位。 */
    void setTranslationPending();
    /* 翻译失败占位。 */
    void setTranslationFailed();
    void clearTranslation();

    /* 同步播放按钮的播放/停止外观。 */
    void setPlaying(bool playing);

    /* 把气泡正文换成语音条（用于语音消息）。 */
    void setVoice(const QByteArray& audio, int durationMs);

signals:
    /* 点击气泡里的播放按钮，把原文交给 TTS 朗读（文本消息）。 */
    void playRequested(const QString& text);
    /* 点击语音条，请求播放这段语音。 */
    void voicePlayRequested(const QByteArray& audio);

private:
    QFrame*      m_pBubble = nullptr;
    QVBoxLayout* m_pBubbleLayout = nullptr;
    QLabel*      m_pBody = nullptr;
    CVoiceBar*    m_pVoiceBar = nullptr;
    QByteArray   m_byVoiceAudio;
    QLabel* m_pTranscriptLabel = nullptr;
    QFrame* m_pTranslationDivider = nullptr;
    QLabel* m_pTranslationLabel = nullptr;
    CPlayButton* m_pPlayBtn = nullptr;
};
