// VoiceCodec.h
//
// 语音消息的 Opus 编解码 + 自定义容器格式 DPV1。
//
// 为什么不用 Ogg/WAV：
//   - WAV(PCM) 太大：16k/16bit 单声道 20 秒 = 640 KB，Base64 后 850 KB，远超单帧 64 KiB
//   - Ogg 封装/解封装要额外依赖 libogg，而服务端只是不透明转发
//   - 自定义「长度前缀」格式最简单：编码端顺序写，解码端按包长顺序喂 opus_decode
//
// 容器格式（全部小端）：
//   +--------+--------------+------------------------------------------+
//   | "DPV1" | durationMs   | [pktLen:u16 | opus packet] * N           |
//   |  4 B   |    4 B       |  2 B + pktLen                            |
//   +--------+--------------+------------------------------------------+
#pragma once

#include <QByteArray>
#include <QString>

class VoiceCodec {
public:
    static constexpr int kSampleRate  = 16000;   // 与 whisper / QAudioSource 一致，免重采样
    static constexpr int kChannels    = 1;
    static constexpr int kFrameMs     = 20;      // 每包 20ms，Opus 的标准帧长
    static constexpr int kFrameSamples = kSampleRate * kFrameMs / 1000;   // 320
    static constexpr int kBitrate     = 12000;   // 12 kbps：20 秒约 30 KB

    /// PCM（16bit LE / 单声道 / 16 kHz）-> DPV1 容器。失败返回空。
    static QByteArray encode(const QByteArray& pcm16);

    /// DPV1 容器 -> PCM（16bit LE）。失败返回空。
    static QByteArray decode(const QByteArray& container);

    /// 读容器头里的时长（毫秒）；格式非法返回 0。
    static int durationMs(const QByteArray& container);

    /// 给 PCM 套上 44 字节 WAV 头（whisper 转写和 QSoundEffect 播放都要 WAV）。
    static QByteArray toWav(const QByteArray& pcm16);

    /// 上次失败的原因（空表示成功）。
    static QString lastError();

private:
    static void setError(const QString& message);
};
