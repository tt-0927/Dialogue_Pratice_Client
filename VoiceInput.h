/**
 * @file       VoiceInput.h
 * @brief      CVoiceInput 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoiceInput.h。
 */

/**
 * @brief  语音输入：QAudioSource 采集 16 kHz/单声道/16-bit PCM，交给子线程里的 whisper 识别。 对界面只暴露「开始/停止录音」和几个信号，UI 不需要知道音频与模型的细节。
 */
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

/**
 * @brief   C语音消息输入。
 * @author  JaTin
 */
class CVoiceInput : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CVoiceInput(QObject* parent = nullptr);
    ~CVoiceInput() override;

    void setModelPath(const QString& path) { m_strModelPath = path; }

    /**
     * @brief      异步加载模型（1~2 秒），完成后发 modelLoaded。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void start();

    bool modelReady() const { return m_nModelReady; }
    bool isRecording() const { return m_nRecording; }
    bool isBusy() const { return m_nBusy || m_nRecording; }

    /**
     * @brief  录音用途：Transcribe = 交给 whisper 转文字；Send = 直接取走 PCM 发语音消息。
     */
    enum class Purpose_EN {
        Transcribe,
        Send,
    };

    /**
     * @brief      启动 recording。
     * @param[in] purpose        输入参数；类型 Purpose_EN。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void startRecording(Purpose_EN purpose = Purpose_EN::Transcribe);
    /**
     * @brief      停止 recording。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void stopRecording();

    /**
     * @brief      从 WAV 文件识别（无麦克风时的演示/验证通道）。
     * @param[in] wavPath        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void transcribeFile(const QString& wavPath);

    /**
     * @brief  与协议 kMaxVoiceDurationMs 一致
     * @note   取值 20000。
     */
    static constexpr int kMaxRecordMs = 20000;

signals:
    /**
     * @brief      执行模型名 loaded。
     * @param[in] ok             输入参数；类型 bool。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void modelLoaded(bool ok, const QString& message);
    /**
     * @brief      执行 recordingstarted相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void recordingStarted();
    /**
     * @brief      执行 recordingtick相关的数据并返回处理结果。
     * @param[in] seconds        输入参数；类型 int。
     * @param[in] level          日志级别；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void recordingTick(int seconds, int level);
    /**
     * @brief      执行 recognizing相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void recognizing();
    /**
     * @brief      执行文本 ready。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] elapsedMs      输入参数；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void textReady(const QString& text, qint64 elapsedMs);
    /**
     * @brief      Send 模式下停止录音后发出：16 kHz / 单声道 / 16bit 的原始 PCM。
     * @param[in] pcm            输入参数；类型 const QByteArray&。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void pcmReady(const QByteArray& pcm, int durationMs);
    /**
     * @brief      执行 failed相关的数据并返回处理结果。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(const QString& message);

private:
    /**
     * @brief      执行 on 音频数据 ready。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onAudioReady();
    /**
     * @brief      提交 submit。
     * @param[in] pcm            输入参数；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void submit(const QByteArray& pcm);

    /**
     * @brief  线程。 初值 = nullptr。
     * @note   类型 QThread*；类/结构体成员变量。
     */
    QThread*       m_pThread = nullptr;
    /**
     * @brief  Worker。 初值 = nullptr。
     * @note   类型 CWhisperWorker*；类/结构体成员变量。
     */
    CWhisperWorker* m_pWorker = nullptr;
    /**
     * @brief  配置来源路径。 初值 = nullptr。
     * @note   类型 QAudioSource*；类/结构体成员变量。
     */
    QAudioSource*  m_pSource = nullptr;
    /**
     * @brief  Device。 初值 = nullptr。
     * @note   类型 QIODevice*；类/结构体成员变量。
     */
    QIODevice*     m_pDevice = nullptr;
    /**
     * @brief  Tick。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*        m_pTick = nullptr;
    /**
     * @brief  Pcm。
     * @note   类型 QByteArray；类/结构体成员变量。
     */
    QByteArray     m_byPcm;
    /**
     * @brief  Purpose。 初值 = Purpose_EN::Transcribe。
     * @note   类型 Purpose_EN；类/结构体成员变量。
     */
    Purpose_EN        m_stPurpose = Purpose_EN::Transcribe;
    /**
     * @brief  Elapsed。
     * @note   类型 QElapsedTimer；类/结构体成员变量。
     */
    QElapsedTimer  m_stElapsed;
    /**
     * @brief  模型名文件路径。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString        m_strModelPath;
    /**
     * @brief  模型名Ready。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool           m_nModelReady = false;
    /**
     * @brief  Recording。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool           m_nRecording = false;
    /**
     * @brief  Busy。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool           m_nBusy = false;
};
