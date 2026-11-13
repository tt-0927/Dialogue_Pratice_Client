/*
 * 文件名:    ReplyService.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CReplyService 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CReplyService.cpp */
#include "business/ReplyService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include "business/LlmClient.h"

namespace {

const char* kSystemPrompt =
    "你是英语口语陪练。对话中最后一条消息是对方发给我的，请给出 3 条「我」作为接收方回复对方的话。"
    "要求：口语化、每条不超过 12 个单词、难度适合中级学习者、三条之间角度不同。"
    "每条还要给出对应的中文翻译（自然口语化，不要逐字直译）。"
    "只输出一个 JSON 数组，每个元素形如 "
    "{\"en\":\"Sure, that sounds good.\",\"zh\":\"好的，听起来不错。\"}，"
    "不要任何解释、不要编号、不要代码块。";

constexpr int kMaxSuggestions = 3;
constexpr int kMaxEnglishLength = 120;

/* 字符串数组或按行拆分时没有中文，只有英文。 */
/*
 * 功能描述:  执行 englishOnly 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    返回值，含义见功能描述。
 */
Suggestion_S englishOnly(const QString& text) {
    Suggestion_S item;
    item.english = text;
    return item;
}

/* namespace */
}

CReplyService::CReplyService(QObject* parent) : QObject(parent) {}

/*
 * 功能描述:  执行 setClient 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  client  参数。
 * return:    void
 */
void CReplyService::setClient(CLlmClient* client) {
    if (m_pClient != nullptr) {
        disconnect(m_pClient, nullptr, this, nullptr);
    }
    m_pClient = client;
    if (m_pClient == nullptr) return;

    connect(m_pClient, &CLlmClient::finished, this, [this](int requestId, const QString& text) {
        /* 不是本次请求 */
        if (requestId != m_nCurrentId) return;
        m_nBusy = false;
        const QVector<Suggestion_S> items = parseSuggestions(text);
        if (items.isEmpty()) {
            emit failed(QStringLiteral("模型没有给出可用的建议"));
            return;
        }
        emit suggestionsReady(items);
    });
    connect(m_pClient, &CLlmClient::failed, this, [this](int requestId, const QString& reason) {
        if (requestId != m_nCurrentId) return;
        m_nBusy = false;
        emit failed(reason);
    });
}

/*
 * 功能描述:  执行 isEnabled 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool CReplyService::isEnabled() const {
    return m_pClient != nullptr && m_pClient->isEnabled();
}

/*
 * 功能描述:  执行 request 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  context  参数。
 * return:    void
 */
void CReplyService::request(const QVector<ChatMessage_S>& context) {
    if (!isEnabled() || m_nBusy || context.isEmpty()) return;

    QString dialog;
    for (const ChatMessage_S& message : context) {
        dialog += QStringLiteral("%1: %2\n").arg(message.name, message.text);
    }

    const QString user =
        QStringLiteral("对话上下文：\n%1\n请给出 3 条「我」可以回复对方的话，每条带中文翻译。").arg(dialog);

    m_nBusy = true;
    /* 每条要输出英文 + 中文，token 比纯英文多，上限提到 400 */
    m_nCurrentId = m_pClient->chat(QString::fromUtf8(kSystemPrompt), user, 400);
    if (m_nCurrentId <= 0) {
        m_nBusy = false;
        emit failed(QStringLiteral("LLM 未启用"));
    }
}

/*
 * 功能描述:  执行 parseSuggestions 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  raw  参数。
 * return:    返回值，含义见功能描述。
 */
QVector<Suggestion_S> CReplyService::parseSuggestions(const QString& raw) {
    QVector<Suggestion_S> result;

    QString text = raw.trimmed();
    const int begin = text.indexOf(QLatin1Char('['));
    const int end = text.lastIndexOf(QLatin1Char(']'));

    /* 1) 优先按 JSON 数组解析（模型可能包一层代码块） */
    if (begin >= 0 && end > begin) {
        const QByteArray candidate = text.mid(begin, end - begin + 1).toUtf8();
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(candidate, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isArray()) {
            for (const QJsonValue& value : doc.array()) {
                if (value.isObject()) {
                    /* 新格式：{"en": "...", "zh": "..."} */
                    const QJsonObject obj = value.toObject();
                    Suggestion_S item;
                    item.english = obj.value(QStringLiteral("en")).toString().trimmed();
                    item.chinese = obj.value(QStringLiteral("zh")).toString().trimmed();
                    if (item.english.isEmpty()) {
                        /* 模型偶尔把字段写成 english / chinese */
                        item.english = obj.value(QStringLiteral("english")).toString().trimmed();
                        item.chinese = obj.value(QStringLiteral("chinese")).toString().trimmed();
                    }
                    if (!item.english.isEmpty()) result.append(item);
                } else {
                    /* 兼容旧格式：纯字符串数组 */
                    const QString item = value.toString().trimmed();
                    if (!item.isEmpty()) result.append(englishOnly(item));
                }
            }
        }
    }

    /* 2) 退化成按行拆分，去掉 "1." "- " 这类前缀 */
    if (result.isEmpty()) {
        static const QRegularExpression prefix(
            QStringLiteral("^\\s*(?:[-*•]|\\d+[.、)])\\s*"));
        for (const QString& line : text.split(QLatin1Char('\n'))) {
            QString item = line.trimmed();
            item.remove(prefix);
            item.remove(QLatin1Char('"'));
            if (item.isEmpty() || item.startsWith(QLatin1Char('[')) || item.startsWith(QLatin1Char(']'))) {
                continue;
            }
            result.append(englishOnly(item));
        }
    }

    /* 最多 3 条，太长的不像建议 */
    while (result.size() > kMaxSuggestions) result.removeLast();
    for (int i = result.size() - 1; i >= 0; --i) {
        if (result.at(i).english.size() > kMaxEnglishLength) result.removeAt(i);
    }
    return result;
}
