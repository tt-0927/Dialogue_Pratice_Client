// TranslationService.h
//
// 翻译调度：串行队列 + 内存缓存 + 失败不阻塞后续。
// 一次只发一个请求——本地 1.5B 模型并发跑反而更慢，串行还能保证译文顺序。
#pragma once

#include <QHash>
#include <QSet>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

class LlmClient;

class TranslationService : public QObject {
    Q_OBJECT

public:
    explicit TranslationService(QObject* parent = nullptr);

    void setClient(LlmClient* client);
    bool isEnabled() const;

    /// 把一条他人的英文消息排进翻译队列（重复文本命中缓存直接返回）。
    void requestMessage(qint64 seq, const QString& english);

    /// 输入框里的中文 → 英文（单条，高优先级）。
    void requestInput(const QString& chinese);

    static bool looksEnglish(const QString& text);
    static bool looksChinese(const QString& text);

signals:
    void messageTranslated(qint64 seq, const QString& chinese);
    void inputTranslated(const QString& sourceChinese, const QString& english);
    void failed(qint64 seq, const QString& reason);

private:
    enum class Kind { Message, Input };

    struct Job {
        Kind    kind = Kind::Message;
        qint64  seq = 0;
        QString text;
    };

    void enqueue(const Job& job);
    void pump();
    void deliver(const Job& job, const QString& result);
    void onFinished(int requestId, const QString& text);
    void onFailed(int requestId, const QString& reason);
    void cachePut(const QString& key, const QString& value);

    static QString cacheKey(Kind kind, const QString& text);
    static QString cleanOutput(const QString& raw);

    LlmClient*             client_ = nullptr;
    QVector<Job>           queue_;
    QHash<QString, QString> cache_;
    QList<QString>         cacheOrder_;
    QSet<QString>          inFlight_;
    Job                    current_;
    int                    currentId_ = 0;
    bool                   busy_ = false;
};
