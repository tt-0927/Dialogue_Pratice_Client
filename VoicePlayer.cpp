// VoicePlayer.cpp
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

/// 给 PCM 加 44 字节 WAV 头（QSoundEffect 只吃 WAV）。
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
    put32(16);                                          // fmt 块长度
    put16(1);                                           // PCM
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

}  // namespace

VoicePlayer::VoicePlayer(QObject* parent) : QObject(parent) {
    effect_ = new QSoundEffect(this);
    effect_->setVolume(1.0);

    connect(effect_, &QSoundEffect::statusChanged, this, [this] {
        if (effect_->status() == QSoundEffect::Error) {
            pendingPlay_ = false;
            emit failed(QStringLiteral("语音加载失败"));
            return;
        }
        if (pendingPlay_ && effect_->status() == QSoundEffect::Ready) {
            pendingPlay_ = false;
            effect_->play();
        }
    });

    connect(effect_, &QSoundEffect::playingChanged, this, [this] {
        const bool playing = effect_->isPlaying();
        if (speaking_ == playing) return;
        speaking_ = playing;
        if (!playing) current_.clear();
        emit playingChanged(playing);
    });
}

VoicePlayer* VoicePlayer::instance() {
    static VoicePlayer player;
    return &player;
}

QString VoicePlayer::wavPathFor(const QByteArray& container) const {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/voice");
    QDir().mkpath(dir);

    const QByteArray hash =
        QCryptographicHash::hash(container, QCryptographicHash::Md5).toHex();
    return dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".wav");
}

void VoicePlayer::play(const QByteArray& container) {
    if (container.isEmpty()) return;

    // 同一条再点一次：先停再从头播（QSoundEffect 的 play 在播放中不会重头）
    if (current_ == container && effect_->status() == QSoundEffect::Ready) {
        effect_->stop();
        effect_->play();
        return;
    }

    stop();

    const QString wav = wavPathFor(container);
    if (!QFileInfo::exists(wav) || QFileInfo(wav).size() <= 44) {
        const QByteArray pcm = VoiceCodec::decode(container);
        if (pcm.isEmpty()) {
            emit failed(QStringLiteral("语音解码失败：%1").arg(VoiceCodec::lastError()));
            return;
        }
        const QByteArray payload =
            wrapWav(pcm, VoiceCodec::kSampleRate, VoiceCodec::kChannels, 16);

        QFile file(wav);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit failed(QStringLiteral("无法写入语音缓存"));
            return;
        }
        file.write(payload);
        file.close();
    }

    current_ = container;
    pendingPlay_ = true;
    effect_->setSource(QUrl::fromLocalFile(wav));
    if (effect_->status() == QSoundEffect::Ready) {
        pendingPlay_ = false;
        effect_->play();
    }
}

void VoicePlayer::stop() {
    pendingPlay_ = false;
    if (effect_ != nullptr) effect_->stop();
    current_.clear();
}

bool VoicePlayer::isPlaying() const {
    return speaking_;
}
