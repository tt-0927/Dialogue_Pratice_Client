/*
 * 文件名:    VoiceInput.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CVoiceInput 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CVoiceInput.cpp */
#include "VoiceInput.h"

#include "WhisperWorker.h"

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

/* 极简 WAV 解析：只接受 16 kHz / 单声道 / 16-bit，正好是 whisper 的输入要求。 */
/*
 * 功能描述:  执行 loadWavPcm16 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  path  参数。
 * param [out]  pcm  参数。
 * param [out]  error  参数。
 * return:    返回值，含义见功能描述。
 */
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

/* namespace */
}

/*
 * 功能描述:  执行 CVoiceInput 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CVoiceInput::CVoiceInput(QObject* parent) : QObject(parent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pTick = new QTimer(this);
    m_pTick->setInterval(200);
    connect(m_pTick, &QTimer::timeout, this, [this] {
        if (!m_nRecording) return;
        if (m_stElapsed.elapsed() >= kMaxRecordMs) {
            stopRecording();
        }
    });
}

/*
 * 功能描述:  执行 CVoiceInput 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
CVoiceInput::~CVoiceInput() {
    if (m_pThread != nullptr) {
        m_pThread->quit();
        m_pThread->wait(3000);
    }
}

/*
 * 功能描述:  执行 start 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CVoiceInput::start() {
    if (m_pWorker != nullptr) return;

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pThread = new QThread(this);
    m_pThread->setObjectName(QStringLiteral("whisper"));

    m_pWorker = new CWhisperWorker(m_strModelPath);
    m_pWorker->moveToThread(m_pThread);

    connect(m_pThread, &QThread::finished, m_pWorker, &QObject::deleteLater);

    connect(m_pWorker, &CWhisperWorker::initialized, this, [this](bool ok, const QString& message) {
        m_nModelReady = ok;
        emit modelLoaded(ok, message);
    });
    connect(m_pWorker, &CWhisperWorker::finished, this, [this](const QString& text, qint64 ms) {
        m_nBusy = false;
        emit textReady(text, ms);
    });
    connect(m_pWorker, &CWhisperWorker::failed, this, [this](const QString& message) {
        m_nBusy = false;
        emit failed(message);
    });

    m_pThread->start();
    QMetaObject::invokeMethod(m_pWorker, "initialize", Qt::QueuedConnection);
}

/*
 * 功能描述:  执行 startRecording 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  purpose  参数。
 * return:    void
 */
void CVoiceInput::startRecording(Purpose_EN purpose) {
    if (m_nRecording) return;

    /* 发送模式不需要 whisper 模型，也不受「上一句还在识别」限制 */
    if (purpose == Purpose_EN::Transcribe) {
        if (!m_nModelReady) {
            emit failed(QStringLiteral("语音模型还在加载，请稍候再试"));
            return;
        }
        if (m_nBusy) {
            emit failed(QStringLiteral("上一句还在识别中，请稍候"));
            return;
        }
    }
    m_stPurpose = purpose;

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

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pSource = new QAudioSource(device, format, this);
    /* 约 200 ms */
    m_pSource->setBufferSize(16000 * 2 / 5);

    m_pDevice = m_pSource->start();
    if (m_pDevice == nullptr) {
        m_pSource->deleteLater();
        m_pSource = nullptr;
        emit failed(QStringLiteral("麦克风启动失败"));
        return;
    }
    connect(m_pDevice, &QIODevice::readyRead, this, &CVoiceInput::onAudioReady);

    m_byPcm.clear();
    m_nRecording = true;
    m_stElapsed.start();
    m_pTick->start();
    emit recordingStarted();
}

/*
 * 功能描述:  执行 onAudioReady 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CVoiceInput::onAudioReady() {
    if (m_pDevice == nullptr) return;

    const QByteArray chunk = m_pDevice->readAll();
    if (chunk.isEmpty()) return;
    m_byPcm.append(chunk);

    /* 供 UI 显示电平：RMS 映射到 0..100 */
    const auto* samples = reinterpret_cast<const qint16*>(chunk.constData());
    const int count = chunk.size() / 2;
    double sum = 0.0;
    for (int i = 0; i < count; ++i) {
        const double v = static_cast<double>(samples[i]);
        sum += v * v;
    }
    const double rms = count > 0 ? std::sqrt(sum / count) : 0.0;
    const int level = static_cast<int>(std::min(100.0, rms / 32768.0 * 400.0));

    emit recordingTick(static_cast<int>(m_stElapsed.elapsed() / 1000), level);
}

/*
 * 功能描述:  执行 stopRecording 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CVoiceInput::stopRecording() {
    if (!m_nRecording) return;
    m_nRecording = false;
    m_pTick->stop();

    if (m_pDevice != nullptr) {
        /* 收尾：把内核缓冲里剩下的读干净 */
        m_byPcm.append(m_pDevice->readAll());
        m_pDevice = nullptr;
    }
    if (m_pSource != nullptr) {
        m_pSource->stop();
        m_pSource->deleteLater();
        m_pSource = nullptr;
    }

    if (m_byPcm.size() < 3200) {
        emit failed(QStringLiteral("录音太短，没有说话"));
        return;
    }

    if (m_stPurpose == Purpose_EN::Send) {
        /* 语音消息：直接把原始 PCM 交出去编码发送，不走识别 */
        const int durationMs = static_cast<int>(m_byPcm.size() / 2) * 1000 / 16000;
        const QByteArray captured = m_byPcm;
        m_byPcm.clear();
        emit pcmReady(captured, durationMs);
        return;
    }

    emit recognizing();
    submit(m_byPcm);
}

/*
 * 功能描述:  执行 transcribeFile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  wavPath  参数。
 * return:    void
 */
void CVoiceInput::transcribeFile(const QString& wavPath) {
    if (!m_nModelReady) {
        emit failed(QStringLiteral("语音模型还在加载，请稍候再试"));
        return;
    }
    if (m_nBusy) {
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

/*
 * 功能描述:  执行 submit 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  pcm  参数。
 * return:    void
 */
void CVoiceInput::submit(const QByteArray& pcm) {
    if (m_pWorker == nullptr) {
        emit failed(QStringLiteral("语音识别线程未启动"));
        return;
    }
    m_nBusy = true;
    QMetaObject::invokeMethod(m_pWorker, "transcribe", Qt::QueuedConnection,
                              Q_ARG(QByteArray, pcm));
}
