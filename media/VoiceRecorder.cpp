/**
 * @file       VoiceRecorder.cpp
 * @brief      CVoiceRecorder 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoiceRecorder.cpp。
 */
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

/**
 * @brief  极简 WAV 解析：只接受 16 kHz / 单声道 / 16-bit，正好是 whisper 的输入要求。
 * @details 加载 wavpcm16；失败时返回 false。
 */
bool loadWavPcm16(const QString& strPath, QByteArray& byPcm, QString& strError) {
    QFile stFile(strPath);
    if (!stFile.open(QIODevice::ReadOnly)) {
        strError = QStringLiteral("无法打开文件：%1").arg(strPath);
        return false;
    }
    const QByteArray byAll = stFile.readAll();
    if (byAll.size() < 44 || !byAll.startsWith("RIFF")) {
        strError = QStringLiteral("不是合法的 WAV 文件");
        return false;
    }

    int nChannels = 0;
    int nRate = 0;
    int nBits = 0;
    int nPos = 12;

    while (nPos + 8 <= byAll.size()) {
        const QByteArray byId = byAll.mid(nPos, 4);
        const quint32 uSize = qFromLittleEndian<quint32>(
            reinterpret_cast<const uchar*>(byAll.constData() + nPos + 4));
        const int nDataStart = nPos + 8;

        if (byId == "fmt " && nDataStart + 16 <= byAll.size()) {
            nChannels = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(byAll.constData() + nDataStart + 2));
            nRate     = qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(byAll.constData() + nDataStart + 4));
            nBits     = qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(byAll.constData() + nDataStart + 14));
        } else if (byId == "data") {
            if (nDataStart + static_cast<int>(uSize) > byAll.size()) {
                strError = QStringLiteral("WAV 数据块不完整");
                return false;
            }
            byPcm = byAll.mid(nDataStart, static_cast<int>(uSize));
            break;
        }
        nPos = nDataStart + static_cast<int>(uSize) + (static_cast<int>(uSize) & 1);
    }

    if (byPcm.isEmpty()) {
        strError = QStringLiteral("WAV 里没有音频数据");
        return false;
    }
    if (nChannels != 1 || nRate != 16000 || nBits != 16) {
        strError = QStringLiteral("只支持 16 kHz / 单声道 / 16-bit 的 WAV（当前 %1 Hz / %2 声道 / %3 bit）")
                    .arg(nRate).arg(nChannels).arg(nBits);
        return false;
    }
    return true;
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CVoiceRecorder 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject* pParent) : QObject(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CVoiceRecorder::CVoiceRecorder(QObject* pParent) : QObject(pParent) {
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pTick = new QTimer(this);
    m_pTick->setInterval(200);
    connect(m_pTick, &QTimer::timeout, this, [this] {
        if (!m_nRecording) return;
        if (m_stElapsed.elapsed() >= kMaxRecordMs) {
            stopRecording();
        }
    });
}

/**
 * @brief      析构 CVoiceRecorder 对象，释放运行期持有的全部资源。
 * @note       本函数无输入参数。
 * @return     无返回值（构造函数或析构函数）。
 * @author     JaTin
 */
CVoiceRecorder::~CVoiceRecorder() {
    if (m_pThread != nullptr) {
        m_pThread->quit();
        m_pThread->wait(3000);
    }
}

/**
 * @brief      启动 start；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceRecorder::start() {
    if (m_pWorker != nullptr) return;

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
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

/**
 * @brief      启动 recording；实现上连接 Qt 信号与槽、创建 Qt 控件并纳入父子对象树。
 * @param[in] enPurpose      输入数据；类型 Purpose_EN。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceRecorder::startRecording(Purpose_EN enPurpose) {
    if (m_nRecording) return;

    /**
     * @brief  发送模式不需要 whisper 模型，也不受「上一句还在识别」限制。
     */
    if (enPurpose == Purpose_EN::Transcribe) {
        if (!m_nModelReady) {
            emit failed(QStringLiteral("语音模型还在加载，请稍候再试"));
            return;
        }
        if (m_nBusy) {
            emit failed(QStringLiteral("上一句还在识别中，请稍候"));
            return;
        }
    }
    m_stPurpose = enPurpose;

    const QAudioDevice stDevice = QMediaDevices::defaultAudioInput();
    if (stDevice.isNull()) {
        emit failed(QStringLiteral("没有检测到麦克风"));
        return;
    }

    QAudioFormat stFormat;
    stFormat.setSampleRate(16000);
    stFormat.setChannelCount(1);
    stFormat.setSampleFormat(QAudioFormat::Int16);

    if (!stDevice.isFormatSupported(stFormat)) {
        emit failed(QStringLiteral("麦克风不支持 16 kHz 单声道采集"));
        return;
    }

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pSource = new QAudioSource(stDevice, stFormat, this);
    /**
     * @brief  约 200 ms。
     */
    m_pSource->setBufferSize(16000 * 2 / 5);

    m_pDevice = m_pSource->start();
    if (m_pDevice == nullptr) {
        m_pSource->deleteLater();
        m_pSource = nullptr;
        emit failed(QStringLiteral("麦克风启动失败"));
        return;
    }
    connect(m_pDevice, &QIODevice::readyRead, this, &CVoiceRecorder::onAudioReady);

    m_byPcm.clear();
    m_nRecording = true;
    m_stElapsed.start();
    m_pTick->start();
    emit recordingStarted();
}

