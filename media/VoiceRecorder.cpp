// VoiceRecorder.cpp
#include "media/VoiceRecorder.h"

#include "media/WhisperWorker.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QFile>
#include <QMediaDevices>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QtEndian>

#include <algorithm>
#include <cmath>

namespace {

/// 极简 WAV 解析：只接受 16 kHz / 单声道 / 16-bit，正好是 whisper 的输入要求。
bool loadWavPcm16(const QString& path, QByteArray& pcm, QString& error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("无法打开文件：%1").arg(path);
        return false;
    }
    const QByteArray all = file.readAll();
    if (all.size() < 44 || !all.startsWith("RIFF")) {
        error = QStringLiteral("不是合法的 WAV 文件");
        return false;
    }

    int channels = 0;
    int rate = 0;
    int bits = 0;
    int pos = 12;

    while (pos + 8 <= all.size()) {
        const QByteArray id = all.mid(pos, 4);
        const quint32 size = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(all.constData() + pos + 4));
        const int dataStart = pos + 8;

        if (id == "fmt " && dataStart + 16 <= all.size()) {
            channels = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(all.constData() + dataStart + 2));
            rate     = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(all.constData() + dataStart + 4));
            bits     = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(all.constData() + dataStart + 14));
        } else if (id == "data") {
            if (dataStart + static_cast<int>(size) > all.size()) {
                error = QStringLiteral("WAV 数据块不完整");
                return false;
            }
            pcm = all.mid(dataStart, static_cast<int>(size));
            break;
        }
        pos = dataStart + static_cast<int>(size) + (static_cast<int>(size) & 1);
    }

    if (pcm.isEmpty()) {
        error = QStringLiteral("WAV 里没有音频数据");
        return false;
    }
    if (channels != 1 || rate != 16000 || bits != 16) {
        error = QStringLiteral("只支持 16 kHz / 单声道 / 16-bit 的 WAV（当前 %1 Hz / %2 声道 / %3 bit）")
                    .arg(rate).arg(channels).arg(bits);
        return false;
    }
    return true;
}

}  // namespace

VoiceRecorder::VoiceRecorder(QObject* parent) : QObject(parent) {
    tick_ = new QTimer(this);
    tick_->setInterval(200);
    connect(tick_, &QTimer::timeout, this, [this] {
        if (!recording_) return;
        if (elapsed_.elapsed() >= kMaxRecordMs) {
            stopRecording();
        }
    });
}

VoiceRecorder::~VoiceRecorder() {
    if (thread_ != nullptr) {
        thread_->quit();
        thread_->wait(3000);
    }
}

void VoiceRecorder::start() {
    if (worker_ != nullptr) return;

    thread_ = new QThread(this);
    thread_->setObjectName(QStringLiteral("whisper"));

    worker_ = new WhisperWorker(modelPath_);
    worker_->moveToThread(thread_);

    connect(thread_, &QThread::finished, worker_, &QObject::deleteLater);

    connect(worker_, &WhisperWorker::initialized, this, [this](bool ok, const QString& message) {
        modelReady_ = ok;
        emit modelLoaded(ok, message);
    });
    connect(worker_, &WhisperWorker::finished, this, [this](const QString& text, qint64 ms) {
        busy_ = false;
        emit textReady(text, ms);
    });
    connect(worker_, &WhisperWorker::failed, this, [this](const QString& message) {
        busy_ = false;
        emit failed(message);
    });

    thread_->start();
    QMetaObject::invokeMethod(worker_, "initialize", Qt::QueuedConnection);
}

void VoiceRecorder::startRecording(Purpose purpose) {
    if (recording_) return;

    // 发送模式不需要 whisper 模型，也不受「上一句还在识别」限制
    if (purpose == Purpose::Transcribe) {
        if (!modelReady_) {
            emit failed(QStringLiteral("语音模型还在加载，请稍候再试"));
            return;
        }
        if (busy_) {
            emit failed(QStringLiteral("上一句还在识别中，请稍候"));
            return;
        }
    }
    purpose_ = purpose;

    const QAudioDevice device = QMediaDevices::defaultAudioInput();
    if (device.isNull()) {
        emit failed(QStringLiteral("没有检测到麦克风"));
        return;
    }

    QAudioFormat format;
    format.setSampleRate(16000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);

    if (!device.isFormatSupported(format)) {
        emit failed(QStringLiteral("麦克风不支持 16 kHz 单声道采集"));
        return;
    }

    source_ = new QAudioSource(device, format, this);
    source_->setBufferSize(16000 * 2 / 5);   // 约 200 ms

    device_ = source_->start();
    if (device_ == nullptr) {
        source_->deleteLater();
        source_ = nullptr;
        emit failed(QStringLiteral("麦克风启动失败"));
        return;
    }
    connect(device_, &QIODevice::readyRead, this, &VoiceRecorder::onAudioReady);

    pcm_.clear();
    recording_ = true;
    elapsed_.start();
    tick_->start();
    emit recordingStarted();
}

void VoiceRecorder::onAudioReady() {
    if (device_ == nullptr) return;

    const QByteArray chunk = device_->readAll();
    if (chunk.isEmpty()) return;
    pcm_.append(chunk);

    // 供 UI 显示电平：RMS 映射到 0..100
    const auto* samples = reinterpret_cast<const qint16*>(chunk.constData());
    const int count = chunk.size() / 2;
    double sum = 0.0;
    for (int i = 0; i < count; ++i) {
        const double v = static_cast<double>(samples[i]);
        sum += v * v;
    }
    const double rms = count > 0 ? std::sqrt(sum / count) : 0.0;
    const int level = static_cast<int>(std::min(100.0, rms / 32768.0 * 400.0));

    emit recordingTick(static_cast<int>(elapsed_.elapsed() / 1000), level);
}

void VoiceRecorder::stopRecording() {
    if (!recording_) return;
    recording_ = false;
    tick_->stop();

    if (device_ != nullptr) {
        pcm_.append(device_->readAll());   // 收尾：把内核缓冲里剩下的读干净
        device_ = nullptr;
    }
    if (source_ != nullptr) {
        source_->stop();
        source_->deleteLater();
        source_ = nullptr;
    }

    if (pcm_.size() < 3200) {
        emit failed(QStringLiteral("录音太短，没有说话"));
        return;
    }

    if (purpose_ == Purpose::Send) {
        // 语音消息：直接把原始 PCM 交出去编码发送，不走识别
        const int durationMs = static_cast<int>(pcm_.size() / 2) * 1000 / 16000;
        const QByteArray captured = pcm_;
        pcm_.clear();
        emit pcmReady(captured, durationMs);
        return;
    }

    emit recognizing();
    submit(pcm_);
}

void VoiceRecorder::transcribeFile(const QString& wavPath) {
    if (!modelReady_) {
        emit failed(QStringLiteral("语音模型还在加载，请稍候再试"));
        return;
    }
    if (busy_) {
        emit failed(QStringLiteral("上一句还在识别中，请稍候"));
        return;
    }

    QByteArray pcm;
    QString error;
    if (!loadWavPcm16(wavPath, pcm, error)) {
        emit failed(error);
        return;
    }

    emit recognizing();
    submit(pcm);
}

void VoiceRecorder::submit(const QByteArray& pcm) {
    if (worker_ == nullptr) {
        emit failed(QStringLiteral("语音识别线程未启动"));
        return;
    }
    busy_ = true;
    QMetaObject::invokeMethod(worker_, "transcribe", Qt::QueuedConnection,
                              Q_ARG(QByteArray, pcm));
}
