// TtsService.h
//
// Piper 离线语音合成：把英文文本合成 WAV 并播放。
// 合成结果按文本哈希缓存，同一句只跑一次 Piper。
//
// 播放用 QSoundEffect 而不是 QMediaPlayer：QMediaPlayer 在「播放中途 stop()
// 后立刻 setSource 同一文件再 play()」时会进入 PlayingState 却不出声
// （实测：连续点 6 次只有第 1 次有声音）。QSoundEffect 专为短音效设计，
// 反复 play() 可靠。
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QProcess;
class QSoundEffect;

class TtsService : public QObject {
    Q_OBJECT

public:
    explicit TtsService(QObject* parent = nullptr);
    ~TtsService() override;

    void setPaths(const QString& piperBin, const QString& voiceModel);
    bool isAvailable() const;

    /// 合成并播放；同一句命中缓存直接播。重复调用会从头重播。
    void speak(const QString& text);
    void stop();
    bool isSpeaking() const { return speaking_; }

    /// 最近一次 speak 的文本（UI 用来高亮对应气泡的按钮）。
    QString currentText() const { return currentText_; }

signals:
    void speakingChanged(bool speaking);
    void failed(const QString& reason);

private:
    void playFile(const QString& path);
    QString cachePathFor(const QString& text) const;
    void cleanupPartFile();

    QString piperBin_;
    QString voiceModel_;
    QProcess*     process_ = nullptr;
    QSoundEffect* effect_ = nullptr;

    QString pendingPart_;    // piper 正在写的临时文件（xxx.wav.part）
    QString pendingFinal_;   // 合成成功后要改名成的最终缓存路径
    QString pendingText_;
    QString currentText_;    // 当前（或最近）要朗读的文本

    QString loadedPath_;     // QSoundEffect 当前已加载的文件
    bool    pendingPlay_ = false;

    QHash<QString, QString> cache_;   // text -> wav 路径
    bool    speaking_ = false;
};
