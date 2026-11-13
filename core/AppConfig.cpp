// AppConfig.cpp
#include "core/AppConfig.h"

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

QString defaultConfigDir() {
#ifdef DP_CONFIG_DIR
    return QString::fromUtf8(DP_CONFIG_DIR);
#else
    return QString();
#endif
}

QString userConfigPath() {
    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
    return base + QStringLiteral("/DialoguePratice/llm.json");
}

QString readString(const QJsonObject& obj, const char* key, const QString& fallback) {
    const QJsonValue value = obj.value(QLatin1String(key));
    return value.isString() ? value.toString() : fallback;
}

/// 本地或局域网地址不需要 apiKey（自建 llama-server 等）。
bool isLocalOrLan(const QString& baseUrl) {
    const QString host = QUrl(baseUrl).host();
    if (host.isEmpty() || host == QLatin1String("localhost") || host == QLatin1String("::1")) {
        return true;
    }
    if (host.startsWith(QLatin1String("127.")) || host.startsWith(QLatin1String("10."))
        || host.startsWith(QLatin1String("192.168."))) {
        return true;
    }
    // 172.16.0.0 – 172.31.255.255
    const QStringList parts = host.split(QLatin1Char('.'));
    if (parts.size() == 4 && parts.at(0) == QLatin1String("172")) {
        bool ok = false;
        const int second = parts.at(1).toInt(&ok);
        if (ok && second >= 16 && second <= 31) return true;
    }
    return false;
}

}  // namespace

void setPathOverride(const QString& path) {
    g_overridePath = path;
}

QString resolvePath() {
    if (!g_overridePath.isEmpty()) return g_overridePath;

    const QString fromEnv = QProcessEnvironment::systemEnvironment()
                                .value(QStringLiteral("DP_LLM_CONFIG"));
    if (!fromEnv.isEmpty()) return fromEnv;

    const QString projectConfig = defaultConfigDir() + QStringLiteral("/llm.json");
    if (QFileInfo::exists(projectConfig)) return projectConfig;

    const QString userConfig = userConfigPath();
    if (QFileInfo::exists(userConfig)) return userConfig;

    // 都不存在时返回源码树路径，供 ensureTemplate 落盘
    if (!defaultConfigDir().isEmpty()) return projectConfig;
    return userConfig;
}

bool ensureTemplate(const QString& path) {
    if (path.isEmpty() || QFileInfo::exists(path)) return false;

    const QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) return false;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;

    const QByteArray tmpl = R"({
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
    file.write(tmpl);
    file.close();
    return true;
}

