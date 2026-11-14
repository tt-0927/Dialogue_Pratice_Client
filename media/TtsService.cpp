/**
 * @file       TtsService.cpp
 * @brief      CTtsService 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CTtsService.cpp。
 */
#include "media/TtsService.h"

#ifdef DP_WITH_TTS

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QUrl>
#include <QtDebug>

namespace {
/**
 * @brief  小于这个值说明是半成品
 * @note   取值 44。
 */
constexpr qint64 kWavHeaderSize = 44;
}

/**
 * @brief      构造 CTtsService 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject* pParent) : QObject(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CTtsService::CTtsService(QObject* pParent) : QObject(pParent) {
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pEffect = new QSoundEffect(this);
    m_pEffect->setVolume(1.0);

    connect(m_pEffect, &QSoundEffect::statusChanged, this, [this] {
        if (m_pEffect->status() == QSoundEffect::Error) {
            m_nPendingPlay = false;
            emit failed(QStringLiteral("音频加载失败"));
            return;
        }
        /**
         * @brief  setSource 是异步加载，加载完才能 play。
         */
        if (m_nPendingPlay && m_pEffect->status() == QSoundEffect::Ready) {
            m_nPendingPlay = false;
            m_pEffect->play();
        }
    });

    connect(m_pEffect, &QSoundEffect::playingChanged, this, [this] {
        const bool nPlaying = m_pEffect->isPlaying();
        if (speaking_ != nPlaying) {
            speaking_ = nPlaying;
            emit speakingChanged(nPlaying);
        }
    });
}

/**
 * @brief      析构 CTtsService 对象，释放运行期持有的全部资源。
 * @note       本函数无输入参数。
 * @return     无返回值（构造函数或析构函数）。
 * @author     JaTin
 */
CTtsService::~CTtsService() {
    if (m_pProcess != nullptr) {
        m_pProcess->kill();
        m_pProcess->waitForFinished(1000);
    }
    cleanupPartFile();
}

/**
 * @brief      设置 paths相关的数据并返回处理结果。
 * @param[in] strPiperBin    输入数据；类型 const QString&。
 * @param[in] strVoiceModel  输入数据；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::setPaths(const QString& strPiperBin, const QString& strVoiceModel) {
    m_strPiperBin = strPiperBin;
    m_strVoiceModel = strVoiceModel;
}

/**
 * @brief      判断 available相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool CTtsService::isAvailable() const {
    return !m_strPiperBin.isEmpty() && QFileInfo::exists(m_strPiperBin)
           && QFileInfo::exists(m_strVoiceModel);
}

/**
 * @brief      获取缓存文件路径 for。
 * @param[in] strText        消息文本；类型 const QString&。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString CTtsService::cachePathFor(const QString& strText) const {
    const QString strDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tts");
    QDir().mkpath(strDir);

    const QByteArray byHash =
        QCryptographicHash::hash(strText.toUtf8(), QCryptographicHash::Md5).toHex();
    return strDir + QLatin1Char('/') + QString::fromLatin1(byHash) + QStringLiteral(".wav");
}

/**
 * @brief      调用本地语音合成把文本转换为音频并播放。
 * @param[in] strText        消息文本；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::speak(const QString& strText) {
    const QString strTrimmed = strText.trimmed();
    if (strTrimmed.isEmpty()) return;

    if (!isAvailable()) {
        emit failed(QStringLiteral("未安装 Piper（运行 scripts/setup_tts.sh）"));
        return;
    }

    /**
     * @brief  停掉上一段，也清掉正在合成的半成品。
     */
    stop();

    m_strCurrentText = strTrimmed;

    const auto stHit = m_strCache.constFind(strTrimmed);
    if (stHit != m_strCache.constEnd() && QFileInfo::exists(stHit.value())) {
        playFile(stHit.value());
        return;
    }

    const QString strWav = cachePathFor(strTrimmed);
    /**
     * @brief  大小要过 WAV 头：历史版本可能留下被 kill 截断的半成品文件。
     */
    if (QFileInfo::exists(strWav) && QFileInfo(strWav).size() > kWavHeaderSize) {
        m_strCache.insert(strTrimmed, strWav);
        playFile(strWav);
        return;
    }

    m_strPendingPart = strWav + QStringLiteral(".part");
    m_strPendingFinal = strWav;
    m_strPendingText = strTrimmed;

    if (m_pProcess == nullptr) {
        /**
         * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
         */
        m_pProcess = new QProcess(this);
        /**
         * @brief  piper 需要同目录下的 espeak-ng-data，工作目录设成二进制所在目录。
         */
        m_pProcess->setWorkingDirectory(QFileInfo(m_strPiperBin).absolutePath());

        connect(m_pProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
            /**
             * @brief  被 stop() 主动打断：pendingPart_ 已被清空，不报错。
             */
            if (m_strPendingPart.isEmpty()) return;

            if (status != QProcess::NormalExit || code != 0) {
                cleanupPartFile();
                emit failed(QStringLiteral("语音合成失败（piper 退出码 %1）").arg(code));
                return;
            }
            if (!QFileInfo::exists(m_strPendingPart)
                || QFileInfo(m_strPendingPart).size() <= kWavHeaderSize) {
                cleanupPartFile();
                emit failed(QStringLiteral("语音合成没有生成有效文件"));
                return;
            }

            /**
             * @brief  原子替换：先写 .part，成功后才改名成正式缓存。 否则合成中途被打断会留下半个 WAV，下次被当成缓存命中，播出来只有半秒。
             */
            QFile::remove(m_strPendingFinal);
            if (!QFile::rename(m_strPendingPart, m_strPendingFinal)) {
                cleanupPartFile();
                emit failed(QStringLiteral("无法写入语音缓存"));
                return;
            }

            const QString strWav = m_strPendingFinal;
            m_strCache.insert(m_strPendingText, strWav);
            m_strPendingPart.clear();
            m_strPendingFinal.clear();
            playFile(strWav);
        });
    }

    m_pProcess->start(m_strPiperBin, {QStringLiteral("-m"), m_strVoiceModel,
                                QStringLiteral("-f"), m_strPendingPart,
                                QStringLiteral("--length-scale"), QStringLiteral("1.05")});
    if (!m_pProcess->waitForStarted(3000)) {
        cleanupPartFile();
        emit failed(QStringLiteral("无法启动 piper"));
        return;
    }
    m_pProcess->write(strTrimmed.toUtf8());
    m_pProcess->closeWriteChannel();
}

