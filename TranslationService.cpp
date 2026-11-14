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

/*
 * 功能描述:  构造 CTranslationService 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject* parent) : QObject(。
 * return:    无返回值（构造函数或析构函数）。
 */
CTranslationService::CTranslationService(QObject* pParent) : QObject(pParent) {}

/*
 * 功能描述:  设置客户端；实现上连接 Qt 信号与槽。
 * 作者:      JaTin
 * param [in]   pClient          客户端，类型 CLlmClient*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::setClient(CLlmClient* pClient) {
    if (m_pClient != nullptr) {
        disconnect(m_pClient, nullptr, this, nullptr);
    }
    m_pClient = pClient;
    if (m_pClient != nullptr) {
        connect(m_pClient, &CLlmClient::finished, this, &CTranslationService::onFinished);
        connect(m_pClient, &CLlmClient::failed, this, &CTranslationService::onFailed);
    }
}

/*
 * 功能描述:  判断启用状态。
 * 作者:      JaTin
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CTranslationService::isEnabled() const {
    return m_pClient != nullptr && m_pClient->isEnabled();
}

/*
 * 功能描述:  获取looksenglish；失败时返回 false。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CTranslationService::looksEnglish(const QString& strText) {
    int nLatinRun = 0;
    bool nHasLatinWord = false;
    for (const QChar stC : strText) {
        /* 含汉字，不是纯英文 */
        if (stC.unicode() >= 0x4E00 && stC.unicode() <= 0x9FFF) return false;
        if (stC.isLetter() && stC.unicode() < 128) {
            if (++nLatinRun >= 2) nHasLatinWord = true;
        } else {
            nLatinRun = 0;
        }
    }
    return nHasLatinWord;
}

/*
 * 功能描述:  获取lookschinese；失败时返回 false。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CTranslationService::looksChinese(const QString& strText) {
    for (const QChar stC : strText) {
        if (stC.unicode() >= 0x4E00 && stC.unicode() <= 0x9FFF) return true;
    }
    return false;
}

/*
 * 功能描述:  请求消息内容。
 * 作者:      JaTin
 * param [in]   lSeq             全局消息序号，类型 qint64。
 * param [in]   strEnglish       输入数据，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::requestMessage(qint64 lSeq, const QString& strEnglish) {
    if (!isEnabled() || strEnglish.trimmed().isEmpty()) return;
    if (!looksEnglish(strEnglish)) return;

    Job_S stJob;
    stJob.kind = Kind_EN::Message;
    stJob.seq = lSeq;
    stJob.text = strEnglish.trimmed();
    enqueue(stJob);
}

/*
 * 功能描述:  请求输入。
 * 作者:      JaTin
 * param [in]   strChinese       输入数据，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::requestInput(const QString& strChinese) {
    if (!isEnabled()) return;
    const QString strText = strChinese.trimmed();
    if (strText.isEmpty() || !looksChinese(strText)) return;

    /* 用户连续打字时只保留最新一条，避免排一堆过期的输入翻译 */
    for (int i = queue_.size() - 1; i >= 0; --i) {
        if (queue_.at(i).kind == Kind_EN::Input) queue_.removeAt(i);
    }

    Job_S stJob;
    stJob.kind = Kind_EN::Input;
    stJob.seq = 0;
    stJob.text = strText;
    enqueue(stJob);
}

/*
 * 功能描述:  根据任务类型与文本生成译文缓存键。
 * 作者:      JaTin
 * param [in]   enKind           消息类型，类型 Kind_EN。
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString CTranslationService::cacheKey(Kind_EN enKind, const QString& strText) {
    return (enKind == Kind_EN::Message ? QStringLiteral("M:") : QStringLiteral("I:")) + strText;
}

/*
 * 功能描述:  执行缓存 put。
 * 作者:      JaTin
 * param [in]   strKey           缓存键，类型 const QString&。
 * param [in]   strValue         待设置的数值，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::cachePut(const QString& strKey, const QString& strValue) {
    if (m_strCache.contains(strKey)) return;
    m_strCache.insert(strKey, strValue);
    m_strCacheOrder.append(strKey);
    while (m_strCacheOrder.size() > kMaxCacheEntries) {
        m_strCache.remove(m_strCacheOrder.takeFirst());
    }
}

/*
 * 功能描述:  入队enqueue。
 * 作者:      JaTin
 * param [in]   stJob            任务，类型 const Job_S&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::enqueue(const Job_S& stJob) {
    const QString strKey = cacheKey(stJob.kind, stJob.text);
    const auto stHit = m_strCache.constFind(strKey);
    if (stHit != m_strCache.constEnd()) {
        deliver(stJob, stHit.value());
        return;
    }
    /* 同一条已在队列里，不重复请求 */
    if (m_strInFlight.contains(strKey)) return;
    m_strInFlight.insert(strKey);
    queue_.append(stJob);
    pump();
}

