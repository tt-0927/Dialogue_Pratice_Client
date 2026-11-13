// TranslationService.cpp
#include "TranslationService.h"

#include "LlmClient.h"

#include <QDebug>

namespace {

constexpr int kMaxCacheEntries = 500;

const char* kMessageSystem =
    "你是英汉翻译引擎。把用户给出的英文翻译成自然、口语化的中文。"
    "只输出译文本身：不要解释，不要加引号，不要重复原文，不要输出拼音。";

const char* kInputSystem =
    "你是汉英翻译引擎。把用户给出的中文翻译成自然、口语化的英文（日常对话语气，不要书面语）。"
    "只输出英文译文：不要解释，不要加引号，不要重复原文。";

}  // namespace

TranslationService::TranslationService(QObject* parent) : QObject(parent) {}

void TranslationService::setClient(LlmClient* client) {
    if (client_ != nullptr) {
        disconnect(client_, nullptr, this, nullptr);
    }
    client_ = client;
    if (client_ != nullptr) {
        connect(client_, &LlmClient::finished, this, &TranslationService::onFinished);
        connect(client_, &LlmClient::failed, this, &TranslationService::onFailed);
    }
}

bool TranslationService::isEnabled() const {
    return client_ != nullptr && client_->isEnabled();
}

bool TranslationService::looksEnglish(const QString& text) {
    int latinRun = 0;
    bool hasLatinWord = false;
    for (const QChar c : text) {
        if (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF) return false;   // 含汉字，不是纯英文
        if (c.isLetter() && c.unicode() < 128) {
            if (++latinRun >= 2) hasLatinWord = true;
        } else {
            latinRun = 0;
        }
    }
    return hasLatinWord;
}

bool TranslationService::looksChinese(const QString& text) {
    for (const QChar c : text) {
        if (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF) return true;
    }
    return false;
}

void TranslationService::requestMessage(qint64 seq, const QString& english) {
    if (!isEnabled() || english.trimmed().isEmpty()) return;
    if (!looksEnglish(english)) return;

    Job job;
    job.kind = Kind::Message;
    job.seq = seq;
    job.text = english.trimmed();
    enqueue(job);
}

void TranslationService::requestInput(const QString& chinese) {
    if (!isEnabled()) return;
    const QString text = chinese.trimmed();
    if (text.isEmpty() || !looksChinese(text)) return;

    // 用户连续打字时只保留最新一条，避免排一堆过期的输入翻译
    for (int i = queue_.size() - 1; i >= 0; --i) {
        if (queue_.at(i).kind == Kind::Input) queue_.removeAt(i);
    }

    Job job;
    job.kind = Kind::Input;
    job.seq = 0;
    job.text = text;
    enqueue(job);
}

QString TranslationService::cacheKey(Kind kind, const QString& text) {
    return (kind == Kind::Message ? QStringLiteral("M:") : QStringLiteral("I:")) + text;
}

void TranslationService::cachePut(const QString& key, const QString& value) {
    if (cache_.contains(key)) return;
    cache_.insert(key, value);
    cacheOrder_.append(key);
    while (cacheOrder_.size() > kMaxCacheEntries) {
        cache_.remove(cacheOrder_.takeFirst());
    }
}

void TranslationService::enqueue(const Job& job) {
    const QString key = cacheKey(job.kind, job.text);
    const auto hit = cache_.constFind(key);
    if (hit != cache_.constEnd()) {
        deliver(job, hit.value());
        return;
    }
    if (inFlight_.contains(key)) return;   // 同一条已在队列里，不重复请求
    inFlight_.insert(key);
    queue_.append(job);
    pump();
}

void TranslationService::pump() {
    if (busy_ || queue_.isEmpty() || !isEnabled()) return;

    current_ = queue_.takeFirst();
    busy_ = true;

    const bool toChinese = (current_.kind == Kind::Message);
    currentId_ = client_->chat(QString::fromUtf8(toChinese ? kMessageSystem : kInputSystem),
                               current_.text,
                               toChinese ? 256 : 200);

    if (currentId_ <= 0) {
        busy_ = false;
        inFlight_.remove(cacheKey(current_.kind, current_.text));
        emit failed(current_.seq, QStringLiteral("翻译服务未启用"));
    }
}

void TranslationService::onFinished(int requestId, const QString& text) {
    if (requestId != currentId_) return;   // 过期响应（理论上不会发生，串行队列）

    busy_ = false;
    inFlight_.remove(cacheKey(current_.kind, current_.text));

    const QString result = cleanOutput(text);
    if (result.isEmpty()) {
        qWarning() << "translation empty result for:" << current_.text;
        emit failed(current_.seq, QStringLiteral("模型返回空译文"));
        pump();
        return;
    }

    // 小模型偶尔会把原文原样吐回来（尤其含下划线标识符的短句），
    // 这种"译文"没有意义，按失败处理更诚实。
    if (result.compare(current_.text, Qt::CaseInsensitive) == 0) {
        qWarning() << "translation returned source text unchanged:" << current_.text;
        emit failed(current_.seq, QStringLiteral("模型没有翻译，返回了原文"));
        pump();
        return;
    }

    cachePut(cacheKey(current_.kind, current_.text), result);
    deliver(current_, result);
    pump();
}

void TranslationService::onFailed(int requestId, const QString& reason) {
    if (requestId != currentId_) return;
    busy_ = false;
    inFlight_.remove(cacheKey(current_.kind, current_.text));
    qWarning() << "LLM request failed:" << reason;
    emit failed(current_.seq, reason);
    pump();   // 一条失败不影响后面的
}

void TranslationService::deliver(const Job& job, const QString& result) {
    if (job.kind == Kind::Message) {
        emit messageTranslated(job.seq, result);
    } else {
        emit inputTranslated(job.text, result);
    }
}

QString TranslationService::cleanOutput(const QString& raw) {
    QString s = raw.trimmed();

    static const QStringList prefixes = {
        QStringLiteral("译文："), QStringLiteral("译文:"),
        QStringLiteral("翻译："), QStringLiteral("翻译:"),
        QStringLiteral("中文："), QStringLiteral("中文:"),
        QStringLiteral("英文："), QStringLiteral("英文:"),
        QStringLiteral("Translation:"), QStringLiteral("English:"),
    };
    for (const QString& prefix : prefixes) {
        if (s.startsWith(prefix, Qt::CaseInsensitive)) {
            s = s.mid(prefix.size()).trimmed();
            break;
        }
    }

    // 去掉整体包裹的引号
    if (s.size() >= 2) {
        const QChar first = s.front();
        const QChar last = s.back();
        const bool asciiQuoted = (first == QLatin1Char('"') && last == QLatin1Char('"'))
                                 || (first == QLatin1Char('\'') && last == QLatin1Char('\''));
        const bool cjkQuoted = (first.unicode() == 0x201C && last.unicode() == 0x201D)
                               || (first.unicode() == 0x300C && last.unicode() == 0x300D);
        if (asciiQuoted || cjkQuoted) {
            s = s.mid(1, s.size() - 2).trimmed();
        }
    }

    return s;
}
