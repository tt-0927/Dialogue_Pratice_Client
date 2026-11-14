/**
 * @file       AppConfig.h
 * @brief      AppConfig 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  AppConfig.h。
 */

/**
 * @brief  统一的配置文件加载：LLM 走云端（OpenAI 兼容），地址/密钥/模型都写在 JSON 里， 改完保存即可生效（ChatPage 里用 QFileSystemWatcher 热重载），不用重新编译或重启。
 */

/**
 * @brief  查找顺序： 1. 命令行 --config <path> 2. 环境变量 DP_LLM_CONFIG 3. 源码树 config/llm.json（编译期写入 DP_CONFIG_DIR） 4. ~/.config/DialoguePratice/llm.json。
 */
#pragma once

#include <QByteArray>
#include <QString>

namespace AppConfig {

/**
 * @brief   大模型配置集合S。
 * @author  JaTin
 */
struct LlmSettings_S {
    /**
     * @brief  是否启用。 初值 = true。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    enabled     = true;
    /**
     * @brief  模型服务地址。 初值 = QStringLiteral( )。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString baseUrl     = QStringLiteral("https://api.deepseek.com");
    /**
     * @brief  API 密钥。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString apiKey;
    /**
     * @brief  模型名称。 初值 = QStringLiteral( )。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString model       = QStringLiteral("deepseek-chat");
    /**
     * @brief  采样温度。 初值 = 0.2。
     * @note   类型 double；类/结构体成员变量。
     */
    double  temperature = 0.2;
    /**
     * @brief  reasoningEffort。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString reasoningEffort;
    /**
     * @brief  超时时间，单位毫秒。 初值 = 40000。
     * @note   类型 int；类/结构体成员变量。
     */
    int     timeoutMs   = 40000;
};

/**
 * @brief   译文配置集合S。
 * @author  JaTin
 */
struct TranslationSettings_S {
    /**
     * @brief  进房补历史时最多翻多少条
     * @note   类型 int；类/结构体成员变量。
     */
    int maxHistoryMessages = 12;
    /**
     * @brief  缓存长度。 初值 = 500。
     * @note   类型 int；类/结构体成员变量。
     */
    int cacheSize          = 500;

    /**
     * @brief  翻译专用模型（可选）。 翻译和「智能建议」任务性质不同：翻译要准，建议要自然， 所以允许分开配——比如翻译用 Hunyuan-MT，对话/建议用 Qwen。
     * @details 没配这一段时翻译沿用 Settings_S::llm。
     */
    LlmSettings_S llm;
    /**
     * @brief  hasOwn大模型。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool        hasOwnLlm = false;
};

/**
 * @brief  传输层配置。目前只有自研服务端一种实现，保留结构是为了以后换传输方式。
 * @details （比如 WebSocket）时不用改调用方。
 */
struct TransportSettings_S {
    /**
     * @brief  目前仅支持 server
     * @note   类型 QString；类/结构体成员变量。
     */
    QString transport = QStringLiteral("server");
    /**
     * @brief  文件路径。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString path;
    /**
     * @brief  loaded。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    loaded = false;
};

/**
 * @brief   配置集合S。
 * @author  JaTin
 */
struct Settings_S {
    /**
     * @brief  大模型。
     * @note   类型 LlmSettings_S；类/结构体成员变量。
     */
    LlmSettings_S         llm;
    /**
     * @brief  译文。
     * @note   类型 TranslationSettings_S；类/结构体成员变量。
     */
    TranslationSettings_S translation;

    /**
     * @brief  实际加载到的文件（空表示没找到）
     * @note   类型 QString；类/结构体成员变量。
     */
    QString path;
    /**
     * @brief  加载/校验错误，空表示正常
     * @note   类型 QString；类/结构体成员变量。
     */
    QString error;
    /**
     * @brief  loaded。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    loaded = false;

    /**
     * @brief  配置文件根本不存在（而非内容有问题）。
     * @details 这种情况不该打扰用户——配置可能由服务端随登录下发。
     */
    bool    missing = false;
};

/**
 * @brief      命令行 --config 覆盖路径（在 CMainWindow 之前调用）。
 * @param[in] path           文件路径；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void setPathOverride(const QString& path);

/**
 * @brief      按上述优先级解析出配置文件路径（不保证存在）。
 * @note       本函数无输入参数。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString resolvePath();

/**
 * @brief      读取配置；文件不存在时返回默认值并给出提示，不会抛异常。
 * @note       本函数无输入参数。
 * @return     返回 Settings_S 类型的处理结果。
 * @author     JaTin
 */
Settings_S load();

/**
 * @brief  从内存里的 JSON 解析配置（不读文件）。 data JSON 字节流。 source 来源描述，仅用于错误信息与 path 字段。 解析结果；JSON 非法时 error 非空、loaded 为 false。 服务端下发的配置走这条路径，与本地文件共用同一套校验逻辑。
 * @details （包括 apiKey 缺失、翻译专用模型回退等规则）。
 */
Settings_S parse(const QByteArray& data, const QString& source);

/**
 * @brief      若文件不存在，写一份带注释的模板，返回是否创建成功。
 * @param[in] path           文件路径；类型 const QString&。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool ensureTemplate(const QString& path);

/**
 * @brief  读取 config/transport.json。文件不存在时返回默认值（transport=server）。
 * @details 兼容旧版文件名 config/agora.json（会自动读取，但不再写入）。
 */
TransportSettings_S loadTransport();

/**
 * @brief  namespace AppConfig。
 */
}
