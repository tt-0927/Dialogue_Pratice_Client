/**
 * @file       MessageStore.h
 * @brief      CMessageStore 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CMessageStore.h。
 */

/**
 * @brief  本地消息缓存（SQLite）。声网 RTM 不提供历史消息，进房后靠这里补。 按 (source, room, seq) 去重：source 区分 server / rtm，避免两套 seq 体系混在一起。
 */
#pragma once

#include <QString>
#include <QVector>

#include "ChatTypes.h"

/**
 * @brief   C消息内容存储。
 * @author  JaTin
 */
class CMessageStore {
public:
    CMessageStore() = default;
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @note       本函数无输入参数。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    ~CMessageStore();

    /**
     * @brief      打开（或创建）数据库。失败时 error 里是原因，功能降级为"不缓存"。
     * @param[in] path           文件路径；类型 const QString&。
     * @param[in] error          错误信息；类型 QString*。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool open(const QString& path, QString* error = nullptr);

    /**
     * @brief      保存 save。
     * @param[in] source         配置来源路径；类型 const QString&。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] message        消息内容；类型 const ChatMessage_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void save(const QString& source, const QString& room, const ChatMessage_S& message);

    /**
     * @brief      取某房间最近 limit 条，按时间正序返回。
     * @param[in] source         配置来源路径；类型 const QString&。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] limit          输入参数；类型 int。
     * @return     返回 QVector<ChatMessage_S> 类型的处理结果。
     * @author     JaTin
     */
    QVector<ChatMessage_S> loadRecent(const QString& source, const QString& room, int limit = 200);

    /**
     * @brief      清空房间。
     * @param[in] source         配置来源路径；类型 const QString&。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearRoom(const QString& source, const QString& room);

    /**
     * @brief      每个房间只保留最近 keep 条，防止无限增长。
     * @param[in] source         配置来源路径；类型 const QString&。
     * @param[in] room           房间号；类型 const QString&。
     * @param[in] keep           输入参数；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void prune(const QString& source, const QString& room, int keep = 500);

    bool isOpen() const { return m_nOpened; }
    QString path() const { return m_strPath; }

private:
    /**
     * @brief  连接名称。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strConnectionName;
    /**
     * @brief  文件路径。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPath;
    /**
     * @brief  Opened。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    m_nOpened = false;
};
