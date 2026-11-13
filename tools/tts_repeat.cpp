// 用真实 TtsService 复现「反复快速点击播放」——每次都在上一段还没播完时打断
#include "media/TtsService.h"

#include <QGuiApplication>
#include <QStringList>
#include <QTimer>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    QGuiApplication app(argc, argv);

    // rapid 模式：同一句每 800ms 点一次，共 6 次（音频 2.1s，每次都打断上一段）
    const bool rapid = (argc > 1 && std::strcmp(argv[1], "rapid") == 0);
    const int intervalMs = rapid ? 800 : 4000;
    const int times = rapid ? 6 : 4;

    TtsService tts;
    tts.setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
    if (!tts.isAvailable()) {
        std::fprintf(stderr, "Piper 不可用\n");
        return 2;
    }

    QObject::connect(&tts, &TtsService::failed, [](const QString& message) {
        std::printf("[fail] %s\n", message.toUtf8().constData());
        std::fflush(stdout);
    });
    QObject::connect(&tts, &TtsService::speakingChanged, [](bool speaking) {
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
