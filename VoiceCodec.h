/**
 * @file       VoiceCodec.h
 * @brief      CVoiceCodec 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoiceCodec.h。
 */

/**
 * @brief  语音消息的 Opus 编解码 + 自定义容器格式 DPV1。
 */

/**
 * @brief  为什么不用 Ogg/WAV： - WAV(PCM) 太大：16k/16bit 单声道 20 秒 = 640 KB，Base64 后 850 KB，远超单帧 64 KiB - Ogg 封装/解封装要额外依赖 libogg，而服务端只是不透明转发 - 自定义「长度前缀」格式最简单：编码端顺序写，解码端按包长顺序喂 opus_decode。
 */

/**
 * @brief  容器格式（全部小端）： +--------+--------------+------------------------------------------+ | "DPV1" | durationMs | [pktLen:u16 | opus packet] * N | | 4 B | 4 B | 2 B + pktLen | +--------+--------------+------------------------------------------+。
 */
#pragma once

#include <QByteArray>
#include <QString>

/**
 * @brief   C语音消息编解码器。
 * @author  JaTin
 */
class CVoiceCodec {
public:
    /**
     * @brief  与 whisper / QAudioSource 一致，免重采样
     * @note   取值 16000。
     */
    static constexpr int kSampleRate  = 16000;
    /**
     * @brief  kChannels。
     * @note   取值 1。
     */
    static constexpr int kChannels    = 1;
    /**
     * @brief  每包 20ms，Opus 的标准帧长
     * @note   取值 20。
     */
    static constexpr int kFrameMs     = 20;
    /**
     * @brief  320
     * @note   取值 kSampleRate * kFrameMs / 1000。
     */
    static constexpr int kFrameSamples = kSampleRate * kFrameMs / 1000;
    /**
     * @brief  12 kbps：20 秒约 30 KB
     * @note   取值 12000。
     */
    static constexpr int kBitrate     = 12000;

    /**
     * @brief      PCM（16bit LE / 单声道 / 16 kHz）-> DPV1 容器。失败返回空。
     * @param[in] pcm16          输入参数；类型 const QByteArray&。
     * @return     返回字节数据；失败时返回空 QByteArray。
     * @author     JaTin
     */
    static QByteArray encode(const QByteArray& pcm16);

    /**
     * @brief      DPV1 容器 -> PCM（16bit LE）。失败返回空。
     * @param[in] container      容器；类型 const QByteArray&。
     * @return     返回字节数据；失败时返回空 QByteArray。
     * @author     JaTin
     */
    static QByteArray decode(const QByteArray& container);

    /**
     * @brief      读容器头里的时长（毫秒）；格式非法返回 0。
     * @param[in] container      容器；类型 const QByteArray&。
     * @return     返回处理结果码；0 表示成功，负值或 -1 表示失败。
     * @author     JaTin
     */
    static int durationMs(const QByteArray& container);

    /**
     * @brief      给 PCM 套上 44 字节 WAV 头（whisper 转写和 QSoundEffect 播放都要 WAV）。
     * @param[in] pcm16          输入参数；类型 const QByteArray&。
     * @return     返回字节数据；失败时返回空 QByteArray。
     * @author     JaTin
     */
    static QByteArray toWav(const QByteArray& pcm16);

    /**
     * @brief      上次失败的原因（空表示成功）。
     * @note       本函数无输入参数。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    static QString lastError();

private:
    /**
     * @brief      设置错误。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    static void setError(const QString& message);
};
