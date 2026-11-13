/*
 * 文件名:    main.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  main 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* main.cpp */

/* 客户端入口。除组装各层与解析命令行外不做任何业务逻辑。 */
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

/* 诊断模式：只做「WAV -> 文本」，用来在没有麦克风的环境验证语音链路。 */
/*
 * 功能描述:  执行 runVoiceDiagnostic 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  wavPath  参数。
 * param [in]  modelPath  参数。
 * return:    返回值，含义见功能描述。
 */
int runVoiceDiagnostic(const QString& wavPath, const QString& modelPath) {
    CVoiceRecorder voice;
    voice.setModelPath(modelPath);

    QObject::connect(&voice, &CVoiceRecorder::modelLoaded, &voice,
                     [&voice, wavPath](bool ok, const QString& message) {
                         if (!ok) {
                             std::fprintf(stderr, "模型加载失败: %s\n", message.toUtf8().constData());
                             QCoreApplication::exit(2);
                             return;
                         }
                         voice.transcribeFile(wavPath);
                     });

    QObject::connect(&voice, &CVoiceRecorder::textReady, &voice,
                     [](const QString& text, qint64 elapsedMs) {
                         std::printf("%s\n", text.toUtf8().constData());
                         std::fprintf(stderr, "识别耗时 %lld ms\n",
                                      static_cast<long long>(elapsedMs));
                         QCoreApplication::exit(0);
                     });

    QObject::connect(&voice, &CVoiceRecorder::failed, &voice, [](const QString& message) {
        std::fprintf(stderr, "识别失败: %s\n", message.toUtf8().constData());
        QCoreApplication::exit(3);
    });

    voice.start();
    return QCoreApplication::exec();
}

/* namespace */
}

/*
 * 功能描述:  执行 main 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  argc  参数。
 * param [in]  argv  参数。
 * return:    返回值，含义见功能描述。
 */
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
        CTtsService tts;
        tts.setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
        if (!tts.isAvailable()) {
            std::fprintf(stderr, "未安装 Piper，请运行 scripts/setup_tts.sh\n");
            return 4;
        }
        QObject::connect(&tts, &CTtsService::failed, &tts, [](const QString& message) {
            std::fprintf(stderr, "TTS 失败: %s\n", message.toUtf8().constData());
            QCoreApplication::exit(5);
        });
        QObject::connect(&tts, &CTtsService::speakingChanged, &tts, [](bool speaking) {
            if (speaking) std::fprintf(stderr, "开始播放\n");
        });
        tts.speak(parser.value(sayOption));
        QTimer::singleShot(4000, &app, [] { QCoreApplication::quit(); });
        return app.exec();
    }

    qRegisterMetaType<MemberInfo_S>();
    qRegisterMetaType<QVector<MemberInfo_S>>("QVector<MemberInfo_S>");
    qRegisterMetaType<ChatMessage_S>();
    qRegisterMetaType<QVector<ChatMessage_S>>("QVector<ChatMessage_S>");
    qRegisterMetaType<Suggestion_S>();
    qRegisterMetaType<QVector<Suggestion_S>>("QVector<Suggestion_S>");
    /* ChatModel 的 itemAppended 信号带这条结构；同线程直连虽不强制注册， */
    /* 但注册后未来接队列连接或写测试时不会踩「未注册类型」的坑。 */
    qRegisterMetaType<business::ChatItem_S>();
    qRegisterMetaType<QVector<business::ChatItem_S>>("QVector<business::ChatItem_S>");

    app.setStyleSheet(theme::styleSheet());

    CMainWindow window;
    window.show();

    return app.exec();
}
