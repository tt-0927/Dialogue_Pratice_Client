/**
 * @file       LlmClient.h
 * @brief      CLlmClient 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CLlmClient.h。
 */

/**
 * @brief  OpenAI 兼容的 chat/completions 客户端。 既能连本地 llama-server（http://127.0.0.1:8080），也能连云端（DeepSeek/通义等）， 差别只在 baseUrl / apiKey / model 三项配置。
 */
#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * @brief   C大模型客户端。
 * @author  JaTin
 */
class CLlmClient : public QObject {
    Q_OBJECT

public:
    struct Config_S {
        /**
         * @brief  模型服务地址。 初值 = QStringLiteral( )。
         * @note   类型 QString；类/结构体成员变量。
         */
        QString baseUrl  = QStringLiteral("http://127.0.0.1:8080");
        /**
         * @brief  云端才需要
         * @note   类型 QString；类/结构体成员变量。
         */
        QString apiKey;
        /**
         * @brief  模型名称。 初值 = QStringLiteral( )。
         * @note   类型 QString；类/结构体成员变量。
         */
        QString model    = QStringLiteral("deepseek-chat");
        /**
         * @brief  采样温度。 初值 = 0.2。
         * @note   类型 double；类/结构体成员变量。
         */
        double  temperature = 0.2;
        /**
         * @brief  low / medium / high；留空则不传
         * @note   类型 QString；类/结构体成员变量。
         */
        QString reasoningEffort;
        /**
         * @brief  超时时间，单位毫秒。 初值 = 40000。
         * @note   类型 int；类/结构体成员变量。
         */
        int     timeoutMs = 40000;
        /**
         * @brief  是否启用。 初值 = true。
         * @note   类型 bool；类/结构体成员变量。
         */
        bool    enabled   = true;
    };

    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CLlmClient(QObject* parent = nullptr);

    void setConfig(const Config_S& config) { m_stConfig = config; }
    const Config_S& config() const { return m_stConfig; }
    bool isEnabled() const { return m_stConfig.enabled && !m_stConfig.baseUrl.isEmpty(); }

    /**
     * @brief      发一条对话请求，返回 requestId；返回 0 表示未发出（未启用）。
     * @param[in] system         系统提示词；类型 const QString&。
     * @param[in] user           用户输入内容；类型 const QString&。
     * @param[in] maxTokens      最大生成 token 数；类型 int。
     * @return     返回处理结果码；0 表示成功，负值或 -1 表示失败。
     * @author     JaTin
     */
    int chat(const QString& system, const QString& user, int maxTokens = 256);

signals:
    /**
     * @brief      执行 finished相关的数据并返回处理结果。
     * @param[in] requestId      请求编号；类型 int。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void finished(int requestId, const QString& text);
    /**
     * @brief      执行 failed相关的数据并返回处理结果。
     * @param[in] requestId      请求编号；类型 int。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(int requestId, const QString& reason);

private:
    /**
     * @brief      处理回复。
     * @param[in] reply          响应对象；类型 QNetworkReply*。
     * @param[in] requestId      请求编号；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void handleReply(QNetworkReply* reply, int requestId);

    /**
     * @brief  配置项。
     * @note   类型 Config_S；类/结构体成员变量。
     */
    Config_S                 m_stConfig;
    /**
     * @brief  Nam。 初值 = nullptr。
     * @note   类型 QNetworkAccessManager*；类/结构体成员变量。
     */
    QNetworkAccessManager* m_pNam = nullptr;
    /**
     * @brief  Next编号。 初值 = 1。
     * @note   类型 int；类/结构体成员变量。
     */
    int                    m_nNextId = 1;
};
