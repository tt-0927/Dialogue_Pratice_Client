/**
 * @file       WhisperWorker.cpp
 * @brief      CWhisperWorker 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CWhisperWorker.cpp。
 */
#include "media/WhisperWorker.h"

#ifdef DP_WITH_WHISPER

#include <QElapsedTimer>
#include <QFileInfo>
#include <QThread>
#include <QtDebug>

#include <algorithm>
#include <vector>

#include "whisper.h"

/**
 * @brief      构造 CWhisperWorker 对象，完成成员初始化与依赖注入。
 * @param[in] strModelPath   输入数据；类型 QString。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CWhisperWorker::CWhisperWorker(QString strModelPath, QObject* pParent)
    : QObject(pParent), m_strModelPath(std::move(strModelPath)) {}

/**
 * @brief      析构 CWhisperWorker 对象，释放运行期持有的全部资源。
 * @note       本函数无输入参数。
 * @return     无返回值（构造函数或析构函数）。
 * @author     JaTin
 */
CWhisperWorker::~CWhisperWorker() {
    if (m_pCtx != nullptr) {
        whisper_free(m_pCtx);
        m_pCtx = nullptr;
    }
}

/**
 * @brief      初始化 initialize；实现上调用本地 whisper 完成语音转写。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

    whisper_context_params stParams = whisper_context_default_params();
    /**
     * @brief  虚拟机无 GPU；纯 CPU 推理足够应付短句。
     */
    stParams.use_gpu = false;

    QElapsedTimer stTimer;
    stTimer.start();
    m_pCtx = whisper_init_from_file_with_params(m_strModelPath.toUtf8().constData(), stParams);
    if (m_pCtx == nullptr) {
        emit initialized(false, QStringLiteral("语音模型加载失败：%1").arg(m_strModelPath));
        return;
    }

    qInfo() << "whisper model loaded in" << stTimer.elapsed() << "ms:" << m_strModelPath;
    emit initialized(true, QString());
}

/**
 * @brief      转写 transcribe；实现上调用本地 whisper 完成语音转写。
 * @param[in] byPcm          输入数据；类型 const QByteArray&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CWhisperWorker::transcribe(const QByteArray& byPcm) {
    if (m_pCtx == nullptr) {
        emit failed(QStringLiteral("语音模型尚未就绪"));
        return;
    }
    /**
     * @brief  < 0.1 秒，没必要送进模型。
     */
    if (byPcm.size() < 3200) {
        emit failed(QStringLiteral("录音太短，没有识别到语音"));
        return;
    }

    /**
     * @brief  int16 -> float [-1, 1]，whisper 只吃 float PCM。
     */
    const int nSampleCount = byPcm.size() / 2;
    std::vector<float> fSamples(static_cast<size_t>(nSampleCount));
    const auto* pSrc = reinterpret_cast<const int16_t*>(byPcm.constData());
    for (int i = 0; i < nSampleCount; ++i) {
        fSamples[static_cast<size_t>(i)] = static_cast<float>(pSrc[i]) / 32768.0f;
    }

    whisper_full_params stP = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    stP.print_progress   = false;
    stP.print_special    = false;
    stP.print_realtime   = false;
    stP.print_timestamps = false;
    stP.translate        = false;
    /**
     * @brief  英语练习场景，锁死英文可减少误判。
     */
    stP.language         = "en";
    /**
     * @brief  每句独立，不继承上一句上下文。
     */
    stP.no_context       = true;
    stP.single_segment   = false;
    stP.n_threads        = std::max(1, QThread::idealThreadCount() - 1);

    QElapsedTimer stTimer;
    stTimer.start();
    const int nRc = whisper_full(m_pCtx, stP, fSamples.data(), nSampleCount);
    const qint64 lElapsed = stTimer.elapsed();

    if (nRc != 0) {
        emit failed(QStringLiteral("语音识别失败（whisper_full 返回 %1）").arg(nRc));
        return;
    }

    QString strText;
    const int nSegments = whisper_full_n_segments(m_pCtx);
    for (int i = 0; i < nSegments; ++i) {
        strText += QString::fromUtf8(whisper_full_get_segment_text(m_pCtx, i));
    }
    strText = strText.trimmed();

    /**
     * @brief  静音时 whisper 会输出 [BLANK_AUDIO] / [ Silence ] 这类标记。
     */
    if (strText.isEmpty() || strText.startsWith(QLatin1Char('[')) || strText == QLatin1String("()")) {
        emit failed(QStringLiteral("没有识别到语音，请靠近麦克风重试"));
        return;
    }

    emit finished(strText, lElapsed);
}

/**
 * @brief  !DP_WITH_WHISPER。
 */
#else

/**
 * @brief  本平台未编译 whisper.cpp（Android 交叉编译用 -DDP_WITH_WHISPER=OFF）。 接口签名保持一致，初始化直接失败，调用方按「语音输入不可用」降级， 文字聊天与语音消息收发不受影响。
 */

#include <utility>

/**
 * @brief      构造 CWhisperWorker 对象，完成成员初始化与依赖注入。
 * @param[in] strModelPath   输入数据；类型 QString。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CWhisperWorker::CWhisperWorker(QString strModelPath, QObject* pParent)
    : QObject(pParent), m_strModelPath(std::move(strModelPath)) {}

CWhisperWorker::~CWhisperWorker() = default;

/**
 * @brief      初始化 initialize。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CWhisperWorker::initialize() {
    emit initialized(false, QStringLiteral("本平台未编译语音识别（whisper.cpp）"));
}

/**
 * @brief      转写 transcribe。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CWhisperWorker::transcribe(const QByteArray&) {
    emit failed(QStringLiteral("本平台未编译语音识别（whisper.cpp）"));
}

/**
 * @brief  DP_WITH_WHISPER。
 */
#endif
