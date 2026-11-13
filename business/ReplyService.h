// ReplyService.h
//
// 智能提示回复：根据最近几条对话，让 LLM 给出 3 条适合学习者说的英文短句，
// 并同时给出对应中文译文（同一次请求产出，避免逐条再翻一遍的额外延迟）。
// 与 TranslationService 共用同一个 LlmClient，靠 requestId 区分各自的响应。
#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include "business/ChatTypes.h"

class LlmClient;

class ReplyService : public QObject {
    Q_OBJECT

public:
    explicit ReplyService(QObject* parent = nullptr);

    void setClient(LlmClient* client);
    bool isEnabled() const;

    /// 传入最近几条对话（按时间正序），返回 3 条建议（含中文译文）。
    void request(const QVector<ChatMessage>& context);

    /// 解析模型输出：优先按对象数组 {en, zh}，其次字符串数组，最后按行拆分。
    static QVector<Suggestion> parseSuggestions(const QString& raw);

signals:
    void suggestionsReady(const QVector<Suggestion>& items);
    void failed(const QString& reason);

private:
    LlmClient* client_ = nullptr;
    int        currentId_ = 0;
    bool       busy_ = false;
};
