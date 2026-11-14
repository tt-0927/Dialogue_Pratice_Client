/**
 * @file       config_probe.cpp
 * @brief      config_probe 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  打印 AppConfig 的解析结果，用于验证「翻译专用模型」配置是否被正确识别。
 */
#include "core/AppConfig.h"

#include <QCoreApplication>

#include <cstdio>

/**
 * @brief      程序入口：解析命令行参数、初始化服务端并运行事件循环。
 * @param[in] nArgc          输入数据；类型 int。
 * @param[in] strArgv        输入数据；类型 char**。
 * @return     返回处理结果码；0 表示成功，负值或 -1 表示失败。
 * @author     JaTin
 */
int main(int nArgc, char** strArgv) {
    QCoreApplication stApp(nArgc, strArgv);

    if (nArgc > 1) AppConfig::setPathOverride(QString::fromLocal8Bit(strArgv[1]));

    const AppConfig::Settings_S stS = AppConfig::load();

    std::printf("配置文件: %s\n", stS.path.toUtf8().constData());
    std::printf("  主模型（对话/建议）: enabled=%d  model=%s\n",
                stS.llm.enabled ? 1 : 0, stS.llm.model.toUtf8().constData());
    std::printf("                       baseUrl=%s  apiKey=%s\n",
                stS.llm.baseUrl.toUtf8().constData(),
                stS.llm.apiKey.isEmpty() ? "(空)" : "(已填)");
    std::printf("                       temperature=%.2f  effort=%s\n",
                stS.llm.temperature,
                stS.llm.reasoningEffort.isEmpty() ? "(未设)" : stS.llm.reasoningEffort.toUtf8().constData());

    std::printf("  翻译模型: hasOwnLlm=%d\n", stS.translation.hasOwnLlm ? 1 : 0);
    if (stS.translation.hasOwnLlm) {
        std::printf("            enabled=%d  model=%s\n",
                    stS.translation.llm.enabled ? 1 : 0,
                    stS.translation.llm.model.toUtf8().constData());
        std::printf("            baseUrl=%s  apiKey=%s  temperature=%.2f\n",
                    stS.translation.llm.baseUrl.toUtf8().constData(),
                    stS.translation.llm.apiKey.isEmpty() ? "(空)" : "(已填)",
                    stS.translation.llm.temperature);
    } else {
        std::printf("            （未单独配置，翻译沿用主模型）\n");
    }

    std::printf("  maxHistoryMessages=%d  cacheSize=%d\n",
                stS.translation.maxHistoryMessages, stS.translation.cacheSize);
    std::printf("  error=%s\n", stS.error.isEmpty() ? "(无)" : stS.error.toUtf8().constData());

    return 0;
}
