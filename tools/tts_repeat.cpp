/**
 * @file       tts_repeat.cpp
 * @brief      tts_repeat 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  用真实 CTtsService 复现「反复快速点击播放」——每次都在上一段还没播完时打断。
 */
#include "media/TtsService.h"

#include <QGuiApplication>
#include <QStringList>
#include <QTimer>

#include <cstdio>
#include <cstring>

/**
 * @brief      程序入口：解析命令行参数、初始化服务端并运行事件循环。
 * @param[in] nArgc          输入数据；类型 int。
 * @param[in] strArgv        输入数据；类型 char**。
 * @return     返回处理结果码；0 表示成功，负值或 -1 表示失败。
 * @author     JaTin
 */
int main(int nArgc, char** strArgv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QGuiApplication stApp(nArgc, strArgv);

    /**
     * @brief  rapid 模式：同一句每 800ms 点一次，共 6 次（音频 2.1s，每次都打断上一段）。
     */
    const bool nRapid = (nArgc > 1 && std::strcmp(strArgv[1], "rapid") == 0);
    const int nIntervalMs = nRapid ? 800 : 4000;
    const int nTimes = nRapid ? 6 : 4;

    CTtsService stTts;
    stTts.setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
    if (!stTts.isAvailable()) {
        std::fprintf(stderr, "Piper 不可用\n");
        return 2;
    }

    QObject::connect(&stTts, &CTtsService::failed, [](const QString& message) {
        std::printf("[fail] %s\n", message.toUtf8().constData());
        std::fflush(stdout);
    });
    QObject::connect(&stTts, &CTtsService::speakingChanged, [](bool speaking) {
        std::printf("[speaking] %d\n", speaking ? 1 : 0);
        std::fflush(stdout);
    });

    const QStringList strTexts = {
        QStringLiteral("Hello there, this is a short test."),
        QStringLiteral("Hello there, this is a short test."),
        QStringLiteral("Nice to meet you, how are you today?"),
        QStringLiteral("Hello there, this is a short test."),
    };
    const QString strSame = QStringLiteral("Hello there, this is a short test.");

    int nIndex = 0;
    auto stFire = [&]() {
        if (nIndex >= nTimes) {
            std::printf("=== done ===\n");
            QCoreApplication::quit();
            return;
        }
        const QString strText = nRapid ? strSame : strTexts.at(nIndex % strTexts.size());
        std::printf("=== speak #%d (t=%dms) ===\n", nIndex + 1, nIndex * nIntervalMs);
        std::fflush(stdout);
        stTts.speak(strText);
        ++nIndex;
    };

    stFire();
    QTimer stTimer;
    QObject::connect(&stTimer, &QTimer::timeout, stFire);
    stTimer.start(nIntervalMs);

    return stApp.exec();
}
