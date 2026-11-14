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
 * 功能描述:  获取englishonly。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    返回 Suggestion_S 类型的处理结果，具体含义见功能描述。
 */
Suggestion_S englishOnly(const QString& strText) {
    Suggestion_S stItem;
    stItem.english = strText;
    return stItem;
}

/* namespace */
}

/*
 * 功能描述:  构造 CReplyService 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject* parent) : QObject(。
 * return:    无返回值（构造函数或析构函数）。
 */
CReplyService::CReplyService(QObject* pParent) : QObject(pParent) {}

/*
 * 功能描述:  设置客户端；实现上连接 Qt 信号与槽。
 * 作者:      JaTin
 * param [in]   pClient          客户端，类型 CLlmClient*。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CReplyService::setClient(CLlmClient* pClient) {
    if (m_pClient != nullptr) {
        disconnect(m_pClient, nullptr, this, nullptr);
    }
    m_pClient = pClient;
    if (m_pClient == nullptr) return;

    connect(m_pClient, &CLlmClient::finished, this, [this](int requestId, const QString& strText) {
        /* 不是本次请求 */
        if (requestId != m_nCurrentId) return;
        m_nBusy = false;
        const QVector<Suggestion_S> aItems = parseSuggestions(strText);
        if (aItems.isEmpty()) {
            emit failed(QStringLiteral("模型没有给出可用的建议"));
            return;
        }
        emit suggestionsReady(aItems);
    });
    connect(m_pClient, &CLlmClient::failed, this, [this](int requestId, const QString& reason) {
        if (requestId != m_nCurrentId) return;
        m_nBusy = false;
        emit failed(reason);
    });
}

/*
 * 功能描述:  判断启用状态。
 * 作者:      JaTin
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CReplyService::isEnabled() const {
    return m_pClient != nullptr && m_pClient->isEnabled();
}

/*
 * 功能描述:  请求request。
 * 作者:      JaTin
 * param [in]   aContext         上下文，类型 const QVector<ChatMessage_S>&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CReplyService::request(const QVector<ChatMessage_S>& aContext) {
    if (!isEnabled() || m_nBusy || aContext.isEmpty()) return;

    QString strDialog;
    for (const ChatMessage_S& stMessage : aContext) {
        strDialog += QStringLiteral("%1: %2\n").arg(stMessage.name, stMessage.text);
    }

    const QString strUser =
        QStringLiteral("对话上下文：\n%1\n请给出 3 条「我」可以回复对方的话，每条带中文翻译。").arg(strDialog);

    m_nBusy = true;
    /* 每条要输出英文 + 中文，token 比纯英文多，上限提到 400 */
    m_nCurrentId = m_pClient->chat(QString::fromUtf8(kSystemPrompt), strUser, 400);
    if (m_nCurrentId <= 0) {
        m_nBusy = false;
        emit failed(QStringLiteral("LLM 未启用"));
    }
}

/*
 * 功能描述:  解析智能建议列表；实现上按协议字段组装或解析 JSON。
 * 作者:      JaTin
 * param [in]   strRaw           输入数据，类型 const QString&。
 * return:    返回 QVector<Suggestion_S> 类型的处理结果，具体含义见功能描述。
 */
QVector<Suggestion_S> CReplyService::parseSuggestions(const QString& strRaw) {
    QVector<Suggestion_S> aResult;

    QString strText = strRaw.trimmed();
    const int nBegin = strText.indexOf(QLatin1Char('['));
    const int nEnd = strText.lastIndexOf(QLatin1Char(']'));

    /* 1) 优先按 JSON 数组解析（模型可能包一层代码块） */
    if (nBegin >= 0 && nEnd > nBegin) {
        const QByteArray byCandidate = strText.mid(nBegin, nEnd - nBegin + 1).toUtf8();
        QJsonParseError stParseError{};
        const QJsonDocument stDoc = QJsonDocument::fromJson(byCandidate, &stParseError);
        if (stParseError.error == QJsonParseError::NoError && stDoc.isArray()) {
            for (const QJsonValue& stValue : stDoc.array()) {
                if (stValue.isObject()) {
                    /* 新格式：{"en": "...", "zh": "..."} */
                    const QJsonObject stObj = stValue.toObject();
                    Suggestion_S strItem;
                    strItem.english = stObj.value(QStringLiteral("en")).toString().trimmed();
                    strItem.chinese = stObj.value(QStringLiteral("zh")).toString().trimmed();
                    if (strItem.english.isEmpty()) {
                        /* 模型偶尔把字段写成 english / chinese */
                        strItem.english = stObj.value(QStringLiteral("english")).toString().trimmed();
                        strItem.chinese = stObj.value(QStringLiteral("chinese")).toString().trimmed();
                    }
                    if (!strItem.english.isEmpty()) aResult.append(strItem);
                } else {
                    /* 兼容旧格式：纯字符串数组 */
                    const QString strItem = stValue.toString().trimmed();
                    if (!strItem.isEmpty()) aResult.append(englishOnly(strItem));
                }
            }
        }
    }

    /* 2) 退化成按行拆分，去掉 "1." "- " 这类前缀 */
    if (aResult.isEmpty()) {
        static const QRegularExpression stPrefix(
            QStringLiteral("^\\s*(?:[-*•]|\\d+[.、)])\\s*"));
        for (const QString& strLine : strText.split(QLatin1Char('\n'))) {
            QString strItem = strLine.trimmed();
            strItem.remove(stPrefix);
            strItem.remove(QLatin1Char('"'));
            if (strItem.isEmpty() || strItem.startsWith(QLatin1Char('[')) || strItem.startsWith(QLatin1Char(']'))) {
                continue;
            }
            aResult.append(englishOnly(strItem));
        }
    }

    /* 最多 3 条，太长的不像建议 */
    while (aResult.size() > kMaxSuggestions) aResult.removeLast();
    for (int i = aResult.size() - 1; i >= 0; --i) {
        if (aResult.at(i).english.size() > kMaxEnglishLength) aResult.removeAt(i);
    }
    return aResult;
}
