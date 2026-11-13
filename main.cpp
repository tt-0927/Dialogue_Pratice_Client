// main.cpp
//
// 客户端入口。除组装各层与解析命令行外不做任何业务逻辑。
#include "business/ChatModel.h"
#include "core/AppConfig.h"
#include "core/Theme.h"
#include "media/TtsService.h"
#include "media/VoiceRecorder.h"
#include "ui/MainWindow.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QFont>

#include <QTimer>

#include <cstdio>

namespace {

/// 诊断模式：只做「WAV -> 文本」，用来在没有麦克风的环境验证语音链路。
int runVoiceDiagnostic(const QString& wavPath, const QString& modelPath) {
    VoiceRecorder voice;
    voice.setModelPath(modelPath);

    QObject::connect(&voice, &VoiceRecorder::modelLoaded, &voice,
                     [&voice, wavPath](bool ok, const QString& message) {
                         if (!ok) {
                             std::fprintf(stderr, "模型加载失败: %s\n", message.toUtf8().constData());
                             QCoreApplication::exit(2);
                             return;
                         }
                         voice.transcribeFile(wavPath);
                     });

    QObject::connect(&voice, &VoiceRecorder::textReady, &voice,
                     [](const QString& text, qint64 elapsedMs) {
                         std::printf("%s\n", text.toUtf8().constData());
                         std::fprintf(stderr, "识别耗时 %lld ms\n",
                                      static_cast<long long>(elapsedMs));
                         QCoreApplication::exit(0);
                     });

    QObject::connect(&voice, &VoiceRecorder::failed, &voice, [](const QString& message) {
        std::fprintf(stderr, "识别失败: %s\n", message.toUtf8().constData());
        QCoreApplication::exit(3);
    });

    voice.start();
    return QCoreApplication::exec();
}

}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("DialoguePratice"));
    QCoreApplication::setApplicationName(QStringLiteral("Client"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Dialogue Pratice 客户端"));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption voiceFileOption(
        QStringLiteral("voice-file"),
        QStringLiteral("识别指定 WAV（16kHz/单声道/16-bit）后退出，用于验证语音链路"),
        QStringLiteral("path"));
    QCommandLineOption configOption(
        QStringLiteral("config"),
        QStringLiteral("LLM 配置文件路径，默认 config/llm.json"),
        QStringLiteral("path"));
    parser.addOption(configOption);
    QCommandLineOption sayOption(
        QStringLiteral("say"),
        QStringLiteral("用 Piper 合成并播放一段英文后退出，用于验证 TTS 链路"),
        QStringLiteral("text"));
    parser.addOption(sayOption);
    QCommandLineOption modelOption(
        QStringLiteral("model"),
        QStringLiteral("语音模型路径，默认 models/ggml-base.en.bin"),
        QStringLiteral("path"));
    parser.addOption(voiceFileOption);
    parser.addOption(modelOption);
    parser.process(app);

    if (parser.isSet(configOption)) {
        AppConfig::setPathOverride(parser.value(configOption));
    }

    const QString modelPath = parser.isSet(modelOption)
                                  ? parser.value(modelOption)
                                  : QString::fromUtf8(DP_DEFAULT_MODEL_PATH);

    if (parser.isSet(voiceFileOption)) {
        return runVoiceDiagnostic(parser.value(voiceFileOption), modelPath);
    }

    if (parser.isSet(sayOption)) {
        TtsService tts;
        tts.setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
        if (!tts.isAvailable()) {
            std::fprintf(stderr, "未安装 Piper，请运行 scripts/setup_tts.sh\n");
            return 4;
        }
        QObject::connect(&tts, &TtsService::failed, &tts, [](const QString& message) {
            std::fprintf(stderr, "TTS 失败: %s\n", message.toUtf8().constData());
            QCoreApplication::exit(5);
        });
        QObject::connect(&tts, &TtsService::speakingChanged, &tts, [](bool speaking) {
            if (speaking) std::fprintf(stderr, "开始播放\n");
        });
        tts.speak(parser.value(sayOption));
        QTimer::singleShot(4000, &app, [] { QCoreApplication::quit(); });
        return app.exec();
    }

    qRegisterMetaType<MemberInfo>();
    qRegisterMetaType<QVector<MemberInfo>>("QVector<MemberInfo>");
    qRegisterMetaType<ChatMessage>();
    qRegisterMetaType<QVector<ChatMessage>>("QVector<ChatMessage>");
    qRegisterMetaType<Suggestion>();
    qRegisterMetaType<QVector<Suggestion>>("QVector<Suggestion>");
    // ChatModel 的 itemAppended 信号带这条结构；同线程直连虽不强制注册，
    // 但注册后未来接队列连接或写测试时不会踩「未注册类型」的坑。
    qRegisterMetaType<business::ChatItem>();
    qRegisterMetaType<QVector<business::ChatItem>>("QVector<business::ChatItem>");

    app.setStyleSheet(theme::styleSheet());

    MainWindow window;
    window.show();

    return app.exec();
}
