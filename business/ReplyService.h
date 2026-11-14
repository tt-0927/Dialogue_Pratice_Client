/**
 * @file       ReplyService.h
 * @brief      CReplyService 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CReplyService.h。
 */

/**
 * @brief  智能提示回复：根据最近几条对话，让 LLM 给出 3 条适合学习者说的英文短句， 并同时给出对应中文译文（同一次请求产出，避免逐条再翻一遍的额外延迟）。 与 CTranslationService 共用同一个 CLlmClient，靠 requestId 区分各自的响应。
 */
#pragma once

#include <QObject>
#include <QStringList>
#include <QVector>

#include "business/ChatTypes.h"

class CLlmClient;

/**
 * @brief   应答业务服务。
 * @author  JaTin
 */
class CReplyService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CReplyService(QObject* parent = nullptr);

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
     * @brief      传入最近几条对话（按时间正序），返回 3 条建议（含中文译文）。
     * @param[in] context        上下文；类型 const QVector<ChatMessage_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void request(const QVector<ChatMessage_S>& context);

    /**
     * @brief      解析模型输出：优先按对象数组 {en, zh}，其次字符串数组，最后按行拆分。
     * @param[in] raw            输入参数；类型 const QString&。
     * @return     返回 QVector<Suggestion_S> 类型的处理结果。
     * @author     JaTin
     */
    static QVector<Suggestion_S> parseSuggestions(const QString& raw);

signals:
    /**
     * @brief      执行智能建议列表 ready。
     * @param[in] items          条目列表；类型 const QVector<Suggestion_S>&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void suggestionsReady(const QVector<Suggestion_S>& items);
    /**
     * @brief      执行 failed相关的数据并返回处理结果。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(const QString& reason);

private:
    /**
     * @brief  客户端。 初值 = nullptr。
     * @note   类型 CLlmClient*；类/结构体成员变量。
     */
    CLlmClient* m_pClient = nullptr;
    /**
     * @brief  Current编号。 初值 = 0。
     * @note   类型 int；类/结构体成员变量。
     */
    int        m_nCurrentId = 0;
    /**
     * @brief  Busy。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool       m_nBusy = false;
};
