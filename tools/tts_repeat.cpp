/*
 * 文件名:    tts_repeat.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  tts_repeat 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* 用真实 CTtsService 复现「反复快速点击播放」——每次都在上一段还没播完时打断 */
#include "media/TtsService.h"

#include <QGuiApplication>
#include <QStringList>
#include <QTimer>

#include <cstdio>
#include <cstring>

/*
 * 功能描述:  执行 main 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  argc  参数。
 * param [in]  argv  参数。
 * return:    返回值，含义见功能描述。
 */
int main(int argc, char** argv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QGuiApplication app(argc, argv);

    /* rapid 模式：同一句每 800ms 点一次，共 6 次（音频 2.1s，每次都打断上一段） */
    const bool rapid = (argc > 1 && std::strcmp(argv[1], "rapid") == 0);
    const int intervalMs = rapid ? 800 : 4000;
    const int times = rapid ? 6 : 4;

    CTtsService tts;
    tts.setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
    if (!tts.isAvailable()) {
        std::fprintf(stderr, "Piper 不可用\n");
        return 2;
    }

    QObject::connect(&tts, &CTtsService::failed, [](const QString& message) {
        std::printf("[fail] %s\n", message.toUtf8().constData());
        std::fflush(stdout);
    });
    QObject::connect(&tts, &CTtsService::speakingChanged, [](bool speaking) {
        std::printf("[speaking] %d\n", speaking ? 1 : 0);
        std::fflush(stdout);
    });

    const QStringList texts = {
        QStringLiteral("Hello there, this is a short test."),
        QStringLiteral("Hello there, this is a short test."),
        QStringLiteral("Nice to meet you, how are you today?"),
        QStringLiteral("Hello there, this is a short test."),
    };
    const QString same = QStringLiteral("Hello there, this is a short test.");

    int index = 0;
    auto fire = [&]() {
        if (index >= times) {
            std::printf("=== done ===\n");
            QCoreApplication::quit();
            return;
        }
        const QString text = rapid ? same : texts.at(index % texts.size());
        std::printf("=== speak #%d (t=%dms) ===\n", index + 1, index * intervalMs);
        std::fflush(stdout);
        tts.speak(text);
        ++index;
    };

    fire();
    QTimer timer;
    QObject::connect(&timer, &QTimer::timeout, fire);
    timer.start(intervalMs);

    return app.exec();
}
