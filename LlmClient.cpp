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
 * 功能描述:  构造 CLlmClient 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject* parent) : QObject(。
 * return:    无返回值（构造函数或析构函数）。
 */
CLlmClient::CLlmClient(QObject* pParent) : QObject(pParent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pNam = new QNetworkAccessManager(this);
}

/*
 * 功能描述:  获取chat；实现上发起异步 HTTP 请求、按协议字段组装或解析 JSON。
 * 作者:      JaTin
 * param [in]   strSystem        系统提示词，类型 const QString&。
 * param [in]   strUser          用户输入内容，类型 const QString&。
 * param [in]   nMaxTokens       最大生成 token 数，类型 int。
 * return:    返回处理结果码；0 表示成功，负值或 -1 表示失败。
 */
int CLlmClient::chat(const QString& strSystem, const QString& strUser, int nMaxTokens) {
    if (!isEnabled()) return 0;

    const int nRequestId = m_nNextId++;

    QJsonObject stSystemMessage;
    stSystemMessage[QStringLiteral("role")] = QStringLiteral("system");
    stSystemMessage[QStringLiteral("content")] = strSystem;

    QJsonObject stUserMessage;
    stUserMessage[QStringLiteral("role")] = QStringLiteral("user");
    stUserMessage[QStringLiteral("content")] = strUser;

    QJsonArray aMessages;
    aMessages.append(stSystemMessage);
    aMessages.append(stUserMessage);

    QJsonObject stBody;
    stBody[QStringLiteral("model")] = m_stConfig.model;
    stBody[QStringLiteral("messages")] = aMessages;
    stBody[QStringLiteral("temperature")] = m_stConfig.temperature;
    stBody[QStringLiteral("max_tokens")] = nMaxTokens;
    stBody[QStringLiteral("stream")] = false;
    if (!m_stConfig.reasoningEffort.isEmpty()) {
        /* 推理模型专用：翻译这种任务用 low 能省一半时间，质量不变 */
        stBody[QStringLiteral("reasoning_effort")] = m_stConfig.reasoningEffort;
    }

    /* baseUrl 允许三种写法，统一补成 <base>/.../chat/completions： */
    /* https://api.deepseek.com                 -> /v1/chat/completions */
    /* https://dashscope.aliyuncs.com/compatible-mode/v1 -> /chat/completions */
    /* https://x/v1/chat/completions（完整）     -> 原样使用 */
    QUrl stUrl(m_stConfig.baseUrl);
    QString strPath = stUrl.path();
    while (strPath.endsWith(QLatin1Char('/'))) strPath.chop(1);
    if (!strPath.endsWith(QStringLiteral("/chat/completions"))) {
        if (strPath.endsWith(QStringLiteral("/v1")) || strPath.endsWith(QStringLiteral("/v2"))) {
            strPath += QStringLiteral("/chat/completions");
        } else {
            strPath += QStringLiteral("/v1/chat/completions");
        }
        stUrl.setPath(strPath);
    }

    QNetworkRequest stRequest(stUrl);
    stRequest.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_stConfig.apiKey.isEmpty()) {
        stRequest.setRawHeader("Authorization", "Bearer " + m_stConfig.apiKey.toUtf8());
    }
    stRequest.setTransferTimeout(m_stConfig.timeoutMs);

    QNetworkReply* pReply = m_pNam->post(stRequest, QJsonDocument(stBody).toJson(QJsonDocument::Compact));
    connect(pReply, &QNetworkReply::finished, this, [this, pReply, nRequestId] {
        handleReply(pReply, nRequestId);
    });

    return nRequestId;
}

/*
 * 功能描述:  处理回复；实现上按协议字段组装或解析 JSON。
 * 作者:      JaTin
 * param [in]   pReply           响应对象，类型 QNetworkReply*。
 * param [in]   nRequestId       请求编号，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLlmClient::handleReply(QNetworkReply* pReply, int nRequestId) {
    pReply->deleteLater();

    if (pReply->error() != QNetworkReply::NoError) {
        emit failed(nRequestId, pReply->errorString());
        return;
    }

    const QByteArray byPayload = pReply->readAll();
    QJsonParseError stParseError{};
    const QJsonDocument stDoc = QJsonDocument::fromJson(byPayload, &stParseError);
    if (stParseError.error != QJsonParseError::NoError || !stDoc.isObject()) {
        emit failed(nRequestId, QStringLiteral("返回内容不是合法 JSON"));
        return;
    }

    const QJsonArray aChoices = stDoc.object().value(QStringLiteral("choices")).toArray();
    if (aChoices.isEmpty()) {
        emit failed(nRequestId, QStringLiteral("返回里没有 choices"));
        return;
    }

    const QJsonObject stMessage =
        aChoices.first().toObject().value(QStringLiteral("message")).toObject();
    const QString strContent = stMessage.value(QStringLiteral("content")).toString().trimmed();
    if (strContent.isEmpty()) {
        /* 推理模型（如 spark-x2.6、gpt-6.1-sol）会把 token 全花在 reasoning_content 上， */
        /* max_tokens 不够时正文就是空的。这种情况直接报「空内容」会让人一头雾水。 */
        const QString strReasoning =
            stMessage.value(QStringLiteral("reasoning_content")).toString();
        if (!strReasoning.isEmpty()) {
            emit failed(nRequestId,
                        QStringLiteral("模型只输出了思考过程，没有正文（把 reasoningEffort 设为 none）"));
        } else {
            emit failed(nRequestId, QStringLiteral("模型返回了空内容"));
        }
        return;
    }

    emit finished(nRequestId, strContent);
}
