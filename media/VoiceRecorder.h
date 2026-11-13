/*
 * 文件名:    VoiceRecorder.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CVoiceRecorder 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CVoiceRecorder.h */

/* 语音输入：QAudioSource 采集 16 kHz/单声道/16-bit PCM，交给子线程里的 whisper 识别。 */
/* 对界面只暴露「开始/停止录音」和几个信号，UI 不需要知道音频与模型的细节。 */
#pragma once

#include <QByteArray>
#include <QElapsedTimer>
#include <QObject>
#include <QString>

class QAudioSource;
class QIODevice;
class QThread;
class QTimer;
class CWhisperWorker;

class CVoiceRecorder : public QObject {
    Q_OBJECT

public:
    explicit CVoiceRecorder(QObject* parent = nullptr);
    ~CVoiceRecorder() override;

    void setModelPath(const QString& path) { m_strModelPath = path; }

    /* 异步加载模型（1~2 秒），完成后发 modelLoaded。 */
    void start();

    bool modelReady() const { return m_nModelReady; }
    bool isRecording() const { return m_nRecording; }
    bool isBusy() const { return m_nBusy || m_nRecording; }

    /* 录音用途：Transcribe = 交给 whisper 转文字；Send = 直接取走 PCM 发语音消息。 */
    enum class Purpose_EN {
        Transcribe,
        Send,
    };

    void startRecording(Purpose_EN purpose = Purpose_EN::Transcribe);
    void stopRecording();

    /* 从 WAV 文件识别（无麦克风时的演示/验证通道）。 */
    void transcribeFile(const QString& wavPath);

    /* 与协议 kMaxVoiceDurationMs 一致 */
    static constexpr int kMaxRecordMs = 20000;

signals:
    void modelLoaded(bool ok, const QString& message);
    void recordingStarted();
    void recordingTick(int seconds, int level);
    void recognizing();
    void textReady(const QString& text, qint64 elapsedMs);
    /* Send 模式下停止录音后发出：16 kHz / 单声道 / 16bit 的原始 PCM。 */
    void pcmReady(const QByteArray& pcm, int durationMs);
    void failed(const QString& message);

private:
    void onAudioReady();
    void submit(const QByteArray& pcm);

    QThread*       m_pThread = nullptr;
    CWhisperWorker* m_pWorker = nullptr;
    QAudioSource*  m_pSource = nullptr;
    QIODevice*     m_pDevice = nullptr;
    QTimer*        m_pTick = nullptr;
    QByteArray     m_byPcm;
    Purpose_EN        m_stPurpose = Purpose_EN::Transcribe;
    QElapsedTimer  m_stElapsed;
    QString        m_strModelPath;
    bool           m_nModelReady = false;
    bool           m_nRecording = false;
    bool           m_nBusy = false;
};