/**
 * @brief      播放文件。
 * @param[in] strPath        文件路径；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::playFile(const QString& strPath) {
    if (m_strLoadedPath != strPath) {
        /**
         * @brief  换文件：setSource 是异步的，等 Ready 再播。
         */
        m_strLoadedPath = strPath;
        m_nPendingPlay = true;
        m_pEffect->setSource(QUrl::fromLocalFile(strPath));
        if (m_pEffect->status() == QSoundEffect::Ready) {
            m_nPendingPlay = false;
            m_pEffect->play();
        }
        return;
    }

    /**
     * @brief  同一个文件再点：play() 在播放中不会重头，必须先 stop。
     */
    m_pEffect->stop();
    m_pEffect->play();
}

/**
 * @brief      停止 stop。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::stop() {
    m_nPendingPlay = false;
    if (m_pEffect != nullptr) m_pEffect->stop();

    /**
     * @brief  先清空 pendingPart_ 再 kill，让 finished 回调知道这是主动取消。
     */
    const QString strPart = m_strPendingPart;
    m_strPendingPart.clear();
    m_strPendingFinal.clear();

    if (m_pProcess != nullptr && m_pProcess->state() != QProcess::NotRunning) {
        m_pProcess->kill();
        m_pProcess->waitForFinished(500);
    }
    if (!strPart.isEmpty()) QFile::remove(strPart);
}

/**
 * @brief      执行 cleanuppart 文件。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::cleanupPartFile() {
    if (!m_strPendingPart.isEmpty()) {
        QFile::remove(m_strPendingPart);
        m_strPendingPart.clear();
    }
}

/**
 * @brief  !DP_WITH_TTS。
 */
#else

/**
 * @brief  本平台未编译 Piper TTS（Android 交叉编译用 -DDP_WITH_TTS=OFF）。 isAvailable() 恒为 false，UI 自动隐藏朗读按钮；接口签名保持一致，。
 * @details 构造 CTtsService 对象，完成成员初始化与依赖注入。
 */

CTtsService::CTtsService(QObject* pParent) : QObject(pParent) {}

CTtsService::~CTtsService() = default;

/**
 * @brief      设置 paths相关的数据并返回处理结果。
 * @param[in] strPiperBin    输入数据；类型 const QString&。
 * @param[in] strVoiceModel  输入数据；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::setPaths(const QString& strPiperBin, const QString& strVoiceModel) {
    m_strPiperBin = strPiperBin;
    m_strVoiceModel = strVoiceModel;
}

/**
 * @brief      判断 available；失败时返回 false。
 * @note       本函数无输入参数。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool CTtsService::isAvailable() const { return false; }

/**
 * @brief      调用本地语音合成把文本转换为音频并播放。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::speak(const QString&) {
    emit failed(QStringLiteral("本平台未编译语音合成（Piper）"));
}

/**
 * @brief      停止 stop。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::stop() {}

/**
 * @brief      播放文件。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::playFile(const QString&) {}

/**
 * @brief      获取缓存文件路径 for。
 * @note       本函数无输入参数。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString CTtsService::cachePathFor(const QString&) const { return QString(); }

/**
 * @brief      执行 cleanuppart 文件。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CTtsService::cleanupPartFile() {}

/**
 * @brief  DP_WITH_TTS。
 */
#endif
