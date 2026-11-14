/**
 * @file       VoicePlayer.cpp
 * @brief      CVoicePlayer 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoicePlayer.cpp。
 */
#include "VoicePlayer.h"

#include "VoiceCodec.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QUrl>

#include <cstring>

namespace {

/**
 * @brief  给 PCM 加 44 字节 WAV 头（QSoundEffect 只吃 WAV）。
 * @details 包装 wav。
 */
QByteArray wrapWav(const QByteArray& byPcm, int nSampleRate, int nChannels, int nBitsPerSample) {
    const quint32 uDataSize = static_cast<quint32>(byPcm.size());
    const quint16 uBlockAlign = static_cast<quint16>(nChannels * nBitsPerSample / 8);
    const quint32 uByteRate = static_cast<quint32>(nSampleRate) * uBlockAlign;
    const quint32 uRiffSize = 36 + uDataSize;

    QByteArray byWav;
    byWav.reserve(44 + byPcm.size());

    auto stPut32 = [&byWav](quint32 v) {
        byWav.append(static_cast<char>(v & 0xFF));
        byWav.append(static_cast<char>((v >> 8) & 0xFF));
        byWav.append(static_cast<char>((v >> 16) & 0xFF));
        byWav.append(static_cast<char>((v >> 24) & 0xFF));
    };
    auto stPut16 = [&byWav](quint16 v) {
        byWav.append(static_cast<char>(v & 0xFF));
        byWav.append(static_cast<char>((v >> 8) & 0xFF));
    };

    byWav.append("RIFF", 4);
    stPut32(uRiffSize);
    byWav.append("WAVE", 4);
    byWav.append("fmt ", 4);
    /**
     * @brief  fmt 块长度。
     */
    stPut32(16);
    /**
     * @brief  PCM。
     */
    stPut16(1);
    stPut16(static_cast<quint16>(nChannels));
    stPut32(static_cast<quint32>(nSampleRate));
    stPut32(uByteRate);
    stPut16(uBlockAlign);
    stPut16(static_cast<quint16>(nBitsPerSample));
    byWav.append("data", 4);
    stPut32(uDataSize);
    byWav.append(byPcm);

    return byWav;
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CVoicePlayer 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject* pParent) : QObject(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CVoicePlayer::CVoicePlayer(QObject* pParent) : QObject(pParent) {
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pEffect = new QSoundEffect(this);
    m_pEffect->setVolume(1.0);

    connect(m_pEffect, &QSoundEffect::statusChanged, this, [this] {
        if (m_pEffect->status() == QSoundEffect::Error) {
            m_nPendingPlay = false;
            emit failed(QStringLiteral("语音加载失败"));
            return;
        }
        if (m_nPendingPlay && m_pEffect->status() == QSoundEffect::Ready) {
            m_nPendingPlay = false;
            m_pEffect->play();
        }
    });

    connect(m_pEffect, &QSoundEffect::playingChanged, this, [this] {
        const bool nPlaying = m_pEffect->isPlaying();
        if (speaking_ == nPlaying) return;
        speaking_ = nPlaying;
        if (!nPlaying) current_.clear();
        emit playingChanged(nPlaying);
    });
}

/**
 * @brief      获取本类的进程内单例对象。
 * @note       本函数无输入参数。
 * @return     成功返回对象指针；未找到或失败时返回 nullptr。
 * @author     JaTin
 */
CVoicePlayer* CVoicePlayer::instance() {
    static CVoicePlayer stPlayer;
    return &stPlayer;
}

/**
 * @brief      获取 wav 文件路径 for。
 * @param[in] byContainer    容器；类型 const QByteArray&。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString CVoicePlayer::wavPathFor(const QByteArray& byContainer) const {
    const QString strDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/voice");
    QDir().mkpath(strDir);

    const QByteArray byHash =
        QCryptographicHash::hash(byContainer, QCryptographicHash::Md5).toHex();
    return strDir + QLatin1Char('/') + QString::fromLatin1(byHash) + QStringLiteral(".wav");
}

/**
 * @brief      播放 play。
 * @param[in] byContainer    容器；类型 const QByteArray&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoicePlayer::play(const QByteArray& byContainer) {
    if (byContainer.isEmpty()) return;

    /**
     * @brief  同一条再点一次：先停再从头播（QSoundEffect 的 play 在播放中不会重头）。
     */
    if (current_ == byContainer && m_pEffect->status() == QSoundEffect::Ready) {
        m_pEffect->stop();
        m_pEffect->play();
        return;
    }

    stop();

    const QString strWav = wavPathFor(byContainer);
    if (!QFileInfo::exists(strWav) || QFileInfo(strWav).size() <= 44) {
        const QByteArray byPcm = CVoiceCodec::decode(byContainer);
        if (byPcm.isEmpty()) {
            emit failed(QStringLiteral("语音解码失败：%1").arg(CVoiceCodec::lastError()));
            return;
        }
        const QByteArray byPayload =
            wrapWav(byPcm, CVoiceCodec::kSampleRate, CVoiceCodec::kChannels, 16);

        QFile stFile(strWav);
        if (!stFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit failed(QStringLiteral("无法写入语音缓存"));
            return;
        }
        stFile.write(byPayload);
        stFile.close();
    }

    current_ = byContainer;
    m_nPendingPlay = true;
    m_pEffect->setSource(QUrl::fromLocalFile(strWav));
    if (m_pEffect->status() == QSoundEffect::Ready) {
        m_nPendingPlay = false;
        m_pEffect->play();
    }
}

/**
 * @brief      停止 stop。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoicePlayer::stop() {
    m_nPendingPlay = false;
    if (m_pEffect != nullptr) m_pEffect->stop();
    current_.clear();
}

/**
 * @brief      判断 playing相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool CVoicePlayer::isPlaying() const {
    return speaking_;
}
