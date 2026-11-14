/*
 * 文件名:    voice_make.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  voice_make 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* 把 16k 单声道 WAV 编码成 DPV1 容器并输出 Base64， */
/* 供 peer_bot.py 之类的测试工具直接当作 Voice 帧的 opus 字段发出去。 */
#include "media/VoiceCodec.h"

#include <QCoreApplication>
#include <QFile>

#include <cstdio>

namespace {

/*
 * 功能描述:  读取wavpcm；失败时返回 false。
 * 作者:      JaTin
 * param [in]   strPath          文件路径，类型 const QString&。
 * param [out]  byPcm            输入数据，类型 QByteArray&。
 * param [out]  strError         错误信息，类型 QString&。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool readWavPcm(const QString& strPath, QByteArray& byPcm, QString& strError) {
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
        strError = QStringLiteral("需要 16kHz/单声道/16bit，实际 %1Hz %2ch %3bit")
                    .arg(nSampleRate).arg(nChannels).arg(nBits);
        return false;
    }
    return true;
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
    if (!readWavPcm(QString::fromLocal8Bit(strArgv[1]), byPcm, strError)) {
        std::fprintf(stderr, "读取失败: %s\n", strError.toUtf8().constData());
        return 2;
    }

    const QByteArray byContainer = CVoiceCodec::encode(byPcm);
    if (byContainer.isEmpty()) {
        std::fprintf(stderr, "编码失败: %s\n", CVoiceCodec::lastError().toUtf8().constData());
        return 3;
    }

    /* stdout 只输出一行：<durationMs> <base64> */
    std::printf("%d %s\n", CVoiceCodec::durationMs(byContainer),
                byContainer.toBase64().constData());
    return 0;
}
