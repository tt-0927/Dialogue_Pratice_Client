/**
 * @file       AppConfig.cpp
 * @brief      AppConfig 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  AppConfig.cpp。
 */
#include "AppConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QUrl>

namespace AppConfig {
namespace {

QString g_overridePath;

/**
 * @brief      获取 default 配置 dir。
 * @note       本函数无输入参数。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString defaultConfigDir() {
#ifdef DP_CONFIG_DIR
    return QString::fromUtf8(DP_CONFIG_DIR);
#else
    return QString();
#endif
}

/**
 * @brief      获取用户配置文件路径。
 * @note       本函数无输入参数。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString userConfigPath() {
    const QString strBase = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return strBase + QStringLiteral("/DialoguePratice/llm.json");
}

/**
 * @brief      读取 string；实现上按协议字段组装或解析 JSON。
 * @param[in] stObj          输入数据；类型 const QJsonObject&。
 * @param[in] strKey         缓存键；类型 const char*。
 * @param[in] strFallback    输入数据；类型 const QString&。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString readString(const QJsonObject& stObj, const char* strKey, const QString& strFallback) {
    const QJsonValue stValue = stObj.value(QLatin1String(strKey));
    return stValue.isString() ? stValue.toString() : strFallback;
}

/**
 * @brief  本地或局域网地址不需要 apiKey（自建 llama-server 等）。
 * @details 判断地址是否为本机或局域网地址（此类地址无需 API 密钥）。
 */
bool isLocalOrLan(const QString& strBaseUrl) {
    const QString strHost = QUrl(strBaseUrl).host();
    if (strHost.isEmpty() || strHost == QLatin1String("localhost") || strHost == QLatin1String("::1")) {
        return true;
    }
    if (strHost.startsWith(QLatin1String("127.")) || strHost.startsWith(QLatin1String("10."))
        || strHost.startsWith(QLatin1String("192.168."))) {
        return true;
    }
    /**
     * @brief  172.16.0.0 – 172.31.255.255。
     */
    const QStringList strParts = strHost.split(QLatin1Char('.'));
    if (strParts.size() == 4 && strParts.at(0) == QLatin1String("172")) {
        bool nOk = false;
        const int nSecond = strParts.at(1).toInt(&nOk);
        if (nOk && nSecond >= 16 && nSecond <= 31) return true;
    }
    return false;
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      设置文件路径 override。
 * @param[in] strPath        文件路径；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void setPathOverride(const QString& strPath) {
    g_overridePath = strPath;
}

/**
 * @brief      获取 resolve 文件路径。
 * @note       本函数无输入参数。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString resolvePath() {
    if (!g_overridePath.isEmpty()) return g_overridePath;

    const QString strFromEnv = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("DP_LLM_CONFIG"));
    if (!strFromEnv.isEmpty()) return strFromEnv;

    const QString strProjectConfig = defaultConfigDir() + QStringLiteral("/llm.json");
    if (QFileInfo::exists(strProjectConfig)) return strProjectConfig;

    const QString strUserConfig = userConfigPath();
    if (QFileInfo::exists(strUserConfig)) return strUserConfig;

    /**
     * @brief  都不存在时返回源码树路径，供 ensureTemplate 落盘。
     */
    if (!defaultConfigDir().isEmpty()) return strProjectConfig;
    return strUserConfig;
}

/**
 * @brief      确保 template；失败时返回 false。
 * @param[in] strPath        文件路径；类型 const QString&。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool ensureTemplate(const QString& strPath) {
    if (strPath.isEmpty() || QFileInfo::exists(strPath)) return false;

    const QDir stDir = QFileInfo(strPath).absoluteDir();
    if (!stDir.exists() && !QDir().mkpath(stDir.absolutePath())) return false;

    QFile stFile(strPath);
    if (!stFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    const QByteArray byTmpl = R"({
  "_说明": "填写 apiKey 后即可启用翻译。改完保存即生效，客户端会自动重载。不要提交到 Git。",

  "llm": {
    "enabled": true,
    "baseUrl": "https://api.deepseek.com",
    "apiKey": "",
    "model": "deepseek-chat",
    "temperature": 0.2,
    "reasoningEffort": "",
    "timeoutMs": 40000
  },

  "translation": {
    "maxHistoryMessages": 12,
    "cacheSize": 500,

    "_说明_llm": "可选：翻译专用模型。保持 {} 表示翻译也用上面的 llm；填了之后上面的 llm 只负责生成智能建议。",
    "llm": {}
  }
}
)";
    stFile.write(byTmpl);
    stFile.close();
    return true;
}

/**
 * @brief      加载 load；实现上按协议字段组装或解析 JSON。
 * @note       本函数无输入参数。
 * @return     返回 Settings_S 类型的处理结果。
 * @author     JaTin
 */
Settings_S load() {
    Settings_S stSettings;
    stSettings.path = resolvePath();

    QFile stFile(stSettings.path);
    if (!stFile.open(QIODevice::ReadOnly)) {
        stSettings.error = QStringLiteral("找不到配置文件：%1").arg(stSettings.path);
        return stSettings;
    }

    QJsonParseError stParseError{};
    const QJsonDocument stDoc = QJsonDocument::fromJson(stFile.readAll(), &stParseError);
    stFile.close();

    if (stParseError.error != QJsonParseError::NoError || !stDoc.isObject()) {
        stSettings.error = QStringLiteral("配置文件不是合法 JSON：%1").arg(stParseError.errorString());
        return stSettings;
    }

    const QJsonObject stRoot = stDoc.object();
    const QJsonObject stLlm = stRoot.value(QStringLiteral("llm")).toObject();
    const QJsonObject stTr = stRoot.value(QStringLiteral("translation")).toObject();

    stSettings.llm.enabled = stLlm.value(QStringLiteral("enabled")).toBool(true);
    stSettings.llm.baseUrl = readString(stLlm, "baseUrl", stSettings.llm.baseUrl);
    stSettings.llm.apiKey = readString(stLlm, "apiKey", stSettings.llm.apiKey);
    stSettings.llm.model = readString(stLlm, "model", stSettings.llm.model);
    stSettings.llm.temperature = stLlm.value(QStringLiteral("temperature")).toDouble(0.2);
    stSettings.llm.reasoningEffort = readString(stLlm, "reasoningEffort", QString());
    stSettings.llm.timeoutMs = stLlm.value(QStringLiteral("timeoutMs")).toInt(40000);

    stSettings.translation.maxHistoryMessages =
        stTr.value(QStringLiteral("maxHistoryMessages")).toInt(12);
    stSettings.translation.cacheSize = stTr.value(QStringLiteral("cacheSize")).toInt(500);

    /**
     * @brief  可选的「翻译专用模型」：没写这段就沿用上面的 llm。
     */
    const QJsonObject stTrLlm = stTr.value(QStringLiteral("llm")).toObject();
    if (!stTrLlm.isEmpty()) {
        stSettings.translation.llm.enabled = stTrLlm.value(QStringLiteral("enabled")).toBool(true);
        /**
         * @brief  baseUrl 缺省时继承主模型；apiKey 只在「同一服务商」时继承—— 否则会把 A 家的 Key 发给 B 家，结果是一堆 401。
         */
        stSettings.translation.llm.baseUrl = readString(stTrLlm, "baseUrl", stSettings.llm.baseUrl);
        QString strTrKey = readString(stTrLlm, "apiKey", QString());
        if (strTrKey.isEmpty() && stSettings.translation.llm.baseUrl == stSettings.llm.baseUrl) {
            strTrKey = stSettings.llm.apiKey;
        }
        stSettings.translation.llm.apiKey = strTrKey;
        stSettings.translation.llm.model = readString(stTrLlm, "model", stSettings.llm.model);
        /**
         * @brief  翻译要稳定，温度默认比对话更低。
         */
        stSettings.translation.llm.temperature =
            stTrLlm.value(QStringLiteral("temperature")).toDouble(0.1);
        stSettings.translation.llm.reasoningEffort =
            readString(stTrLlm, "reasoningEffort", QString());
        stSettings.translation.llm.timeoutMs =
            stTrLlm.value(QStringLiteral("timeoutMs")).toInt(30000);
        stSettings.translation.hasOwnLlm = true;
    }

    stSettings.loaded = true;

    /**
     * @brief  云端必须带 Key；本地/自建服务可以留空。
     */
    if (stSettings.llm.enabled && stSettings.llm.apiKey.trimmed().isEmpty()
        && !isLocalOrLan(stSettings.llm.baseUrl)) {
        stSettings.llm.enabled = false;
        stSettings.error = QStringLiteral("配置文件里没有填 apiKey，翻译已停用");
    }

    /**
     * @brief  翻译专用模型配了但不可用：回退到主模型，别让翻译整个挂掉。
     */
    if (stSettings.translation.hasOwnLlm && stSettings.translation.llm.enabled
        && stSettings.translation.llm.apiKey.trimmed().isEmpty()
        && !isLocalOrLan(stSettings.translation.llm.baseUrl)) {
        stSettings.translation.hasOwnLlm = false;
        if (stSettings.error.isEmpty()) {
            stSettings.error = QStringLiteral("翻译专用模型缺少 apiKey，已回退到主模型");
        }
    }

    return stSettings;
}

/**
 * @brief      加载传输层；实现上按协议字段组装或解析 JSON。
 * @note       本函数无输入参数。
 * @return     返回 TransportSettings_S 类型的处理结果。
 * @author     JaTin
 */
TransportSettings_S loadTransport() {
    TransportSettings_S stSettings;

    /**
     * @brief  新文件名优先；读不到再回退旧版 agora.json（声网时期的配置）。
     */
    QString strPath = defaultConfigDir() + QStringLiteral("/transport.json");
    if (!QFileInfo::exists(strPath)) {
        const QString strLegacy = defaultConfigDir() + QStringLiteral("/agora.json");
        if (QFileInfo::exists(strLegacy)) strPath = strLegacy;
    }
    if (!QFileInfo::exists(strPath)) {
        const QString strUserDir =
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/DialoguePratice/");
        if (QFileInfo::exists(strUserDir + QStringLiteral("transport.json"))) {
            strPath = strUserDir + QStringLiteral("transport.json");
        } else if (QFileInfo::exists(strUserDir + QStringLiteral("agora.json"))) {
            strPath = strUserDir + QStringLiteral("agora.json");
        }
    }
    stSettings.path = strPath;

    QFile stFile(strPath);
    if (stFile.open(QIODevice::ReadOnly)) {
        QJsonParseError stParseError{};
        const QJsonDocument stDoc = QJsonDocument::fromJson(stFile.readAll(), &stParseError);
        stFile.close();
        if (stParseError.error == QJsonParseError::NoError && stDoc.isObject()) {
            stSettings.transport =
                readString(stDoc.object(), "transport", stSettings.transport);
            stSettings.loaded = true;
        }
    }

    /**
     * @brief  环境变量覆盖：临时切换不用改文件。
     */
    const QByteArray byEnvTransport = qgetenv("DP_TRANSPORT");
    if (!byEnvTransport.isEmpty()) stSettings.transport = QString::fromUtf8(byEnvTransport);

    return stSettings;
}

/**
 * @brief  namespace AppConfig。
 */
}
