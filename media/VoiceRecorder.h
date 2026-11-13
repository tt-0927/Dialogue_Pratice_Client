// VoiceRecorder.h
//
// 语音输入：QAudioSource 采集 16 kHz/单声道/16-bit PCM，交给子线程里的 whisper 识别。
// 对界面只暴露「开始/停止录音」和几个信号，UI 不需要知道音频与模型的细节。
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QAudioSource;
class QIODevice;
class QThread;
class QTimer;
class WhisperWorker;

class VoiceRecorder : public QObject {
    Q_OBJECT

public:
    explicit VoiceRecorder(QObject* parent = nullptr);
    ~VoiceRecorder() override;

    void setModelPath(const QString& path) { modelPath_ = path; }

    /// 异步加载模型（1~2 秒），完成后发 modelLoaded。
    void start();

    bool modelReady() const { return modelReady_; }
    bool isRecording() const { return recording_; }
    bool isBusy() const { return busy_ || recording_; }

    /// 录音用途：Transcribe = 交给 whisper 转文字；Send = 直接取走 PCM 发语音消息。
    enum class Purpose {
        Transcribe,
        Send,
    };

    void startRecording(Purpose purpose = Purpose::Transcribe);
    void stopRecording();

    /// 从 WAV 文件识别（无麦克风时的演示/验证通道）。
    void transcribeFile(const QString& wavPath);

    static constexpr int kMaxRecordMs = 20000;   // 与协议 kMaxVoiceDurationMs 一致

signals:
    void modelLoaded(bool ok, const QString& message);
    void recordingStarted();
    void recordingTick(int seconds, int level);
    void recognizing();
    void textReady(const QString& text, qint64 elapsedMs);
    /// Send 模式下停止录音后发出：16 kHz / 单声道 / 16bit 的原始 PCM。
    void pcmReady(const QByteArray& pcm, int durationMs);
    void failed(const QString& message);

private:
    void onAudioReady();
    void submit(const QByteArray& pcm);

    QThread*       thread_ = nullptr;
    WhisperWorker* worker_ = nullptr;
    QAudioSource*  source_ = nullptr;
    QIODevice*     device_ = nullptr;
    QTimer*        tick_ = nullptr;
    QByteArray     pcm_;
    Purpose        purpose_ = Purpose::Transcribe;
    QElapsedTimer  elapsed_;
    QString        modelPath_;
    bool           modelReady_ = false;
    bool           recording_ = false;
    bool           busy_ = false;
};