Settings parse(const QByteArray& data, const QString& source) {
    Settings settings;
    settings.path = source;

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);

    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        settings.error = QStringLiteral("配置不是合法 JSON：%1").arg(parseError.errorString());
        return settings;
    }

    const QJsonObject root = doc.object();
    const QJsonObject llm = root.value(QStringLiteral("llm")).toObject();
    const QJsonObject tr = root.value(QStringLiteral("translation")).toObject();

    settings.llm.enabled = llm.value(QStringLiteral("enabled")).toBool(true);
    settings.llm.baseUrl = readString(llm, "baseUrl", settings.llm.baseUrl);
    settings.llm.apiKey = readString(llm, "apiKey", settings.llm.apiKey);
    settings.llm.model = readString(llm, "model", settings.llm.model);
    settings.llm.temperature = llm.value(QStringLiteral("temperature")).toDouble(0.2);
    settings.llm.reasoningEffort = readString(llm, "reasoningEffort", QString());
    settings.llm.timeoutMs = llm.value(QStringLiteral("timeoutMs")).toInt(40000);

    settings.translation.maxHistoryMessages =
        tr.value(QStringLiteral("maxHistoryMessages")).toInt(12);
    settings.translation.cacheSize = tr.value(QStringLiteral("cacheSize")).toInt(500);

    // 可选的「翻译专用模型」：没写这段就沿用上面的 llm
    const QJsonObject trLlm = tr.value(QStringLiteral("llm")).toObject();
    if (!trLlm.isEmpty()) {
        settings.translation.llm.enabled = trLlm.value(QStringLiteral("enabled")).toBool(true);
        // baseUrl 缺省时继承主模型；apiKey 只在「同一服务商」时继承——
        // 否则会把 A 家的 Key 发给 B 家，结果是一堆 401
        settings.translation.llm.baseUrl = readString(trLlm, "baseUrl", settings.llm.baseUrl);
        QString trKey = readString(trLlm, "apiKey", QString());
        if (trKey.isEmpty() && settings.translation.llm.baseUrl == settings.llm.baseUrl) {
            trKey = settings.llm.apiKey;
        }
        settings.translation.llm.apiKey = trKey;
        settings.translation.llm.model = readString(trLlm, "model", settings.llm.model);
        // 翻译要稳定，温度默认比对话更低
        settings.translation.llm.temperature =
            trLlm.value(QStringLiteral("temperature")).toDouble(0.1);
        settings.translation.llm.reasoningEffort =
            readString(trLlm, "reasoningEffort", QString());
        settings.translation.llm.timeoutMs =
            trLlm.value(QStringLiteral("timeoutMs")).toInt(30000);
        settings.translation.hasOwnLlm = true;
    }

    settings.loaded = true;

    // 云端必须带 Key；本地/自建服务可以留空
    if (settings.llm.enabled && settings.llm.apiKey.trimmed().isEmpty()
        && !isLocalOrLan(settings.llm.baseUrl)) {
        settings.llm.enabled = false;

        // 区分「没配」和「配错」：完全是内置默认值说明这是自动生成的空模板，
        // 用户可能正等服务端下发配置，不该弹错误打扰他。
        const bool looksUnconfigured =
            settings.llm.baseUrl == QStringLiteral("https://api.deepseek.com")
            && settings.llm.model == QStringLiteral("deepseek-chat");
        if (!looksUnconfigured) {
            settings.error = QStringLiteral("配置文件里没有填 apiKey，翻译已停用");
        } else {
            settings.missing = true;
        }
    }

    // 翻译专用模型配了但不可用：回退到主模型，别让翻译整个挂掉
    if (settings.translation.hasOwnLlm && settings.translation.llm.enabled
        && settings.translation.llm.apiKey.trimmed().isEmpty()
        && !isLocalOrLan(settings.translation.llm.baseUrl)) {
        settings.translation.hasOwnLlm = false;
        if (settings.error.isEmpty()) {
            settings.error = QStringLiteral("翻译专用模型缺少 apiKey，已回退到主模型");
        }
    }

    return settings;
}

Settings load() {
    Settings settings;
    settings.path = resolvePath();

    QFile file(settings.path);
    if (!file.open(QIODevice::ReadOnly)) {
        settings.error = QStringLiteral("找不到配置文件：%1").arg(settings.path);
        settings.missing = true;   // 可能由服务端下发，不算错误
        return settings;
    }

    const QByteArray data = file.readAll();
    file.close();

    return parse(data, settings.path);
}

TransportSettings loadTransport() {
    TransportSettings settings;

    // 新文件名优先；读不到再回退旧版 agora.json（声网时期的配置）
    QString path = defaultConfigDir() + QStringLiteral("/transport.json");
    if (!QFileInfo::exists(path)) {
        const QString legacy = defaultConfigDir() + QStringLiteral("/agora.json");
        if (QFileInfo::exists(legacy)) path = legacy;
    }
    if (!QFileInfo::exists(path)) {
        const QString userDir =
            QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
            + QStringLiteral("/DialoguePratice/");
        if (QFileInfo::exists(userDir + QStringLiteral("transport.json"))) {
            path = userDir + QStringLiteral("transport.json");
        } else if (QFileInfo::exists(userDir + QStringLiteral("agora.json"))) {
            path = userDir + QStringLiteral("agora.json");
        }
    }
    settings.path = path;

    QFile file(path);
    if (file.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
        file.close();
        if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
            settings.transport =
                readString(doc.object(), "transport", settings.transport);
            settings.loaded = true;
        }
    }

    // 环境变量覆盖：临时切换不用改文件
    const QByteArray envTransport = qgetenv("DP_TRANSPORT");
    if (!envTransport.isEmpty()) settings.transport = QString::fromUtf8(envTransport);

    return settings;
}

}  // namespace AppConfig
