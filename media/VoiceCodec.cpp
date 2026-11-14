/**
 * @file       VoiceCodec.cpp
 * @brief      CVoiceCodec 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoiceCodec.cpp。
 */
#include "media/VoiceCodec.h"

#include <opus/opus.h>

#include <QByteArray>
#include <QVector>
#include <QtDebug>

#include <algorithm>
#include <cstring>

namespace {

QString g_lastError;

constexpr char kMagic[4] = {'D', 'P', 'V', '1'};
/**
 * @brief  magic(4) + durationMs(4)
 * @note   取值 8。
 */
constexpr int kHeaderBytes = 8;

/**
 * @brief      写入32。
 * @param[out] byOut          输入数据；类型 QByteArray&。
 * @param[in] uValue         待设置的数值；类型 quint32。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void writeU32(QByteArray& byOut, quint32 uValue) {
    byOut.append(static_cast<char>(uValue & 0xFF));
    byOut.append(static_cast<char>((uValue >> 8) & 0xFF));
    byOut.append(static_cast<char>((uValue >> 16) & 0xFF));
    byOut.append(static_cast<char>((uValue >> 24) & 0xFF));
}

/**
 * @brief      写入16。
 * @param[out] byOut          输入数据；类型 QByteArray&。
 * @param[in] uValue         待设置的数值；类型 quint16。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void writeU16(QByteArray& byOut, quint16 uValue) {
    byOut.append(static_cast<char>(uValue & 0xFF));
    byOut.append(static_cast<char>((uValue >> 8) & 0xFF));
}

/**
 * @brief      读取 32相关的数据并返回处理结果。
 * @param[in] strP           输入数据；类型 const char*。
 * @return     返回计数或长度；无有效数据时返回 0。
 * @author     JaTin
 */
quint32 readU32(const char* strP) {
    const auto* pB = reinterpret_cast<const unsigned char*>(strP);
    return static_cast<quint32>(pB[0]) | (static_cast<quint32>(pB[1]) << 8)
           | (static_cast<quint32>(pB[2]) << 16) | (static_cast<quint32>(pB[3]) << 24);
}

/**
 * @brief      读取 16相关的数据并返回处理结果。
 * @param[in] strP           输入数据；类型 const char*。
 * @return     返回计数或长度；无有效数据时返回 0。
 * @author     JaTin
 */
quint16 readU16(const char* strP) {
    const auto* pB = reinterpret_cast<const unsigned char*>(strP);
    return static_cast<quint16>(pB[0]) | (static_cast<quint16>(pB[1]) << 8);
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      设置错误。
 * @param[in] strMessage     消息内容；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CVoiceCodec::setError(const QString& strMessage) {
    g_lastError = strMessage;
    qWarning() << "CVoiceCodec:" << strMessage;
}

/**
 * @brief      获取 last 错误。
 * @note       本函数无输入参数。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString CVoiceCodec::lastError() {
    return g_lastError;
}

/**
 * @brief      编码 encode；实现上调用 Opus 完成音频编解码。
 * @param[in] byPcm16        输入数据；类型 const QByteArray&。
 * @return     返回字节数据；失败时返回空 QByteArray。
 * @author     JaTin
 */
QByteArray CVoiceCodec::encode(const QByteArray& byPcm16) {
    g_lastError.clear();
    if (byPcm16.isEmpty()) {
        setError(QStringLiteral("PCM 为空"));
        return {};
    }

    int nErr = OPUS_OK;
    OpusEncoder* pEncoder =
        opus_encoder_create(kSampleRate, kChannels, OPUS_APPLICATION_VOIP, &nErr);
    if (nErr != OPUS_OK || pEncoder == nullptr) {
        setError(QStringLiteral("创建编码器失败：%1").arg(opus_strerror(nErr)));
        return {};
    }
    opus_encoder_ctl(pEncoder, OPUS_SET_BITRATE(kBitrate));
    opus_encoder_ctl(pEncoder, OPUS_SET_COMPLEXITY(5));
    opus_encoder_ctl(pEncoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

    /**
     * @brief  16bit -> 采样点数；不足一帧的尾部补零。
     */
    const int nTotalSamples = static_cast<int>(byPcm16.size() / 2);
    const int nFrameCount = (nTotalSamples + kFrameSamples - 1) / kFrameSamples;
    const int nDurationMs = nTotalSamples * 1000 / kSampleRate;

    QByteArray byOut;
    byOut.reserve(byPcm16.size() / 8 + 64);
    byOut.append(kMagic, 4);
    writeU32(byOut, static_cast<quint32>(nDurationMs));

    const auto* pSamples = reinterpret_cast<const opus_int16*>(byPcm16.constData());
    QVector<opus_int16> aPadded(kFrameSamples, 0);
    unsigned char packet[1024];

    for (int frame = 0; frame < nFrameCount; ++frame) {
        const int nOffset = frame * kFrameSamples;
        const int nRemain = nTotalSamples - nOffset;

        const opus_int16* pInput = nullptr;
        if (nRemain >= kFrameSamples) {
            pInput = pSamples + nOffset;
        } else {
            /**
             * @brief  尾帧补零，Opus 要求整帧。
             */
            std::fill(aPadded.begin(), aPadded.end(), static_cast<opus_int16>(0));
            if (nRemain > 0) {
                std::copy(pSamples + nOffset, pSamples + nOffset + nRemain, aPadded.begin());
            }
            pInput = aPadded.constData();
        }

        const int nN = opus_encode(pEncoder, pInput, kFrameSamples, packet, sizeof(packet));
        if (nN < 0) {
            setError(QStringLiteral("编码失败：%1").arg(opus_strerror(nN)));
            opus_encoder_destroy(pEncoder);
            return {};
        }
        writeU16(byOut, static_cast<quint16>(nN));
        byOut.append(reinterpret_cast<const char*>(packet), nN);
    }

    opus_encoder_destroy(pEncoder);
    return byOut;
}

/**
 * @brief      解码 decode；实现上调用 Opus 完成音频编解码。
 * @param[in] byContainer    容器；类型 const QByteArray&。
 * @return     返回字节数据；失败时返回空 QByteArray。
 * @author     JaTin
 */
QByteArray CVoiceCodec::decode(const QByteArray& byContainer) {
    g_lastError.clear();
    if (byContainer.size() < kHeaderBytes || std::memcmp(byContainer.constData(), kMagic, 4) != 0) {
        setError(QStringLiteral("不是合法的 DPV1 容器"));
        return {};
    }

    int nErr = OPUS_OK;
    OpusDecoder* pDecoder = opus_decoder_create(kSampleRate, kChannels, &nErr);
    if (nErr != OPUS_OK || pDecoder == nullptr) {
        setError(QStringLiteral("创建解码器失败：%1").arg(opus_strerror(nErr)));
        return {};
    }

    QByteArray byPcm;
    const int nDurationMs = static_cast<int>(readU32(byContainer.constData() + 4));
    byPcm.reserve(nDurationMs * kSampleRate / 1000 * 2);

    /**
     * @brief  留出 PLC 抖动余量。
     */
    QVector<opus_int16> aFrame(kFrameSamples * 6, 0);
    int nOffset = kHeaderBytes;

    while (nOffset + 2 <= byContainer.size()) {
        const int nPacketLen = readU16(byContainer.constData() + nOffset);
        nOffset += 2;
        if (nPacketLen <= 0 || nOffset + nPacketLen > byContainer.size()) {
            setError(QStringLiteral("容器内的包长度越界"));
            opus_decoder_destroy(pDecoder);
            return {};
        }

        const int nN = opus_decode(pDecoder,
                                  reinterpret_cast<const unsigned char*>(byContainer.constData() + nOffset),
                                  nPacketLen, aFrame.data(), aFrame.size(), 0);
        nOffset += nPacketLen;
        if (nN < 0) {
            setError(QStringLiteral("解码失败：%1").arg(opus_strerror(nN)));
            opus_decoder_destroy(pDecoder);
            return {};
        }
        byPcm.append(reinterpret_cast<const char*>(aFrame.constData()), nN * 2);
    }

    opus_decoder_destroy(pDecoder);

    /**
     * @brief  按头部声明的时长裁掉尾帧补的零。
     */
    const int nWantedBytes = nDurationMs * kSampleRate / 1000 * 2;
    if (nWantedBytes > 0 && byPcm.size() > nWantedBytes) {
        byPcm.truncate(nWantedBytes);
    }
    return byPcm;
}

/**
 * @brief      转换为 wav。
 * @param[in] byPcm16        输入数据；类型 const QByteArray&。
 * @return     返回字节数据；失败时返回空 QByteArray。
 * @author     JaTin
 */
QByteArray CVoiceCodec::toWav(const QByteArray& byPcm16) {
    const quint32 uDataSize = static_cast<quint32>(byPcm16.size());
    const quint16 uBlockAlign = static_cast<quint16>(kChannels * 2);
    const quint32 uByteRate = static_cast<quint32>(kSampleRate) * uBlockAlign;

    QByteArray byWav;
    byWav.reserve(44 + byPcm16.size());
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
    stPut32(36 + uDataSize);
    byWav.append("WAVE", 4);
    byWav.append("fmt ", 4);
    stPut32(16);
    /**
     * @brief  PCM。
     */
    stPut16(1);
    stPut16(static_cast<quint16>(kChannels));
    stPut32(static_cast<quint32>(kSampleRate));
    stPut32(uByteRate);
    stPut16(uBlockAlign);
    /**
     * @brief  16 bit。
     */
    stPut16(16);
    byWav.append("data", 4);
    stPut32(uDataSize);
    byWav.append(byPcm16);
    return byWav;
}

/**
 * @brief      获取时长 ms。
 * @param[in] byContainer    容器；类型 const QByteArray&。
 * @return     返回处理结果码；0 表示成功，负值或 -1 表示失败。
 * @author     JaTin
 */
int CVoiceCodec::durationMs(const QByteArray& byContainer) {
    if (byContainer.size() < kHeaderBytes || std::memcmp(byContainer.constData(), kMagic, 4) != 0) {
        return 0;
    }
    return static_cast<int>(readU32(byContainer.constData() + 4));
}
