/*
 * 文件名:    AppConfig.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  AppConfig 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* AppConfig.h */

/* 统一的配置文件加载：LLM 走云端（OpenAI 兼容），地址/密钥/模型都写在 JSON 里， */
/* 改完保存即可生效（ChatPage 里用 QFileSystemWatcher 热重载），不用重新编译或重启。 */

/* 查找顺序： */
/* 1. 命令行 --config <path> */
/* 2. 环境变量 DP_LLM_CONFIG */
/* 3. 源码树 config/llm.json（编译期写入 DP_CONFIG_DIR） */
/* 4. ~/.config/DialoguePratice/llm.json */
#pragma once

#include <QString>

namespace AppConfig {

struct LlmSettings_S {
    bool    enabled     = true;
    QString baseUrl     = QStringLiteral("https://api.deepseek.com");
    QString apiKey;
    QString model       = QStringLiteral("deepseek-chat");
    double  temperature = 0.2;
    QString reasoningEffort;
    int     timeoutMs   = 40000;
};

struct TranslationSettings_S {
    /* 进房补历史时最多翻多少条 */
    int maxHistoryMessages = 12;
    int cacheSize          = 500;

    /* 翻译专用模型（可选）。 */
    /* 翻译和「智能建议」任务性质不同：翻译要准，建议要自然， */
    /* 所以允许分开配——比如翻译用 Hunyuan-MT，对话/建议用 Qwen。 */
    /* 没配这一段时翻译沿用 Settings_S::llm。 */
    LlmSettings_S llm;
    bool        hasOwnLlm = false;
};

/* 传输层配置。目前只有自研服务端一种实现，保留结构是为了以后换传输方式 */
/* （比如 WebSocket）时不用改调用方。 */
struct TransportSettings_S {
    /* 目前仅支持 server */
    QString transport = QStringLiteral("server");
    QString path;
    bool    loaded = false;
};

struct Settings_S {
    LlmSettings_S         llm;
    TranslationSettings_S translation;

    /* 实际加载到的文件（空表示没找到） */
    QString path;
    /* 加载/校验错误，空表示正常 */
    QString error;
    bool    loaded = false;
};

/* 命令行 --config 覆盖路径（在 CMainWindow 之前调用）。 */
void setPathOverride(const QString& path);

/* 按上述优先级解析出配置文件路径（不保证存在）。 */
QString resolvePath();

/* 读取配置；文件不存在时返回默认值并给出提示，不会抛异常。 */
Settings_S load();

/* 若文件不存在，写一份带注释的模板，返回是否创建成功。 */
bool ensureTemplate(const QString& path);

/* 读取 config/transport.json。文件不存在时返回默认值（transport=server）。 */
/* 兼容旧版文件名 config/agora.json（会自动读取，但不再写入）。 */
TransportSettings_S loadTransport();

/* namespace AppConfig */
}
