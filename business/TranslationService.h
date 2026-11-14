/**
 * @file       TranslationService.h
 * @brief      翻译调度：串行队列 + 内存缓存 + 相同文本合并。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

#pragma once

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

class CLlmClient;

/**
 * @brief   中英双向翻译服务。
 * @details 两类任务共用一条串行队列： - `Message`：他人英文消息 -> 中文，结果按 `seq` 回填 - `Input`：输入框中文 -> 英文，结果按原文回传 相同文本会合并：若同一句已在请求中，后来的请求只登记等待， 等首个请求返回后一起回调——这样重复句子不会各自发一次请求， 也不会出现「第 2 条永远停在翻译中」的悬挂状态。
 * @author  JaTin
 */
class CTranslationService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CTranslationService(QObject* parent = nullptr);

    /**
     * @brief      绑定 LLM 客户端。
     * @param[in] client         客户端实例，不取得所有权；可为空表示禁用；类型 CLlmClient*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setClient(CLlmClient* client);

    /**
     * @brief      翻译服务是否可用（已绑定客户端且客户端已启用）。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool isEnabled() const;

    /**
     * @brief      把一条他人的英文消息排进翻译队列。
     * @details    命中缓存立即回调；同一文本已在请求中则登记等待，不重复请求。
     * @param[in] seq            消息序号，结果通过 `messageTranslated` 带回；类型 qint64。
     * @param[in] english        英文原文；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestMessage(qint64 seq, const QString& english);

    /**
     * @brief      把输入框里的中文翻成英文。
     * @details    会先清掉队列里尚未执行的旧输入任务，只保留最新一条。
     * @param[in] chinese        中文原文；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestInput(const QString& chinese);

    /**
     * @brief      判断文本是否以英文为主。
     * @param[in] text           待判断文本；类型 const QString&。
     * @return     true  不含汉字且至少有一个拉丁词；false 含汉字，或不构成单词。
     * @author     JaTin
     */
    static bool looksEnglish(const QString& text);

    /**
     * @brief      判断文本是否含汉字。
     * @param[in] text           待判断文本；类型 const QString&。
     * @return     true  至少含一个 CJK 统一汉字。
     * @author     JaTin
     */
    static bool looksChinese(const QString& text);

signals:
    /**
     * @brief      消息翻译完成。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @param[in] chinese        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void messageTranslated(qint64 seq, const QString& chinese);

    /**
     * @brief      输入翻译完成。
     * @param[in] sourceChinese  输入参数；类型 const QString&。
     * @param[in] english        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void inputTranslated(const QString& sourceChinese, const QString& english);

    /**
     * @brief      翻译失败。
     * @param[in] seq            消息序号；0 表示这是输入框翻译；类型 qint64。
     * @param[in] reason         失败原因，可直接展示给用户；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(qint64 seq, const QString& reason);

private:
    /**
     * @brief  任务类型。
     */
    enum class Kind_EN { Message, Input };

    /**
     * @brief  一次翻译任务。
     */
    struct Job_S {
        /**
         * @brief  任务类型
         * @note   类型 Kind_EN；类/结构体成员变量。
         */
        Kind_EN    kind = Kind_EN::Message;
        /**
         * @brief  消息序号（Input 任务为 0）
         * @note   类型 qint64；类/结构体成员变量。
         */
        qint64  seq = 0;
        /**
         * @brief  原文
         * @note   类型 QString；类/结构体成员变量。
         */
        QString text;
    };

    /**
     * @brief      把任务放入队列或登记为等待者。
     * @param[in] job            任务；类型 const Job_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void enqueue(const Job_S& job);

    /**
     * @brief      队列空闲时取下一个任务发出。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void pump();

    /**
     * @brief      把结果发给任务对应的接收方。
     * @param[in] job            任务；类型 const Job_S&。
     * @param[in] result         译文；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void deliver(const Job_S& job, const QString& result);

    /**
     * @brief      处理 LLM 成功响应。
     * @param[in] requestId      请求 id，用于丢弃过期响应；类型 int。
     * @param[in] text           模型输出；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onFinished(int requestId, const QString& text);

    /**
     * @brief      处理 LLM 失败响应。
     * @param[in] requestId      请求 id；类型 int。
     * @param[in] reason         失败原因；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onFailed(int requestId, const QString& reason);

    /**
     * @brief      写入缓存，超出上限时淘汰最旧的。
     * @param[in] key            缓存键；类型 const QString&。
     * @param[in] value          译文；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void cachePut(const QString& key, const QString& value);

    /**
     * @brief      构造缓存键（区分两个方向）。
     * @param[in] kind           消息类型；类型 Kind_EN。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    static QString cacheKey(Kind_EN kind, const QString& text);

    /**
     * @brief      清洗模型输出。
     * @param[in] raw            模型原始输出；类型 const QString&。
     * @return     去掉「译文：」这类前缀与整体引号后的文本。
     * @author     JaTin
     */
    static QString cleanOutput(const QString& raw);

    /**
     * @brief  LLM 客户端（不持有）
     * @note   类型 CLlmClient*；类/结构体成员变量。
     */
    CLlmClient*              m_pClient = nullptr;
    /**
     * @brief  待执行任务
     * @note   类型 QVector<Job_S>；类/结构体成员变量。
     */
    QVector<Job_S>            queue_;
    /**
     * @brief  译文缓存
     * @note   类型 QHash<QString, QString>；类/结构体成员变量。
     */
    QHash<QString, QString> m_strCache;
    /**
     * @brief  缓存插入顺序，用于淘汰
     * @note   类型 QList<QString>；类/结构体成员变量。
     */
    QList<QString>          m_strCacheOrder;

    /**
     * @brief  正在请求中的文本 -> 等待同一译文的全部任务。
     * @note   类型 QHash<QString, QVector<Job_S>>；类/结构体成员变量。
     */
    QHash<QString, QVector<Job_S>> m_strInFlight;

    /**
     * @brief  正在执行的任务
     * @note   类型 Job_S；类/结构体成员变量。
     */
    Job_S   current_;
    /**
     * @brief  当前请求 id
     * @note   类型 int；类/结构体成员变量。
     */
    int   m_nCurrentId = 0;
    /**
     * @brief  是否已有请求在途
     * @note   类型 bool；类/结构体成员变量。
     */
    bool  m_nBusy = false;
};
