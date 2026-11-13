// VoiceCodec 自检：读 16k 单声道 WAV -> Opus 编码 -> 解码 -> 报告压缩比与信号完整性
#include "media/VoiceCodec.h"

#include <QCoreApplication>
#include <QFile>

#include <cmath>
#include <cstdio>

namespace {

/// 极简 WAV 读取：只支持 16bit PCM、单声道、16kHz（piper / whisper 的输出就是这种）
bool readWav(const QString& path, QByteArray& pcm, QString& error) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QStringLiteral("打不开文件");
        return false;
    }
    const QByteArray all = file.readAll();
    file.close();

    if (all.size() < 44 || all.left(4) != "RIFF" || all.mid(8, 4) != "WAVE") {
        error = QStringLiteral("不是 WAV");
        return false;
    }

    int pos = 12;
    int channels = 0;
    int sampleRate = 0;
    int bits = 0;
    while (pos + 8 <= all.size()) {
        const QByteArray id = all.mid(pos, 4);
        const auto size = *reinterpret_cast<const quint32*>(all.constData() + pos + 4);
        pos += 8;
        if (id == "fmt ") {
            channels = *reinterpret_cast<const quint16*>(all.constData() + pos + 2);
            sampleRate = *reinterpret_cast<const quint32*>(all.constData() + pos + 4);
            bits = *reinterpret_cast<const quint16*>(all.constData() + pos + 14);
        } else if (id == "data") {
            pcm = all.mid(pos, static_cast<int>(size));
            break;
        }
        pos += static_cast<int>(size) + (size % 2);
    }

    if (channels != 1 || sampleRate != VoiceCodec::kSampleRate || bits != 16) {
        error = QStringLiteral("需要 16kHz / 单声道 / 16bit，实际 %1Hz %2ch %3bit")
                    .arg(sampleRate).arg(channels).arg(bits);
        return false;
    }
    return true;
}

double rms(const QByteArray& pcm) {
    if (pcm.size() < 2) return 0.0;
    const auto* s = reinterpret_cast<const qint16*>(pcm.constData());
    const int n = static_cast<int>(pcm.size() / 2);
    double sum = 0.0;
    for (int i = 0; i < n; ++i) {
        const double v = s[i];
        sum += v * v;
    }
    return std::sqrt(sum / n);
}

}  // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    if (argc < 2) {
        std::fprintf(stderr, "用法: %s <16k 单声道 wav>\n", argv[0]);
        return 1;
    }

    QByteArray pcm;
    QString error;
    if (!readWav(QString::fromLocal8Bit(argv[1]), pcm, error)) {
        std::fprintf(stderr, "读取失败: %s\n", error.toUtf8().constData());
        return 2;
    }

    const int durationMs = static_cast<int>(pcm.size() / 2) * 1000 / VoiceCodec::kSampleRate;
    std::printf("原始 PCM: %d 字节, %d ms, RMS=%.0f\n", static_cast<int>(pcm.size()), durationMs,
                rms(pcm));

    const QByteArray container = VoiceCodec::encode(pcm);
    if (container.isEmpty()) {
        std::fprintf(stderr, "编码失败: %s\n", VoiceCodec::lastError().toUtf8().constData());
        return 3;
    }
    std::printf("Opus 容器: %d 字节 (压缩到 %.1f%%), 头部时长=%d ms\n",
                static_cast<int>(container.size()),
                100.0 * container.size() / pcm.size(), VoiceCodec::durationMs(container));

    const QByteArray decoded = VoiceCodec::decode(container);
    if (decoded.isEmpty()) {
        std::fprintf(stderr, "解码失败: %s\n", VoiceCodec::lastError().toUtf8().constData());
        return 4;
    }
    std::printf("解码 PCM: %d 字节, RMS=%.0f\n", static_cast<int>(decoded.size()), rms(decoded));

    // 简单完整性判断：时长接近、能量不塌陷
    const int decodedMs = static_cast<int>(decoded.size() / 2) * 1000 / VoiceCodec::kSampleRate;
    const double ratio = rms(decoded) / (rms(pcm) + 1e-9);
    const bool ok = std::abs(decodedMs - durationMs) <= 40 && ratio > 0.3 && ratio < 3.0;
    std::printf("时长差 %d ms，能量比 %.2f -> %s\n", decodedMs - durationMs, ratio,
                ok ? "通过" : "异常");
    return ok ? 0 : 5;
}
