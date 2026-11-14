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
 * 功能描述:  运行语音消息 diagnostic；实现上连接 Qt 信号与槽。
 * 作者:      JaTin
 * param [in]   strWavPath       输入数据，类型 const QString&。
 * param [in]   strModelPath     输入数据，类型 const QString&。
 * return:    返回处理结果码；0 表示成功，负值或 -1 表示失败。
 */
int runVoiceDiagnostic(const QString& strWavPath, const QString& strModelPath) {
    CVoiceRecorder stVoice;
    stVoice.setModelPath(strModelPath);

    QObject::connect(&stVoice, &CVoiceRecorder::modelLoaded, &stVoice,
                     [&stVoice, strWavPath](bool ok, const QString& message) {
                         if (!ok) {
                             std::fprintf(stderr, "模型加载失败: %s\n", message.toUtf8().constData());
                             QCoreApplication::exit(2);
                             return;
                         }
                         stVoice.transcribeFile(strWavPath);
                     });

    QObject::connect(&stVoice, &CVoiceRecorder::textReady, &stVoice,
                     [](const QString& text, qint64 elapsedMs) {
                         std::printf("%s\n", text.toUtf8().constData());
                         std::fprintf(stderr, "识别耗时 %lld ms\n",
                                      static_cast<long long>(elapsedMs));
                         QCoreApplication::exit(0);
                     });

    QObject::connect(&stVoice, &CVoiceRecorder::failed, &stVoice, [](const QString& message) {
        std::fprintf(stderr, "识别失败: %s\n", message.toUtf8().constData());
        QCoreApplication::exit(3);
    });

    stVoice.start();
    return QCoreApplication::exec();
}

/* namespace */
}

/*
 * 功能描述:  程序入口：解析命令行参数、初始化服务端并运行事件循环。
 * 作者:      JaTin
 * param [in]   nArgc            输入数据，类型 int。
 * return:    返回处理结果码；0 表示成功，负值或 -1 表示失败。
 */
int main(int nArgc, char* argv[]) {
    QApplication stApp(nArgc, argv);

    QCoreApplication::setOrganizationName(QStringLiteral("DialoguePratice"));
    QCoreApplication::setApplicationName(QStringLiteral("Client"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    QCommandLineParser stParser;
    stParser.setApplicationDescription(QStringLiteral("Dialogue Pratice 客户端"));
    stParser.addHelpOption();
    stParser.addVersionOption();

    QCommandLineOption stVoiceFileOption(
        QStringLiteral("voice-file"),
        QStringLiteral("识别指定 WAV（16kHz/单声道/16-bit）后退出，用于验证语音链路"),
        QStringLiteral("path"));
    QCommandLineOption stConfigOption(
        QStringLiteral("config"),
        QStringLiteral("LLM 配置文件路径，默认 config/llm.json"),
        QStringLiteral("path"));
    stParser.addOption(stConfigOption);
    QCommandLineOption stSayOption(
        QStringLiteral("say"),
        QStringLiteral("用 Piper 合成并播放一段英文后退出，用于验证 TTS 链路"),
        QStringLiteral("text"));
    stParser.addOption(stSayOption);
    QCommandLineOption stModelOption(
        QStringLiteral("model"),
        QStringLiteral("语音模型路径，默认 models/ggml-base.en.bin"),
        QStringLiteral("path"));
    stParser.addOption(stVoiceFileOption);
    stParser.addOption(stModelOption);
    stParser.process(stApp);

    if (stParser.isSet(stConfigOption)) {
        AppConfig::setPathOverride(stParser.value(stConfigOption));
    }

    const QString strModelPath = stParser.isSet(stModelOption)
                                  ? stParser.value(stModelOption)
                                  : QString::fromUtf8(DP_DEFAULT_MODEL_PATH);

    if (stParser.isSet(stVoiceFileOption)) {
        return runVoiceDiagnostic(stParser.value(stVoiceFileOption), strModelPath);
    }

    if (stParser.isSet(stSayOption)) {
        CTtsService stTts;
        stTts.setPaths(QString::fromUtf8(DP_PIPER_BIN), QString::fromUtf8(DP_PIPER_MODEL));
        if (!stTts.isAvailable()) {
            std::fprintf(stderr, "未安装 Piper，请运行 scripts/setup_tts.sh\n");
            return 4;
        }
        QObject::connect(&stTts, &CTtsService::failed, &stTts, [](const QString& message) {
            std::fprintf(stderr, "TTS 失败: %s\n", message.toUtf8().constData());
            QCoreApplication::exit(5);
        });
        QObject::connect(&stTts, &CTtsService::speakingChanged, &stTts, [](bool speaking) {
            if (speaking) std::fprintf(stderr, "开始播放\n");
        });
        stTts.speak(stParser.value(stSayOption));
        QTimer::singleShot(4000, &stApp, [] { QCoreApplication::quit(); });
        return stApp.exec();
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

    stApp.setStyleSheet(theme::styleSheet());

    CMainWindow stWindow;
    stWindow.show();

    return stApp.exec();
}
