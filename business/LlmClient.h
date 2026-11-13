// LlmClient.h
//
// OpenAI 兼容的 chat/completions 客户端。
// 既能连本地 llama-server（http://127.0.0.1:8080），也能连云端（DeepSeek/通义等），
// 差别只在 baseUrl / apiKey / model 三项配置。
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

class LlmClient : public QObject {
    Q_OBJECT

public:
    struct Config {
        QString baseUrl  = QStringLiteral("http://127.0.0.1:8080");
        QString apiKey;                                        // 云端才需要
        QString model    = QStringLiteral("deepseek-chat");
        double  temperature = 0.2;
        QString reasoningEffort;   // low / medium / high；留空则不传
        int     timeoutMs = 40000;
        bool    enabled   = true;
    };

    explicit LlmClient(QObject* parent = nullptr);

    void setConfig(const Config& config) { config_ = config; }
    const Config& config() const { return config_; }
    bool isEnabled() const { return config_.enabled && !config_.baseUrl.isEmpty(); }

    /// 发一条对话请求，返回 requestId；返回 0 表示未发出（未启用）。
    int chat(const QString& system, const QString& user, int maxTokens = 256);

signals:
    void finished(int requestId, const QString& text);
    void failed(int requestId, const QString& reason);

private:
    void handleReply(QNetworkReply* reply, int requestId);

    Config                 config_;
    QNetworkAccessManager* nam_ = nullptr;
    int                    nextId_ = 1;
};
