/*
 * 文件名:    MessageStore.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMessageStore 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMessageStore.h */

/* 本地消息缓存（SQLite）。声网 RTM 不提供历史消息，进房后靠这里补。 */
/* 按 (source, room, seq) 去重：source 区分 server / rtm，避免两套 seq 体系混在一起。 */
#pragma once

#include <QString>
#include <QVector>

#include "business/ChatTypes.h"

class CMessageStore {
public:
    CMessageStore() = default;
    ~CMessageStore();

    /* 打开（或创建）数据库。失败时 error 里是原因，功能降级为"不缓存"。 */
    bool open(const QString& path, QString* error = nullptr);

    void save(const QString& source, const QString& room, const ChatMessage_S& message);

    /* 取某房间最近 limit 条，按时间正序返回。 */
    QVector<ChatMessage_S> loadRecent(const QString& source, const QString& room, int limit = 200);

    void clearRoom(const QString& source, const QString& room);

    /* 每个房间只保留最近 keep 条，防止无限增长。 */
    void prune(const QString& source, const QString& room, int keep = 500);

    bool isOpen() const { return m_nOpened; }
    QString path() const { return m_strPath; }

private:
    QString m_strConnectionName;
    QString m_strPath;
    bool    m_nOpened = false;
};