/*
 * 功能描述:  驱动待处理队列，逐条取出任务并提交执行。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::pump() {
    if (m_nBusy || queue_.isEmpty() || !isEnabled()) return;

    current_ = queue_.takeFirst();
    m_nBusy = true;

    const bool nToChinese = (current_.kind == Kind_EN::Message);
    m_nCurrentId = m_pClient->chat(QString::fromUtf8(nToChinese ? kMessageSystem : kInputSystem),
                               current_.text,
                               nToChinese ? 256 : 200);

    if (m_nCurrentId <= 0) {
        m_nBusy = false;
        m_strInFlight.remove(cacheKey(current_.kind, current_.text));
        emit failed(current_.seq, QStringLiteral("翻译服务未启用"));
    }
}

/*
 * 功能描述:  异步请求完成回调，解析结果并分发给对应的等待方。
 * 作者:      JaTin
 * param [in]   nRequestId       请求编号，类型 int。
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::onFinished(int nRequestId, const QString& strText) {
    /* 过期响应（理论上不会发生，串行队列） */
    if (nRequestId != m_nCurrentId) return;

    m_nBusy = false;
    m_strInFlight.remove(cacheKey(current_.kind, current_.text));

    const QString strResult = cleanOutput(strText);
    if (strResult.isEmpty()) {
        qWarning() << "translation empty result for:" << current_.text;
        emit failed(current_.seq, QStringLiteral("模型返回空译文"));
        pump();
        return;
    }

    /* 小模型偶尔会把原文原样吐回来（尤其含下划线标识符的短句）， */
    /* 这种"译文"没有意义，按失败处理更诚实。 */
    if (strResult.compare(current_.text, Qt::CaseInsensitive) == 0) {
        qWarning() << "translation returned source text unchanged:" << current_.text;
        emit failed(current_.seq, QStringLiteral("模型没有翻译，返回了原文"));
        pump();
        return;
    }

    cachePut(cacheKey(current_.kind, current_.text), strResult);
    deliver(current_, strResult);
    pump();
}

/*
 * 功能描述:  异步请求失败回调，记录错误原因并通知上层。
 * 作者:      JaTin
 * param [in]   nRequestId       请求编号，类型 int。
 * param [in]   strReason        失败原因描述，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::onFailed(int nRequestId, const QString& strReason) {
    if (nRequestId != m_nCurrentId) return;
    m_nBusy = false;
    m_strInFlight.remove(cacheKey(current_.kind, current_.text));
    qWarning() << "LLM request failed:" << strReason;
    emit failed(current_.seq, strReason);
    /* 一条失败不影响后面的 */
    pump();
}

/*
 * 功能描述:  把处理完成的响应投递回对应的请求方。
 * 作者:      JaTin
 * param [in]   stJob            任务，类型 const Job_S&。
 * param [in]   strResult        处理结果，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CTranslationService::deliver(const Job_S& stJob, const QString& strResult) {
    if (stJob.kind == Kind_EN::Message) {
        emit messageTranslated(stJob.seq, strResult);
    } else {
        emit inputTranslated(stJob.text, strResult);
    }
}

/*
 * 功能描述:  清洗模型输出，去除多余符号与思考过程残留。
 * 作者:      JaTin
 * param [in]   strRaw           输入数据，类型 const QString&。
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString CTranslationService::cleanOutput(const QString& strRaw) {
    QString strS = strRaw.trimmed();

    static const QStringList strPrefixes = {
        QStringLiteral("译文："), QStringLiteral("译文:"),
        QStringLiteral("翻译："), QStringLiteral("翻译:"),
        QStringLiteral("中文："), QStringLiteral("中文:"),
        QStringLiteral("英文："), QStringLiteral("英文:"),
        QStringLiteral("Translation:"), QStringLiteral("English:"),
    };
    for (const QString& strPrefix : strPrefixes) {
        if (strS.startsWith(strPrefix, Qt::CaseInsensitive)) {
            strS = strS.mid(strPrefix.size()).trimmed();
            break;
        }
    }

    /* 去掉整体包裹的引号 */
    if (strS.size() >= 2) {
        const QChar stFirst = strS.front();
        const QChar stLast = strS.back();
        const bool nAsciiQuoted = (stFirst == QLatin1Char('"') && stLast == QLatin1Char('"'))
                                 || (stFirst == QLatin1Char('\'') && stLast == QLatin1Char('\''));
        const bool nCjkQuoted = (stFirst.unicode() == 0x201C && stLast.unicode() == 0x201D)
                               || (stFirst.unicode() == 0x300C && stLast.unicode() == 0x300D);
        if (nAsciiQuoted || nCjkQuoted) {
            strS = strS.mid(1, strS.size() - 2).trimmed();
        }
    }

    return strS;
}
