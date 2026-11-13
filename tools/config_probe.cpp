/*
 * 文件名:    config_probe.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  config_probe 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* 打印 AppConfig 的解析结果，用于验证「翻译专用模型」配置是否被正确识别 */
#include "core/AppConfig.h"

#include <QCoreApplication>

#include <cstdio>

/*
 * 功能描述:  执行 main 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  argc  参数。
 * param [in]  argv  参数。
 * return:    返回值，含义见功能描述。
 */
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);

    if (argc > 1) AppConfig::setPathOverride(QString::fromLocal8Bit(argv[1]));

    const AppConfig::Settings_S s = AppConfig::load();

    std::printf("配置文件: %s\n", s.path.toUtf8().constData());
    std::printf("  主模型（对话/建议）: enabled=%d  model=%s\n",
                s.llm.enabled ? 1 : 0, s.llm.model.toUtf8().constData());
    std::printf("                       baseUrl=%s  apiKey=%s\n",
                s.llm.baseUrl.toUtf8().constData(),
                s.llm.apiKey.isEmpty() ? "(空)" : "(已填)");
    std::printf("                       temperature=%.2f  effort=%s\n",
                s.llm.temperature,
                s.llm.reasoningEffort.isEmpty() ? "(未设)" : s.llm.reasoningEffort.toUtf8().constData());

    std::printf("  翻译模型: hasOwnLlm=%d\n", s.translation.hasOwnLlm ? 1 : 0);
    if (s.translation.hasOwnLlm) {
        std::printf("            enabled=%d  model=%s\n",
                    s.translation.llm.enabled ? 1 : 0,
                    s.translation.llm.model.toUtf8().constData());
        std::printf("            baseUrl=%s  apiKey=%s  temperature=%.2f\n",
                    s.translation.llm.baseUrl.toUtf8().constData(),
                    s.translation.llm.apiKey.isEmpty() ? "(空)" : "(已填)",
                    s.translation.llm.temperature);
    } else {
        std::printf("            （未单独配置，翻译沿用主模型）\n");
    }

    std::printf("  maxHistoryMessages=%d  cacheSize=%d\n",
                s.translation.maxHistoryMessages, s.translation.cacheSize);
    std::printf("  error=%s\n", s.error.isEmpty() ? "(无)" : s.error.toUtf8().constData());

    return 0;
}
