/*
 * 文件名:    TranslationService.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CTranslationService 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CTranslationService.h */

/* 翻译调度：串行队列 + 内存缓存 + 失败不阻塞后续。 */
/* 一次只发一个请求——本地 1.5B 模型并发跑反而更慢，串行还能保证译文顺序。 */
#pragma once

#include <QHash>
#include <QSet>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

class CLlmClient;

class CTranslationService : public QObject {
    Q_OBJECT

public:
    explicit CTranslationService(QObject* parent = nullptr);

    void setClient(CLlmClient* client);
    bool isEnabled() const;

    /* 把一条他人的英文消息排进翻译队列（重复文本命中缓存直接返回）。 */
    void requestMessage(qint64 seq, const QString& english);

    /* 输入框里的中文 → 英文（单条，高优先级）。 */
    void requestInput(const QString& chinese);

    static bool looksEnglish(const QString& text);
    static bool looksChinese(const QString& text);

signals:
    void messageTranslated(qint64 seq, const QString& chinese);
    void inputTranslated(const QString& sourceChinese, const QString& english);
    void failed(qint64 seq, const QString& reason);

private:
    enum class Kind_EN { Message, Input };

    struct Job_S {
        Kind_EN    kind = Kind_EN::Message;
        qint64  seq = 0;
        QString text;
    };

    void enqueue(const Job_S& job);
    void pump();
    void deliver(const Job_S& job, const QString& result);
    void onFinished(int requestId, const QString& text);
    void onFailed(int requestId, const QString& reason);
    void cachePut(const QString& key, const QString& value);

    static QString cacheKey(Kind_EN kind, const QString& text);
    static QString cleanOutput(const QString& raw);

    CLlmClient*             m_pClient = nullptr;
    QVector<Job_S>           queue_;
    QHash<QString, QString> m_strCache;
    QList<QString>         m_strCacheOrder;
    QSet<QString>          m_strInFlight;
    Job_S                    current_;
    int                    m_nCurrentId = 0;
    bool                   m_nBusy = false;
};
