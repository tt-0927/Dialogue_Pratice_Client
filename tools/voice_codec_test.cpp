/*
 * 文件名:    voice_codec_test.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  voice_codec_test 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CVoiceCodec 自检：读 16k 单声道 WAV -> Opus 编码 -> 解码 -> 报告压缩比与信号完整性 */
#include "media/VoiceCodec.h"

#include <QCoreApplication>
#include <QFile>

#include <cmath>
#include <cstdio>

namespace {

/* 极简 WAV 读取：只支持 16bit PCM、单声道、16kHz（piper / whisper 的输出就是这种） */
/*
 * 功能描述:  读取wav；失败时返回 false。
 * 作者:      JaTin
 * param [in]   strPath          文件路径，类型 const QString&。
 * param [out]  byPcm            输入数据，类型 QByteArray&。
 * param [out]  strError         错误信息，类型 QString&。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool readWav(const QString& strPath, QByteArray& byPcm, QString& strError) {
    QFile stFile(strPath);
    if (!stFile.open(QIODevice::ReadOnly)) {
        strError = QStringLiteral("打不开文件");
        return false;
    }
    const QByteArray byAll = stFile.readAll();
    stFile.close();

    if (byAll.size() < 44 || byAll.left(4) != "RIFF" || byAll.mid(8, 4) != "WAVE") {
        strError = QStringLiteral("不是 WAV");
        return false;
    }

    int nPos = 12;
    int nChannels = 0;
    int nSampleRate = 0;
    int nBits = 0;
    while (nPos + 8 <= byAll.size()) {
        const QByteArray byId = byAll.mid(nPos, 4);
        const auto stSize = *reinterpret_cast<const quint32*>(byAll.constData() + nPos + 4);
        nPos += 8;
        if (byId == "fmt ") {
            nChannels = *reinterpret_cast<const quint16*>(byAll.constData() + nPos + 2);
            nSampleRate = *reinterpret_cast<const quint32*>(byAll.constData() + nPos + 4);
            nBits = *reinterpret_cast<const quint16*>(byAll.constData() + nPos + 14);
        } else if (byId == "data") {
            byPcm = byAll.mid(nPos, static_cast<int>(stSize));
            break;
        }
        nPos += static_cast<int>(stSize) + (stSize % 2);
    }

    if (nChannels != 1 || nSampleRate != CVoiceCodec::kSampleRate || nBits != 16) {
        strError = QStringLiteral("需要 16kHz / 单声道 / 16bit，实际 %1Hz %2ch %3bit")
                    .arg(nSampleRate).arg(nChannels).arg(nBits);
        return false;
    }
    return true;
}

/*
 * 功能描述:  获取rms。
 * 作者:      JaTin
 * param [in]   byPcm            输入数据，类型 const QByteArray&。
 * return:    返回 double 类型的处理结果，具体含义见功能描述。
 */
double rms(const QByteArray& byPcm) {
    if (byPcm.size() < 2) return 0.0;
    const auto* pS = reinterpret_cast<const qint16*>(byPcm.constData());
    const int nN = static_cast<int>(byPcm.size() / 2);
    double dSum = 0.0;
    for (int i = 0; i < nN; ++i) {
        const double dV = pS[i];
        dSum += dV * dV;
    }
    return std::sqrt(dSum / nN);
}

/* namespace */
}

/*
 * 功能描述:  程序入口：解析命令行参数、初始化服务端并运行事件循环。
 * 作者:      JaTin
 * param [in]   nArgc            输入数据，类型 int。
 * param [in]   strArgv          输入数据，类型 char**。
 * return:    返回处理结果码；0 表示成功，负值或 -1 表示失败。
 */
int main(int nArgc, char** strArgv) {
    QCoreApplication stApp(nArgc, strArgv);
    if (nArgc < 2) {
        std::fprintf(stderr, "用法: %s <16k 单声道 wav>\n", strArgv[0]);
        return 1;
    }

    QByteArray byPcm;
    QString strError;
    if (!readWav(QString::fromLocal8Bit(strArgv[1]), byPcm, strError)) {
        std::fprintf(stderr, "读取失败: %s\n", strError.toUtf8().constData());
        return 2;
    }

    const int nDurationMs = static_cast<int>(byPcm.size() / 2) * 1000 / CVoiceCodec::kSampleRate;
    std::printf("原始 PCM: %d 字节, %d ms, RMS=%.0f\n", static_cast<int>(byPcm.size()), nDurationMs,
                rms(byPcm));

    const QByteArray byContainer = CVoiceCodec::encode(byPcm);
    if (byContainer.isEmpty()) {
        std::fprintf(stderr, "编码失败: %s\n", CVoiceCodec::lastError().toUtf8().constData());
        return 3;
    }
    std::printf("Opus 容器: %d 字节 (压缩到 %.1f%%), 头部时长=%d ms\n",
                static_cast<int>(byContainer.size()),
                100.0 * byContainer.size() / byPcm.size(), CVoiceCodec::durationMs(byContainer));

    const QByteArray byDecoded = CVoiceCodec::decode(byContainer);
    if (byDecoded.isEmpty()) {
        std::fprintf(stderr, "解码失败: %s\n", CVoiceCodec::lastError().toUtf8().constData());
        return 4;
    }
    std::printf("解码 PCM: %d 字节, RMS=%.0f\n", static_cast<int>(byDecoded.size()), rms(byDecoded));

    /* 简单完整性判断：时长接近、能量不塌陷 */
    const int nDecodedMs = static_cast<int>(byDecoded.size() / 2) * 1000 / CVoiceCodec::kSampleRate;
    const double dRatio = rms(byDecoded) / (rms(byPcm) + 1e-9);
    const bool nOk = std::abs(nDecodedMs - nDurationMs) <= 40 && dRatio > 0.3 && dRatio < 3.0;
    std::printf("时长差 %d ms，能量比 %.2f -> %s\n", nDecodedMs - nDurationMs, dRatio,
                nOk ? "通过" : "异常");
    return nOk ? 0 : 5;
}
