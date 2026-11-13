/*
 * 文件名:    TtsService.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CTtsService 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CTtsService.h */

/* Piper 离线语音合成：把英文文本合成 WAV 并播放。 */
/* 合成结果按文本哈希缓存，同一句只跑一次 Piper。 */

/* 播放用 QSoundEffect 而不是 QMediaPlayer：QMediaPlayer 在「播放中途 stop() */
/* 后立刻 setSource 同一文件再 play()」时会进入 PlayingState 却不出声 */
/* （实测：连续点 6 次只有第 1 次有声音）。QSoundEffect 专为短音效设计， */
/* 反复 play() 可靠。 */
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QProcess;
class QSoundEffect;

class CTtsService : public QObject {
    Q_OBJECT

public:
    explicit CTtsService(QObject* parent = nullptr);
    ~CTtsService() override;

    void setPaths(const QString& piperBin, const QString& voiceModel);
    bool isAvailable() const;

    /* 合成并播放；同一句命中缓存直接播。重复调用会从头重播。 */
    void speak(const QString& text);
    void stop();
    bool isSpeaking() const { return speaking_; }

    /* 最近一次 speak 的文本（UI 用来高亮对应气泡的按钮）。 */
    QString currentText() const { return m_strCurrentText; }

signals:
    void speakingChanged(bool speaking);
    void failed(const QString& reason);

private:
    void playFile(const QString& path);
    QString cachePathFor(const QString& text) const;
    void cleanupPartFile();

    QString m_strPiperBin;
    QString m_strVoiceModel;
    QProcess*     m_pProcess = nullptr;
    QSoundEffect* m_pEffect = nullptr;

    /* piper 正在写的临时文件（xxx.wav.part） */
    QString m_strPendingPart;
    /* 合成成功后要改名成的最终缓存路径 */
    QString m_strPendingFinal;
    QString m_strPendingText;
    /* 当前（或最近）要朗读的文本 */
    QString m_strCurrentText;

    /* QSoundEffect 当前已加载的文件 */
    QString m_strLoadedPath;
    bool    m_nPendingPlay = false;

    /* text -> wav 路径 */
    QHash<QString, QString> m_strCache;
    bool    speaking_ = false;
};