/**
 * @brief      执行 on 音频数据 ready。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceRecorder::onAudioReady() {
    if (m_pDevice == nullptr) return;

    const QByteArray byChunk = m_pDevice->readAll();
    if (byChunk.isEmpty()) return;
    m_byPcm.append(byChunk);

    /**
     * @brief  供 UI 显示电平：RMS 映射到 0..100。
     */
    const auto* pSamples = reinterpret_cast<const qint16*>(byChunk.constData());
    const int nCount = byChunk.size() / 2;
    double dSum = 0.0;
    for (int i = 0; i < nCount; ++i) {
        const double dV = static_cast<double>(pSamples[i]);
        dSum += dV * dV;
    }
    const double dRms = nCount > 0 ? std::sqrt(dSum / nCount) : 0.0;
    const int nLevel = static_cast<int>(std::min(100.0, dRms / 32768.0 * 400.0));

    emit recordingTick(static_cast<int>(m_stElapsed.elapsed() / 1000), nLevel);
}

/**
 * @brief      停止 recording。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceRecorder::stopRecording() {
    if (!m_nRecording) return;
    m_nRecording = false;
    m_pTick->stop();

    if (m_pDevice != nullptr) {
        /**
         * @brief  收尾：把内核缓冲里剩下的读干净。
         */
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
        /**
         * @brief  语音消息：直接把原始 PCM 交出去编码发送，不走识别。
         */
        const int nDurationMs = static_cast<int>(m_byPcm.size() / 2) * 1000 / 16000;
        const QByteArray byCaptured = m_byPcm;
        m_byPcm.clear();
        emit pcmReady(byCaptured, nDurationMs);
        return;
    }

    emit recognizing();
    submit(m_byPcm);
}

/**
 * @brief      转写文件。
 * @param[in] strWavPath     输入数据；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceRecorder::transcribeFile(const QString& strWavPath) {
    if (!m_nModelReady) {
        emit failed(QStringLiteral("语音模型还在加载，请稍候再试"));
        return;
    }
    if (m_nBusy) {
        emit failed(QStringLiteral("上一句还在识别中，请稍候"));
        return;
    }

    QByteArray byPcm;
    QString strError;
    if (!loadWavPcm16(strWavPath, byPcm, strError)) {
        emit failed(strError);
        return;
    }

    emit recognizing();
    submit(byPcm);
}

/**
 * @brief      提交 submit。
 * @param[in] byPcm          输入数据；类型 const QByteArray&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceRecorder::submit(const QByteArray& byPcm) {
    if (m_pWorker == nullptr) {
        emit failed(QStringLiteral("语音识别线程未启动"));
        return;
    }
    m_nBusy = true;
    QMetaObject::invokeMethod(m_pWorker, "transcribe", Qt::QueuedConnection,
                              Q_ARG(QByteArray, byPcm));
}
