/*
 * 文件名:    LlmClient.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CLlmClient 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CLlmClient.cpp */
#include "LlmClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

/*
 * 功能描述:  执行 CLlmClient 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CLlmClient::CLlmClient(QObject* parent) : QObject(parent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNam = new QNetworkAccessManager(this);
}

/*
 * 功能描述:  执行 chat 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  system  参数。
 * param [in]  user  参数。
 * param [in]  maxTokens  参数。
 * return:    返回值，含义见功能描述。
 */
int CLlmClient::chat(const QString& system, const QString& user, int maxTokens) {
    if (!isEnabled()) return 0;

    const int requestId = m_nNextId++;

    QJsonObject systemMessage;
    systemMessage[QStringLiteral("role")] = QStringLiteral("system");
    systemMessage[QStringLiteral("content")] = system;

    QJsonObject userMessage;
    userMessage[QStringLiteral("role")] = QStringLiteral("user");
    userMessage[QStringLiteral("content")] = user;

    QJsonArray messages;
    messages.append(systemMessage);
    messages.append(userMessage);

    QJsonObject body;
    body[QStringLiteral("model")] = m_stConfig.model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("temperature")] = m_stConfig.temperature;
    body[QStringLiteral("max_tokens")] = maxTokens;
    body[QStringLiteral("stream")] = false;
    if (!m_stConfig.reasoningEffort.isEmpty()) {
        /* 推理模型专用：翻译这种任务用 low 能省一半时间，质量不变 */
        body[QStringLiteral("reasoning_effort")] = m_stConfig.reasoningEffort;
    }

    /* baseUrl 允许三种写法，统一补成 <base>/.../chat/completions： */
    /* https://api.deepseek.com                 -> /v1/chat/completions */
    /* https://dashscope.aliyuncs.com/compatible-mode/v1 -> /chat/completions */
    /* https://x/v1/chat/completions（完整）     -> 原样使用 */
    QUrl url(m_stConfig.baseUrl);
    QString path = url.path();
    while (path.endsWith(QLatin1Char('/'))) path.chop(1);
    if (!path.endsWith(QStringLiteral("/chat/completions"))) {
        if (path.endsWith(QStringLiteral("/v1")) || path.endsWith(QStringLiteral("/v2"))) {
            path += QStringLiteral("/chat/completions");
        } else {
            path += QStringLiteral("/v1/chat/completions");
        }
        url.setPath(path);
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_stConfig.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + m_stConfig.apiKey.toUtf8());
    }
    request.setTransferTimeout(m_stConfig.timeoutMs);

    QNetworkReply* reply = m_pNam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId] {
        handleReply(reply, requestId);
    });

    return requestId;
}

/*
 * 功能描述:  执行 handleReply 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  reply  参数。
 * param [in]  requestId  参数。
 * return:    void
 */
void CLlmClient::handleReply(QNetworkReply* reply, int requestId) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit failed(requestId, reply->errorString());
        return;
    }

    const QByteArray payload = reply->readAll();
    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        emit failed(requestId, QStringLiteral("返回内容不是合法 JSON"));
        return;
    }

    const QJsonArray choices = doc.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()) {
        emit failed(requestId, QStringLiteral("返回里没有 choices"));
        return;
    }

    const QJsonObject message =
        choices.first().toObject().value(QStringLiteral("message")).toObject();
    const QString content = message.value(QStringLiteral("content")).toString().trimmed();
    if (content.isEmpty()) {
        /* 推理模型（如 spark-x2.6、gpt-6.1-sol）会把 token 全花在 reasoning_content 上， */
        /* max_tokens 不够时正文就是空的。这种情况直接报「空内容」会让人一头雾水。 */
        const QString reasoning =
            message.value(QStringLiteral("reasoning_content")).toString();
        if (!reasoning.isEmpty()) {
            emit failed(requestId,
                        QStringLiteral("模型只输出了思考过程，没有正文（把 reasoningEffort 设为 none）"));
        } else {
            emit failed(requestId, QStringLiteral("模型返回了空内容"));
        }
        return;
    }

    emit finished(requestId, content);
}
