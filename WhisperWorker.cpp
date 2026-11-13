// WhisperWorker.cpp
#include "WhisperWorker.h"

#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>
#include <QtDebug>

#include <algorithm>
#include <vector>

#include "whisper.h"

WhisperWorker::WhisperWorker(QString modelPath, QObject* parent)
    : QObject(parent), modelPath_(std::move(modelPath)) {}

WhisperWorker::~WhisperWorker() {
    if (ctx_ != nullptr) {
        whisper_free(ctx_);
        ctx_ = nullptr;
    }
}

void WhisperWorker::initialize() {
    if (ctx_ != nullptr) {
        emit initialized(true, QString());
        return;
    }
    if (modelPath_.isEmpty() || !QFileInfo::exists(modelPath_)) {
        emit initialized(false, QStringLiteral("找不到语音模型：%1").arg(modelPath_));
        return;
    }

    whisper_context_params params = whisper_context_default_params();
    params.use_gpu = false;   // 虚拟机无 GPU；纯 CPU 推理足够应付短句

    QElapsedTimer timer;
    timer.start();
    ctx_ = whisper_init_from_file_with_params(modelPath_.toUtf8().constData(), params);
    if (ctx_ == nullptr) {
        emit initialized(false, QStringLiteral("语音模型加载失败：%1").arg(modelPath_));
        return;
    }

    qInfo() << "whisper model loaded in" << timer.elapsed() << "ms:" << modelPath_;
    emit initialized(true, QString());
}

void WhisperWorker::transcribe(const QByteArray& pcm) {
    if (ctx_ == nullptr) {
        emit failed(QStringLiteral("语音模型尚未就绪"));
        return;
    }
    if (pcm.size() < 3200) {   // < 0.1 秒，没必要送进模型
        emit failed(QStringLiteral("录音太短，没有识别到语音"));
        return;
    }

    // int16 -> float [-1, 1]，whisper 只吃 float PCM
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
    p.language         = "en";          // 英语练习场景，锁死英文可减少误判
    p.no_context       = true;          // 每句独立，不继承上一句上下文
    p.single_segment   = false;
    p.n_threads        = std::max(1, QThread::idealThreadCount() - 1);

    QElapsedTimer timer;
    timer.start();
    const int rc = whisper_full(ctx_, p, samples.data(), sampleCount);
    const qint64 elapsed = timer.elapsed();

    if (rc != 0) {
        emit failed(QStringLiteral("语音识别失败（whisper_full 返回 %1）").arg(rc));
        return;
    }

    QString text;
    const int segments = whisper_full_n_segments(ctx_);
    for (int i = 0; i < segments; ++i) {
        text += QString::fromUtf8(whisper_full_get_segment_text(ctx_, i));
    }
    text = text.trimmed();

    // 静音时 whisper 会输出 [BLANK_AUDIO] / [ Silence ] 这类标记
    if (text.isEmpty() || text.startsWith(QLatin1Char('[')) || text == QLatin1String("()")) {
        emit failed(QStringLiteral("没有识别到语音，请靠近麦克风重试"));
        return;
    }

    emit finished(text, elapsed);
}
