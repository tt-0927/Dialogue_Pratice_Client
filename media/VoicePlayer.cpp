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
 * 功能描述:  执行 wrapWav 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  pcm  参数。
 * param [in]  sampleRate  参数。
 * param [in]  channels  参数。
 * param [in]  bitsPerSample  参数。
 * return:    返回值，含义见功能描述。
 */
QByteArray wrapWav(const QByteArray& pcm, int sampleRate, int channels, int bitsPerSample) {
    const quint32 dataSize = static_cast<quint32>(pcm.size());
    const quint16 blockAlign = static_cast<quint16>(channels * bitsPerSample / 8);
    const quint32 byteRate = static_cast<quint32>(sampleRate) * blockAlign;
    const quint32 riffSize = 36 + dataSize;

    QByteArray wav;
    wav.reserve(44 + pcm.size());

    auto put32 = [&wav](quint32 v) {
        wav.append(static_cast<char>(v & 0xFF));
        wav.append(static_cast<char>((v >> 8) & 0xFF));
        wav.append(static_cast<char>((v >> 16) & 0xFF));
        wav.append(static_cast<char>((v >> 24) & 0xFF));
    };
    auto put16 = [&wav](quint16 v) {
        wav.append(static_cast<char>(v & 0xFF));
        wav.append(static_cast<char>((v >> 8) & 0xFF));
    };

    wav.append("RIFF", 4);
    put32(riffSize);
    wav.append("WAVE", 4);
    wav.append("fmt ", 4);
    /* fmt 块长度 */
    put32(16);
    /* PCM */
    put16(1);
    put16(static_cast<quint16>(channels));
    put32(static_cast<quint32>(sampleRate));
    put32(byteRate);
    put16(blockAlign);
    put16(static_cast<quint16>(bitsPerSample));
    wav.append("data", 4);
    put32(dataSize);
    wav.append(pcm);

    return wav;
}

/* namespace */
}

/*
 * 功能描述:  执行 CVoicePlayer 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CVoicePlayer::CVoicePlayer(QObject* parent) : QObject(parent) {
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
        const bool playing = m_pEffect->isPlaying();
        if (speaking_ == playing) return;
        speaking_ = playing;
        if (!playing) current_.clear();
        emit playingChanged(playing);
    });
}

/*
 * 功能描述:  执行 instance 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
CVoicePlayer* CVoicePlayer::instance() {
    static CVoicePlayer player;
    return &player;
}

/*
 * 功能描述:  执行 wavPathFor 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  container  参数。
 * return:    返回值，含义见功能描述。
 */
QString CVoicePlayer::wavPathFor(const QByteArray& container) const {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/voice");
    QDir().mkpath(dir);

    const QByteArray hash =
        QCryptographicHash::hash(container, QCryptographicHash::Md5).toHex();
    return dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".wav");
}

/*
 * 功能描述:  执行 play 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  container  参数。
 * return:    void
 */
void CVoicePlayer::play(const QByteArray& container) {
    if (container.isEmpty()) return;

    /* 同一条再点一次：先停再从头播（QSoundEffect 的 play 在播放中不会重头） */
    if (current_ == container && m_pEffect->status() == QSoundEffect::Ready) {
        m_pEffect->stop();
        m_pEffect->play();
        return;
    }

    stop();

    const QString wav = wavPathFor(container);
    if (!QFileInfo::exists(wav) || QFileInfo(wav).size() <= 44) {
        const QByteArray pcm = CVoiceCodec::decode(container);
        if (pcm.isEmpty()) {
            emit failed(QStringLiteral("语音解码失败：%1").arg(CVoiceCodec::lastError()));
            return;
        }
        const QByteArray payload =
            wrapWav(pcm, CVoiceCodec::kSampleRate, CVoiceCodec::kChannels, 16);

        QFile file(wav);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit failed(QStringLiteral("无法写入语音缓存"));
            return;
        }
        file.write(payload);
        file.close();
    }

    current_ = container;
    m_nPendingPlay = true;
    m_pEffect->setSource(QUrl::fromLocalFile(wav));
    if (m_pEffect->status() == QSoundEffect::Ready) {
        m_nPendingPlay = false;
        m_pEffect->play();
    }
}

/*
 * 功能描述:  执行 stop 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CVoicePlayer::stop() {
    m_nPendingPlay = false;
    if (m_pEffect != nullptr) m_pEffect->stop();
    current_.clear();
}

/*
 * 功能描述:  执行 isPlaying 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool CVoicePlayer::isPlaying() const {
    return speaking_;
}
