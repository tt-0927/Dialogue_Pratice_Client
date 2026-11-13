/*
 * 文件名:    VoiceCodec.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CVoiceCodec 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CVoiceCodec.cpp */
#include "VoiceCodec.h"

#include <opus/opus.h>

#include <QByteArray>
#include <QVector>
#include <QtDebug>

#include <algorithm>
#include <cstring>

namespace {

QString g_lastError;

constexpr char kMagic[4] = {'D', 'P', 'V', '1'};
/* magic(4) + durationMs(4) */
constexpr int kHeaderBytes = 8;

/*
 * 功能描述:  执行 writeU32 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [out]  out  参数。
 * param [in]  value  参数。
 * return:    void
 */
void writeU32(QByteArray& out, quint32 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
    out.append(static_cast<char>((value >> 16) & 0xFF));
    out.append(static_cast<char>((value >> 24) & 0xFF));
}

/*
 * 功能描述:  执行 writeU16 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [out]  out  参数。
 * param [in]  value  参数。
 * return:    void
 */
void writeU16(QByteArray& out, quint16 value) {
    out.append(static_cast<char>(value & 0xFF));
    out.append(static_cast<char>((value >> 8) & 0xFF));
}

/*
 * 功能描述:  执行 readU32 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  p  参数。
 * return:    返回值，含义见功能描述。
 */
quint32 readU32(const char* p) {
    const auto* b = reinterpret_cast<const unsigned char*>(p);
    return static_cast<quint32>(b[0]) | (static_cast<quint32>(b[1]) << 8)
           | (static_cast<quint32>(b[2]) << 16) | (static_cast<quint32>(b[3]) << 24);
}

/*
 * 功能描述:  执行 readU16 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  p  参数。
 * return:    返回值，含义见功能描述。
 */
quint16 readU16(const char* p) {
    const auto* b = reinterpret_cast<const unsigned char*>(p);
    return static_cast<quint16>(b[0]) | (static_cast<quint16>(b[1]) << 8);
}

/* namespace */
}

/*
 * 功能描述:  执行 setError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  message  参数。
 * return:    void
 */
void CVoiceCodec::setError(const QString& message) {
    g_lastError = message;
    qWarning() << "CVoiceCodec:" << message;
}

/*
 * 功能描述:  执行 lastError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
QString CVoiceCodec::lastError() {
    return g_lastError;
}

/*
 * 功能描述:  执行 encode 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  pcm16  参数。
 * return:    返回值，含义见功能描述。
 */
QByteArray CVoiceCodec::encode(const QByteArray& pcm16) {
    g_lastError.clear();
    if (pcm16.isEmpty()) {
        setError(QStringLiteral("PCM 为空"));
        return {};
    }

    int err = OPUS_OK;
    OpusEncoder* encoder =
        opus_encoder_create(kSampleRate, kChannels, OPUS_APPLICATION_VOIP, &err);
    if (err != OPUS_OK || encoder == nullptr) {
        setError(QStringLiteral("创建编码器失败：%1").arg(opus_strerror(err)));
        return {};
    }
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(kBitrate));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    /* 16bit -> 采样点数；不足一帧的尾部补零 */
    const int totalSamples = static_cast<int>(pcm16.size() / 2);
    const int frameCount = (totalSamples + kFrameSamples - 1) / kFrameSamples;
    const int durationMs = totalSamples * 1000 / kSampleRate;

    QByteArray out;
    out.reserve(pcm16.size() / 8 + 64);
    out.append(kMagic, 4);
    writeU32(out, static_cast<quint32>(durationMs));

    const auto* samples = reinterpret_cast<const opus_int16*>(pcm16.constData());
    QVector<opus_int16> padded(kFrameSamples, 0);
    unsigned char packet[1024];

    for (int frame = 0; frame < frameCount; ++frame) {
        const int offset = frame * kFrameSamples;
        const int remain = totalSamples - offset;

        const opus_int16* input = nullptr;
        if (remain >= kFrameSamples) {
            input = samples + offset;
        } else {
            /* 尾帧补零，Opus 要求整帧 */
            std::fill(padded.begin(), padded.end(), static_cast<opus_int16>(0));
            if (remain > 0) {
                std::copy(samples + offset, samples + offset + remain, padded.begin());
            }
            input = padded.constData();
        }

        const int n = opus_encode(encoder, input, kFrameSamples, packet, sizeof(packet));
        if (n < 0) {
            setError(QStringLiteral("编码失败：%1").arg(opus_strerror(n)));
            opus_encoder_destroy(encoder);
            return {};
        }
        writeU16(out, static_cast<quint16>(n));
        out.append(reinterpret_cast<const char*>(packet), n);
    }

    opus_encoder_destroy(encoder);
    return out;
}

