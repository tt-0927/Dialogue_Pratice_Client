// ReplyService.cpp
#include "ReplyService.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>

#include "LlmClient.h"

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

/// 字符串数组或按行拆分时没有中文，只有英文。
Suggestion englishOnly(const QString& text) {
    Suggestion item;
    item.english = text;
    return item;
}

}  // namespace

ReplyService::ReplyService(QObject* parent) : QObject(parent) {}

void ReplyService::setClient(LlmClient* client) {
    if (client_ != nullptr) {
        disconnect(client_, nullptr, this, nullptr);
    }
    client_ = client;
    if (client_ == nullptr) return;

    connect(client_, &LlmClient::finished, this, [this](int requestId, const QString& text) {
        if (requestId != currentId_) return;   // 不是本次请求
        busy_ = false;
        const QVector<Suggestion> items = parseSuggestions(text);
        if (items.isEmpty()) {
            emit failed(QStringLiteral("模型没有给出可用的建议"));
            return;
        }
        emit suggestionsReady(items);
    });
    connect(client_, &LlmClient::failed, this, [this](int requestId, const QString& reason) {
        if (requestId != currentId_) return;
        busy_ = false;
        emit failed(reason);
    });
}

bool ReplyService::isEnabled() const {
    return client_ != nullptr && client_->isEnabled();
}

void ReplyService::request(const QVector<ChatMessage>& context) {
    if (!isEnabled() || busy_ || context.isEmpty()) return;

    QString dialog;
    for (const ChatMessage& message : context) {
        dialog += QStringLiteral("%1: %2\n").arg(message.name, message.text);
    }

    const QString user =
        QStringLiteral("对话上下文：\n%1\n请给出 3 条「我」可以回复对方的话，每条带中文翻译。").arg(dialog);

    busy_ = true;
    // 每条要输出英文 + 中文，token 比纯英文多，上限提到 400
    currentId_ = client_->chat(QString::fromUtf8(kSystemPrompt), user, 400);
    if (currentId_ <= 0) {
        busy_ = false;
        emit failed(QStringLiteral("LLM 未启用"));
    }
}

QVector<Suggestion> ReplyService::parseSuggestions(const QString& raw) {
    QVector<Suggestion> result;

    QString text = raw.trimmed();
    const int begin = text.indexOf(QLatin1Char('['));
    const int end = text.lastIndexOf(QLatin1Char(']'));

    // 1) 优先按 JSON 数组解析（模型可能包一层代码块）
    if (begin >= 0 && end > begin) {
        const QByteArray candidate = text.mid(begin, end - begin + 1).toUtf8();
        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(candidate, &parseError);
        if (parseError.error == QJsonParseError::NoError && doc.isArray()) {
            for (const QJsonValue& value : doc.array()) {
                if (value.isObject()) {
                    // 新格式：{"en": "...", "zh": "..."}
                    const QJsonObject obj = value.toObject();
                    Suggestion item;
                    item.english = obj.value(QStringLiteral("en")).toString().trimmed();
                    item.chinese = obj.value(QStringLiteral("zh")).toString().trimmed();
                    if (item.english.isEmpty()) {
                        // 模型偶尔把字段写成 english / chinese
                        item.english = obj.value(QStringLiteral("english")).toString().trimmed();
                        item.chinese = obj.value(QStringLiteral("chinese")).toString().trimmed();
                    }
                    if (!item.english.isEmpty()) result.append(item);
                } else {
                    // 兼容旧格式：纯字符串数组
                    const QString item = value.toString().trimmed();
                    if (!item.isEmpty()) result.append(englishOnly(item));
                }
            }
        }
    }

    // 2) 退化成按行拆分，去掉 "1." "- " 这类前缀
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

    // 最多 3 条，太长的不像建议
    while (result.size() > kMaxSuggestions) result.removeLast();
    for (int i = result.size() - 1; i >= 0; --i) {
        if (result.at(i).english.size() > kMaxEnglishLength) result.removeAt(i);
    }
    return result;
}
