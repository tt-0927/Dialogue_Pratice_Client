/*
 * 文件名:    WhisperWorker.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CWhisperWorker 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CWhisperWorker.cpp */
#include "media/WhisperWorker.h"

#ifdef DP_WITH_WHISPER

#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>
#include <QtDebug>

#include <algorithm>
#include <vector>

#include "whisper.h"

CWhisperWorker::CWhisperWorker(QString modelPath, QObject* parent)
    : QObject(parent), m_strModelPath(std::move(modelPath)) {}

/*
 * 功能描述:  执行 CWhisperWorker 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
CWhisperWorker::~CWhisperWorker() {
    if (m_pCtx != nullptr) {
        whisper_free(m_pCtx);
        m_pCtx = nullptr;
    }
}

/*
 * 功能描述:  执行 initialize 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CWhisperWorker::initialize() {
    if (m_pCtx != nullptr) {
        emit initialized(true, QString());
        return;
    }
    if (m_strModelPath.isEmpty() || !QFileInfo::exists(m_strModelPath)) {
        emit initialized(false, QStringLiteral("找不到语音模型：%1").arg(m_strModelPath));
        return;
    }

    whisper_context_params params = whisper_context_default_params();
    /* 虚拟机无 GPU；纯 CPU 推理足够应付短句 */
    params.use_gpu = false;

    QElapsedTimer timer;
    timer.start();
    m_pCtx = whisper_init_from_file_with_params(m_strModelPath.toUtf8().constData(), params);
    if (m_pCtx == nullptr) {
        emit initialized(false, QStringLiteral("语音模型加载失败：%1").arg(m_strModelPath));
        return;
    }

    qInfo() << "whisper model loaded in" << timer.elapsed() << "ms:" << m_strModelPath;
    emit initialized(true, QString());
}

/*
 * 功能描述:  执行 transcribe 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  pcm  参数。
 * return:    void
 */
void CWhisperWorker::transcribe(const QByteArray& pcm) {
    if (m_pCtx == nullptr) {
        emit failed(QStringLiteral("语音模型尚未就绪"));
        return;
    }
    /* < 0.1 秒，没必要送进模型 */
    if (pcm.size() < 3200) {
        emit failed(QStringLiteral("录音太短，没有识别到语音"));
        return;
    }

    /* int16 -> float [-1, 1]，whisper 只吃 float PCM */
    const int sampleCount = pcm.size() / 2;
    std::vector<float> samples(static_cast<size_t>(sampleCount));
    const auto* src = reinterpret_cast<const int16_t*>(pcm.constData());
    for (int i = 0; i < sampleCount; ++i) {
        samples[static_cast<size_t>(i)] = static_cast<float>(src[i]) / 32768.0f;
    }

    whisper_full_params p = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    p.print_progress   = false;
    p.print_special    = false;
    p.print_realtime   = false;
    p.print_timestamps = false;
    p.translate        = false;
    /* 英语练习场景，锁死英文可减少误判 */
    p.language         = "en";
    /* 每句独立，不继承上一句上下文 */
    p.no_context       = true;
    p.single_segment   = false;
    p.n_threads        = std::max(1, QThread::idealThreadCount() - 1);

    QElapsedTimer timer;
    timer.start();
    const int rc = whisper_full(m_pCtx, p, samples.data(), sampleCount);
    const qint64 elapsed = timer.elapsed();

    if (rc != 0) {
        emit failed(QStringLiteral("语音识别失败（whisper_full 返回 %1）").arg(rc));
        return;
    }

    QString text;
    const int segments = whisper_full_n_segments(m_pCtx);
    for (int i = 0; i < segments; ++i) {
        text += QString::fromUtf8(whisper_full_get_segment_text(m_pCtx, i));
    }
    text = text.trimmed();

    /* 静音时 whisper 会输出 [BLANK_AUDIO] / [ Silence ] 这类标记 */
    if (text.isEmpty() || text.startsWith(QLatin1Char('[')) || text == QLatin1String("()")) {
        emit failed(QStringLiteral("没有识别到语音，请靠近麦克风重试"));
        return;
    }

    emit finished(text, elapsed);
}

/* !DP_WITH_WHISPER */
#else

/* 本平台未编译 whisper.cpp（Android 交叉编译用 -DDP_WITH_WHISPER=OFF）。 */
/* 接口签名保持一致，初始化直接失败，调用方按「语音输入不可用」降级， */
/* 文字聊天与语音消息收发不受影响。 */

#include <utility>

CWhisperWorker::CWhisperWorker(QString modelPath, QObject* parent)
    : QObject(parent), m_strModelPath(std::move(modelPath)) {}

CWhisperWorker::~CWhisperWorker() = default;

/*
 * 功能描述:  执行 initialize 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CWhisperWorker::initialize() {
    emit initialized(false, QStringLiteral("本平台未编译语音识别（whisper.cpp）"));
}

/*
 * 功能描述:  执行 transcribe 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  QByteArray  参数。
 * return:    void
 */
void CWhisperWorker::transcribe(const QByteArray&) {
    emit failed(QStringLiteral("本平台未编译语音识别（whisper.cpp）"));
}

/* DP_WITH_WHISPER */
#endif
