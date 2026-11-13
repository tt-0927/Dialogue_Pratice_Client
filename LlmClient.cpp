// LlmClient.cpp
#include "LlmClient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

LlmClient::LlmClient(QObject* parent) : QObject(parent) {
    nam_ = new QNetworkAccessManager(this);
}

int LlmClient::chat(const QString& system, const QString& user, int maxTokens) {
    if (!isEnabled()) return 0;

    const int requestId = nextId_++;

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
    body[QStringLiteral("model")] = config_.model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("temperature")] = config_.temperature;
    body[QStringLiteral("max_tokens")] = maxTokens;
    body[QStringLiteral("stream")] = false;
    if (!config_.reasoningEffort.isEmpty()) {
        // 推理模型专用：翻译这种任务用 low 能省一半时间，质量不变
        body[QStringLiteral("reasoning_effort")] = config_.reasoningEffort;
    }

    // baseUrl 允许三种写法，统一补成 <base>/.../chat/completions：
    //   https://api.deepseek.com                 -> /v1/chat/completions
    //   https://dashscope.aliyuncs.com/compatible-mode/v1 -> /chat/completions
    //   https://x/v1/chat/completions（完整）     -> 原样使用
    QUrl url(config_.baseUrl);
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
    if (!config_.apiKey.isEmpty()) {
        request.setRawHeader("Authorization", "Bearer " + config_.apiKey.toUtf8());
    }
    request.setTransferTimeout(config_.timeoutMs);

    QNetworkReply* reply = nam_->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId] {
        handleReply(reply, requestId);
    });

    return requestId;
}

void LlmClient::handleReply(QNetworkReply* reply, int requestId) {
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
        // 推理模型（如 spark-x2.6、gpt-6.1-sol）会把 token 全花在 reasoning_content 上，
        // max_tokens 不够时正文就是空的。这种情况直接报「空内容」会让人一头雾水。
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
