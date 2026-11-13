// 把 16k 单声道 WAV 编码成 DPV1 容器并输出 Base64，
// 供 peer_bot.py 之类的测试工具直接当作 Voice 帧的 opus 字段发出去。
#include "media/VoiceCodec.h"

#include <QCoreApplication>
#include <QFile>

#include <cstdio>

namespace {

bool readWavPcm(const QString& path, QByteArray& pcm, QString& error) {
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
        error = QStringLiteral("需要 16kHz/单声道/16bit，实际 %1Hz %2ch %3bit")
                    .arg(sampleRate).arg(channels).arg(bits);
        return false;
    }
    return true;
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
    if (!readWavPcm(QString::fromLocal8Bit(argv[1]), pcm, error)) {
        std::fprintf(stderr, "读取失败: %s\n", error.toUtf8().constData());
        return 2;
    }

    const QByteArray container = VoiceCodec::encode(pcm);
    if (container.isEmpty()) {
        std::fprintf(stderr, "编码失败: %s\n", VoiceCodec::lastError().toUtf8().constData());
        return 3;
    }

    // stdout 只输出一行：<durationMs> <base64>
    std::printf("%d %s\n", VoiceCodec::durationMs(container),
                container.toBase64().constData());
    return 0;
}
