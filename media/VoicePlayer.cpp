/*
 * 文件名:    VoicePlayer.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CVoicePlayer 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CVoicePlayer.cpp */
#include "media/VoicePlayer.h"

#include "media/VoiceCodec.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QUrl>

#include <cstring>

namespace {

/* 给 PCM 加 44 字节 WAV 头（QSoundEffect 只吃 WAV）。 */
/*
 * 功能描述:  包装wav。
 * 作者:      JaTin
 * param [in]   byPcm            输入数据，类型 const QByteArray&。
 * param [in]   nSampleRate      输入数据，类型 int。
 * param [in]   nChannels        输入数据，类型 int。
 * param [in]   nBitsPerSample   输入数据，类型 int。
 * return:    返回字节数据；失败时返回空 QByteArray。
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
    /* fmt 块长度 */
    stPut32(16);
    /* PCM */
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

/* namespace */
}

/*
 * 功能描述:  构造 CVoicePlayer 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject* parent) : QObject(。
 * return:    无返回值（构造函数或析构函数）。
 */
CVoicePlayer::CVoicePlayer(QObject* pParent) : QObject(pParent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
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

/*
 * 功能描述:  获取instance。
 * 作者:      JaTin
 * return:    成功返回对象指针；未找到或失败时返回 nullptr。
 */
CVoicePlayer* CVoicePlayer::instance() {
    static CVoicePlayer stPlayer;
    return &stPlayer;
}

/*
 * 功能描述:  获取wav 文件路径 for。
 * 作者:      JaTin
 * param [in]   byContainer      容器，类型 const QByteArray&。
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString CVoicePlayer::wavPathFor(const QByteArray& byContainer) const {
    const QString strDir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/voice");
    QDir().mkpath(strDir);

    const QByteArray byHash =
        QCryptographicHash::hash(byContainer, QCryptographicHash::Md5).toHex();
    return strDir + QLatin1Char('/') + QString::fromLatin1(byHash) + QStringLiteral(".wav");
}

/*
 * 功能描述:  播放play。
 * 作者:      JaTin
 * param [in]   byContainer      容器，类型 const QByteArray&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CVoicePlayer::play(const QByteArray& byContainer) {
    if (byContainer.isEmpty()) return;

    /* 同一条再点一次：先停再从头播（QSoundEffect 的 play 在播放中不会重头） */
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

/*
 * 功能描述:  停止stop。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CVoicePlayer::stop() {
    m_nPendingPlay = false;
    if (m_pEffect != nullptr) m_pEffect->stop();
    current_.clear();
}

/*
 * 功能描述:  判断playing。
 * 作者:      JaTin
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CVoicePlayer::isPlaying() const {
    return speaking_;
}