/*
 * 功能描述:  执行 decode 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  container  参数。
 * return:    返回值，含义见功能描述。
 */
QByteArray CVoiceCodec::decode(const QByteArray& container) {
    g_lastError.clear();
    if (container.size() < kHeaderBytes || std::memcmp(container.constData(), kMagic, 4) != 0) {
        setError(QStringLiteral("不是合法的 DPV1 容器"));
        return {};
    }

    int err = OPUS_OK;
    OpusDecoder* decoder = opus_decoder_create(kSampleRate, kChannels, &err);
    if (err != OPUS_OK || decoder == nullptr) {
        setError(QStringLiteral("创建解码器失败：%1").arg(opus_strerror(err)));
        return {};
    }

    QByteArray pcm;
    const int durationMs = static_cast<int>(readU32(container.constData() + 4));
    pcm.reserve(durationMs * kSampleRate / 1000 * 2);

    /* 留出 PLC 抖动余量 */
    QVector<opus_int16> frame(kFrameSamples * 6, 0);
    int offset = kHeaderBytes;

    while (offset + 2 <= container.size()) {
        const int packetLen = readU16(container.constData() + offset);
        offset += 2;
        if (packetLen <= 0 || offset + packetLen > container.size()) {
            setError(QStringLiteral("容器内的包长度越界"));
            opus_decoder_destroy(decoder);
            return {};
        }

        const int n = opus_decode(decoder,
                                  reinterpret_cast<const unsigned char*>(container.constData() + offset),
                                  packetLen, frame.data(), frame.size(), 0);
        offset += packetLen;
        if (n < 0) {
            setError(QStringLiteral("解码失败：%1").arg(opus_strerror(n)));
            opus_decoder_destroy(decoder);
            return {};
        }
        pcm.append(reinterpret_cast<const char*>(frame.constData()), n * 2);
    }

    opus_decoder_destroy(decoder);

    /* 按头部声明的时长裁掉尾帧补的零 */
    const int wantedBytes = durationMs * kSampleRate / 1000 * 2;
    if (wantedBytes > 0 && pcm.size() > wantedBytes) {
        pcm.truncate(wantedBytes);
    }
    return pcm;
}

/*
 * 功能描述:  执行 toWav 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  pcm16  参数。
 * return:    返回值，含义见功能描述。
 */
QByteArray CVoiceCodec::toWav(const QByteArray& pcm16) {
    const quint32 dataSize = static_cast<quint32>(pcm16.size());
    const quint16 blockAlign = static_cast<quint16>(kChannels * 2);
    const quint32 byteRate = static_cast<quint32>(kSampleRate) * blockAlign;

    QByteArray wav;
    wav.reserve(44 + pcm16.size());
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
    put32(36 + dataSize);
    wav.append("WAVE", 4);
    wav.append("fmt ", 4);
    put32(16);
    /* PCM */
    put16(1);
    put16(static_cast<quint16>(kChannels));
    put32(static_cast<quint32>(kSampleRate));
    put32(byteRate);
    put16(blockAlign);
    /* 16 bit */
    put16(16);
    wav.append("data", 4);
    put32(dataSize);
    wav.append(pcm16);
    return wav;
}

/*
 * 功能描述:  执行 durationMs 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  container  参数。
 * return:    返回值，含义见功能描述。
 */
int CVoiceCodec::durationMs(const QByteArray& container) {
    if (container.size() < kHeaderBytes || std::memcmp(container.constData(), kMagic, 4) != 0) {
        return 0;
    }
    return static_cast<int>(readU32(container.constData() + 4));
}
