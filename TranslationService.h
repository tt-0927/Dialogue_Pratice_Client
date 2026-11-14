/**
 * @file       TranslationService.h
 * @brief      CTranslationService 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CTranslationService.h。
 */

/**
 * @brief  翻译调度：串行队列 + 内存缓存 + 失败不阻塞后续。 一次只发一个请求——本地 1.5B 模型并发跑反而更慢，串行还能保证译文顺序。
 */
#pragma once

#include <QHash>
#include <QSet>
#include <QList>
#include <QObject>
#include <QString>
#include <QVector>

class CLlmClient;

/**
 * @brief   C译文业务服务。
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
     * @brief      设置客户端。
     * @param[in] client         客户端；类型 CLlmClient*。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setClient(CLlmClient* client);
    /**
     * @brief      判断启用状态。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool isEnabled() const;

    /**
     * @brief      把一条他人的英文消息排进翻译队列（重复文本命中缓存直接返回）。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @param[in] english        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestMessage(qint64 seq, const QString& english);

    /**
     * @brief      输入框里的中文 → 英文（单条，高优先级）。
     * @param[in] chinese        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void requestInput(const QString& chinese);

    /**
     * @brief      获取 looksenglish相关的数据并返回处理结果。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    static bool looksEnglish(const QString& text);
    /**
     * @brief      获取 lookschinese相关的数据并返回处理结果。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    static bool looksChinese(const QString& text);

signals:
    /**
     * @brief      执行消息内容 translated。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @param[in] chinese        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void messageTranslated(qint64 seq, const QString& chinese);
    /**
     * @brief      执行输入 translated。
     * @param[in] sourceChinese  输入参数；类型 const QString&。
     * @param[in] english        输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void inputTranslated(const QString& sourceChinese, const QString& english);
    /**
     * @brief      执行 failed相关的数据并返回处理结果。
     * @param[in] seq            全局消息序号；类型 qint64。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(qint64 seq, const QString& reason);

private:
    enum class Kind_EN { Message, Input };

    struct Job_S {
        /**
         * @brief  消息类型。 初值 = Kind_EN::Message。
         * @note   类型 Kind_EN；类/结构体成员变量。
         */
        Kind_EN    kind = Kind_EN::Message;
        /**
         * @brief  全局消息序号。 初值 = 0。
         * @note   类型 qint64；类/结构体成员变量。
         */
        qint64  seq = 0;
        /**
         * @brief  消息文本。
         * @note   类型 QString；类/结构体成员变量。
         */
        QString text;
    };

    /**
     * @brief      入队 enqueue。
     * @param[in] job            任务；类型 const Job_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void enqueue(const Job_S& job);
    /**
     * @brief      驱动待处理队列，逐条取出任务并提交执行。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void pump();
    /**
     * @brief      把处理完成的响应投递回对应的请求方。
     * @param[in] job            任务；类型 const Job_S&。
     * @param[in] result         处理结果；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void deliver(const Job_S& job, const QString& result);
    /**
     * @brief      异步请求完成回调，解析结果并分发给对应的等待方。
     * @param[in] requestId      请求编号；类型 int。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onFinished(int requestId, const QString& text);
    /**
     * @brief      异步请求失败回调，记录错误原因并通知上层。
     * @param[in] requestId      请求编号；类型 int。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onFailed(int requestId, const QString& reason);
    /**
     * @brief      执行缓存 put。
     * @param[in] key            缓存键；类型 const QString&。
     * @param[in] value          待设置的数值；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void cachePut(const QString& key, const QString& value);

    /**
     * @brief      根据任务类型与文本生成译文缓存键。
     * @param[in] kind           消息类型；类型 Kind_EN。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    static QString cacheKey(Kind_EN kind, const QString& text);
    /**
     * @brief      清洗模型输出，去除多余符号与思考过程残留。
     * @param[in] raw            输入参数；类型 const QString&。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    static QString cleanOutput(const QString& raw);

    /**
     * @brief  客户端。 初值 = nullptr。
     * @note   类型 CLlmClient*；类/结构体成员变量。
     */
    CLlmClient*             m_pClient = nullptr;
    /**
     * @brief  队列。
     * @note   类型 QVector<Job_S>；类/结构体成员变量。
     */
    QVector<Job_S>           queue_;
    /**
     * @brief  缓存。
     * @note   类型 QHash<QString, QString>；类/结构体成员变量。
     */
    QHash<QString, QString> m_strCache;
    /**
     * @brief  缓存Order。
     * @note   类型 QList<QString>；类/结构体成员变量。
     */
    QList<QString>         m_strCacheOrder;
    /**
     * @brief  InFlight。
     * @note   类型 QSet<QString>；类/结构体成员变量。
     */
    QSet<QString>          m_strInFlight;
    /**
     * @brief  current。
     * @note   类型 Job_S；类/结构体成员变量。
     */
    Job_S                    current_;
    /**
     * @brief  Current编号。 初值 = 0。
     * @note   类型 int；类/结构体成员变量。
     */
    int                    m_nCurrentId = 0;
    /**
     * @brief  Busy。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool                   m_nBusy = false;
};
