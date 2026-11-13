/*
 * 文件名:    TranslationService.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CTranslationService 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CTranslationService.cpp */
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

/* namespace */
}

CTranslationService::CTranslationService(QObject* parent) : QObject(parent) {}

/*
 * 功能描述:  执行 setClient 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  client  参数。
 * return:    void
 */
void CTranslationService::setClient(CLlmClient* client) {
    if (m_pClient != nullptr) {
        disconnect(m_pClient, nullptr, this, nullptr);
    }
    m_pClient = client;
    if (m_pClient != nullptr) {
        connect(m_pClient, &CLlmClient::finished, this, &CTranslationService::onFinished);
        connect(m_pClient, &CLlmClient::failed, this, &CTranslationService::onFailed);
    }
}

/*
 * 功能描述:  执行 isEnabled 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool CTranslationService::isEnabled() const {
    return m_pClient != nullptr && m_pClient->isEnabled();
}

/*
 * 功能描述:  执行 looksEnglish 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    返回值，含义见功能描述。
 */
bool CTranslationService::looksEnglish(const QString& text) {
    int latinRun = 0;
    bool hasLatinWord = false;
    for (const QChar c : text) {
        /* 含汉字，不是纯英文 */
        if (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF) return false;
        if (c.isLetter() && c.unicode() < 128) {
            if (++latinRun >= 2) hasLatinWord = true;
        } else {
            latinRun = 0;
        }
    }
    return hasLatinWord;
}

/*
 * 功能描述:  执行 looksChinese 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    返回值，含义见功能描述。
 */
bool CTranslationService::looksChinese(const QString& text) {
    for (const QChar c : text) {
        if (c.unicode() >= 0x4E00 && c.unicode() <= 0x9FFF) return true;
    }
    return false;
}

/*
 * 功能描述:  执行 requestMessage 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  seq  参数。
 * param [in]  english  参数。
 * return:    void
 */
void CTranslationService::requestMessage(qint64 seq, const QString& english) {
    if (!isEnabled() || english.trimmed().isEmpty()) return;
    if (!looksEnglish(english)) return;

    Job_S job;
    job.kind = Kind_EN::Message;
    job.seq = seq;
    job.text = english.trimmed();
    enqueue(job);
}

/*
 * 功能描述:  执行 requestInput 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  chinese  参数。
 * return:    void
 */
void CTranslationService::requestInput(const QString& chinese) {
    if (!isEnabled()) return;
    const QString text = chinese.trimmed();
    if (text.isEmpty() || !looksChinese(text)) return;

    /* 用户连续打字时只保留最新一条，避免排一堆过期的输入翻译 */
    for (int i = queue_.size() - 1; i >= 0; --i) {
        if (queue_.at(i).kind == Kind_EN::Input) queue_.removeAt(i);
    }

    Job_S job;
    job.kind = Kind_EN::Input;
    job.seq = 0;
    job.text = text;
    enqueue(job);
}

/*
 * 功能描述:  执行 cacheKey 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  kind  参数。
 * param [in]  text  参数。
 * return:    返回值，含义见功能描述。
 */
QString CTranslationService::cacheKey(Kind_EN kind, const QString& text) {
    return (kind == Kind_EN::Message ? QStringLiteral("M:") : QStringLiteral("I:")) + text;
}

/*
 * 功能描述:  执行 cachePut 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  key  参数。
 * param [in]  value  参数。
 * return:    void
 */
void CTranslationService::cachePut(const QString& key, const QString& value) {
    if (m_strCache.contains(key)) return;
    m_strCache.insert(key, value);
    m_strCacheOrder.append(key);
    while (m_strCacheOrder.size() > kMaxCacheEntries) {
        m_strCache.remove(m_strCacheOrder.takeFirst());
    }
}

/*
 * 功能描述:  执行 enqueue 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  job  参数。
 * return:    void
 */
void CTranslationService::enqueue(const Job_S& job) {
    const QString key = cacheKey(job.kind, job.text);
    const auto hit = m_strCache.constFind(key);
    if (hit != m_strCache.constEnd()) {
        deliver(job, hit.value());
        return;
    }
    /* 同一条已在队列里，不重复请求 */
    if (m_strInFlight.contains(key)) return;
    m_strInFlight.insert(key);
    queue_.append(job);
    pump();
}

/*
 * 功能描述:  执行 pump 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CTranslationService::pump() {
    if (m_nBusy || queue_.isEmpty() || !isEnabled()) return;

    current_ = queue_.takeFirst();
    m_nBusy = true;

    const bool toChinese = (current_.kind == Kind_EN::Message);
    m_nCurrentId = m_pClient->chat(QString::fromUtf8(toChinese ? kMessageSystem : kInputSystem),
                               current_.text,
                               toChinese ? 256 : 200);

    if (m_nCurrentId <= 0) {
        m_nBusy = false;
        m_strInFlight.remove(cacheKey(current_.kind, current_.text));
        emit failed(current_.seq, QStringLiteral("翻译服务未启用"));
    }
}

/*
 * 功能描述:  执行 onFinished 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  requestId  参数。
 * param [in]  text  参数。
 * return:    void
 */
void CTranslationService::onFinished(int requestId, const QString& text) {
    /* 过期响应（理论上不会发生，串行队列） */
    if (requestId != m_nCurrentId) return;

    m_nBusy = false;
    m_strInFlight.remove(cacheKey(current_.kind, current_.text));

    const QString result = cleanOutput(text);
    if (result.isEmpty()) {
        qWarning() << "translation empty result for:" << current_.text;
        emit failed(current_.seq, QStringLiteral("模型返回空译文"));
        pump();
        return;
    }

    /* 小模型偶尔会把原文原样吐回来（尤其含下划线标识符的短句）， */
    /* 这种"译文"没有意义，按失败处理更诚实。 */
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

/*
 * 功能描述:  执行 onFailed 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  requestId  参数。
 * param [in]  reason  参数。
 * return:    void
 */
void CTranslationService::onFailed(int requestId, const QString& reason) {
    if (requestId != m_nCurrentId) return;
    m_nBusy = false;
    m_strInFlight.remove(cacheKey(current_.kind, current_.text));
    qWarning() << "LLM request failed:" << reason;
    emit failed(current_.seq, reason);
    /* 一条失败不影响后面的 */
    pump();
}

/*
 * 功能描述:  执行 deliver 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  job  参数。
 * param [in]  result  参数。
 * return:    void
 */
void CTranslationService::deliver(const Job_S& job, const QString& result) {
    if (job.kind == Kind_EN::Message) {
        emit messageTranslated(job.seq, result);
    } else {
        emit inputTranslated(job.text, result);
    }
}

/*
 * 功能描述:  执行 cleanOutput 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  raw  参数。
 * return:    返回值，含义见功能描述。
 */
QString CTranslationService::cleanOutput(const QString& raw) {
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

    /* 去掉整体包裹的引号 */
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
