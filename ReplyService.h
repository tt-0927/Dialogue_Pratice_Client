/*
 * 文件名:    ReplyService.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CReplyService 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CReplyService.h */

/* 智能提示回复：根据最近几条对话，让 LLM 给出 3 条适合学习者说的英文短句， */
/* 并同时给出对应中文译文（同一次请求产出，避免逐条再翻一遍的额外延迟）。 */
/* 与 CTranslationService 共用同一个 CLlmClient，靠 requestId 区分各自的响应。 */
#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include "ChatTypes.h"

class CLlmClient;

class CReplyService : public QObject {
    Q_OBJECT

public:
    explicit CReplyService(QObject* parent = nullptr);

    void setClient(CLlmClient* client);
    bool isEnabled() const;

    /* 传入最近几条对话（按时间正序），返回 3 条建议（含中文译文）。 */
    void request(const QVector<ChatMessage_S>& context);

    /* 解析模型输出：优先按对象数组 {en, zh}，其次字符串数组，最后按行拆分。 */
    static QVector<Suggestion_S> parseSuggestions(const QString& raw);

signals:
    void suggestionsReady(const QVector<Suggestion_S>& items);
    void failed(const QString& reason);

private:
    CLlmClient* m_pClient = nullptr;
    int        m_nCurrentId = 0;
    bool       m_nBusy = false;
};
