/*
 * 文件名:    LlmClient.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CLlmClient 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CLlmClient.h */

/* OpenAI 兼容的 chat/completions 客户端。 */
/* 既能连本地 llama-server（http://127.0.0.1:8080），也能连云端（DeepSeek/通义等）， */
/* 差别只在 baseUrl / apiKey / model 三项配置。 */
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class CLlmClient : public QObject {
    Q_OBJECT

public:
    struct Config_S {
        QString baseUrl  = QStringLiteral("http://127.0.0.1:8080");
        /* 云端才需要 */
        QString apiKey;
        QString model    = QStringLiteral("deepseek-chat");
        double  temperature = 0.2;
        /* low / medium / high；留空则不传 */
        QString reasoningEffort;
        int     timeoutMs = 40000;
        bool    enabled   = true;
    };

    explicit CLlmClient(QObject* parent = nullptr);

    void setConfig(const Config_S& config) { m_stConfig = config; }
    const Config_S& config() const { return m_stConfig; }
    bool isEnabled() const { return m_stConfig.enabled && !m_stConfig.baseUrl.isEmpty(); }

    /* 发一条对话请求，返回 requestId；返回 0 表示未发出（未启用）。 */
    int chat(const QString& system, const QString& user, int maxTokens = 256);

signals:
    void finished(int requestId, const QString& text);
    void failed(int requestId, const QString& reason);

private:
    void handleReply(QNetworkReply* reply, int requestId);

    Config_S                 m_stConfig;
    QNetworkAccessManager* m_pNam = nullptr;
    int                    m_nNextId = 1;
};
