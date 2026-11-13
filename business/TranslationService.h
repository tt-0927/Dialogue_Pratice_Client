/*
 * 文件名:    TranslationService.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  翻译调度：串行队列 + 内存缓存 + 相同文本合并。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

class CLlmClient;

/**
 * @brief 中英双向翻译服务。
 *
 * @details 两类任务共用一条串行队列：
 *          - `Message`：他人英文消息 -> 中文，结果按 `seq` 回填
 *          - `Input`：输入框中文 -> 英文，结果按原文回传
 *
 *          相同文本会合并：若同一句已在请求中，后来的请求只登记等待，
 *          等首个请求返回后一起回调——这样重复句子不会各自发一次请求，
 *          也不会出现「第 2 条永远停在翻译中」的悬挂状态。
 */
class CTranslationService : public QObject {
    Q_OBJECT

public:
    explicit CTranslationService(QObject* parent = nullptr);

    /**
     * @brief 绑定 LLM 客户端。
     * @param client 客户端实例，不取得所有权；可为空表示禁用。
     */
    void setClient(CLlmClient* client);

    /* 翻译服务是否可用（已绑定客户端且客户端已启用）。 */
    bool isEnabled() const;

    /**
     * @brief 把一条他人的英文消息排进翻译队列。
     * @param seq     消息序号，结果通过 `messageTranslated` 带回。
     * @param english 英文原文。
     * @details 命中缓存立即回调；同一文本已在请求中则登记等待，不重复请求。
     */
    void requestMessage(qint64 seq, const QString& english);

    /**
     * @brief 把输入框里的中文翻成英文。
     * @param chinese 中文原文。
     * @details 会先清掉队列里尚未执行的旧输入任务，只保留最新一条。
     */
    void requestInput(const QString& chinese);

    /**
     * @brief 判断文本是否以英文为主。
     * @param text 待判断文本。
     * @retval true  不含汉字且至少有一个拉丁词。
     * @retval false 含汉字，或不构成单词。
     */
    static bool looksEnglish(const QString& text);

    /**
     * @brief 判断文本是否含汉字。
     * @param text 待判断文本。
     * @retval true  至少含一个 CJK 统一汉字。
     */
    static bool looksChinese(const QString& text);

signals:
    /* 消息翻译完成。 */
    void messageTranslated(qint64 seq, const QString& chinese);

    /* 输入翻译完成。 */
    void inputTranslated(const QString& sourceChinese, const QString& english);

    /**
     * @brief 翻译失败。
     * @param seq    消息序号；0 表示这是输入框翻译。
     * @param reason 失败原因，可直接展示给用户。
     */
    void failed(qint64 seq, const QString& reason);

private:
    /* 任务类型。 */
    enum class Kind_EN { Message, Input };

    /* 一次翻译任务。 */
    struct Job_S {
        /* < 任务类型 */
        Kind_EN    kind = Kind_EN::Message;
        /* < 消息序号（Input 任务为 0） */
        qint64  seq = 0;
        /* < 原文 */
        QString text;
    };

    /**
     * @brief 把任务放入队列或登记为等待者。
     * @param job 任务。
     */
    void enqueue(const Job_S& job);

    /* 队列空闲时取下一个任务发出。 */
    void pump();

    /**
     * @brief 把结果发给任务对应的接收方。
     * @param job    任务。
     * @param result 译文。
     */
    void deliver(const Job_S& job, const QString& result);

    /**
     * @brief 处理 LLM 成功响应。
     * @param requestId 请求 id，用于丢弃过期响应。
     * @param text      模型输出。
     */
    void onFinished(int requestId, const QString& text);

    /**
     * @brief 处理 LLM 失败响应。
     * @param requestId 请求 id。
     * @param reason    失败原因。
     */
    void onFailed(int requestId, const QString& reason);

    /**
     * @brief 写入缓存，超出上限时淘汰最旧的。
     * @param key   缓存键。
     * @param value 译文。
     */
    void cachePut(const QString& key, const QString& value);

    /* 构造缓存键（区分两个方向）。 */
    static QString cacheKey(Kind_EN kind, const QString& text);

    /**
     * @brief 清洗模型输出。
     * @param raw 模型原始输出。
     * @retval 去掉「译文：」这类前缀与整体引号后的文本。
     */
    static QString cleanOutput(const QString& raw);

    /* < LLM 客户端（不持有） */
    CLlmClient*              m_pClient = nullptr;
    /* < 待执行任务 */
    QVector<Job_S>            queue_;
    /* < 译文缓存 */
    QHash<QString, QString> m_strCache;
    /* < 缓存插入顺序，用于淘汰 */
    QList<QString>          m_strCacheOrder;

    /* 正在请求中的文本 -> 等待同一译文的全部任务。 */
    QHash<QString, QVector<Job_S>> m_strInFlight;

    /* < 正在执行的任务 */
    Job_S   current_;
    /* < 当前请求 id */
    int   m_nCurrentId = 0;
    /* < 是否已有请求在途 */
    bool  m_nBusy = false;
};
