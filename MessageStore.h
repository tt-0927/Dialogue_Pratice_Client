// MessageStore.h
//
// 本地消息缓存（SQLite）。声网 RTM 不提供历史消息，进房后靠这里补。
// 按 (source, room, seq) 去重：source 区分 server / rtm，避免两套 seq 体系混在一起。
#pragma once

#include <QString>
#include <QVector>

#include "ChatTypes.h"

class MessageStore {
public:
    MessageStore() = default;
    ~MessageStore();

    /// 打开（或创建）数据库。失败时 error 里是原因，功能降级为"不缓存"。
    bool open(const QString& path, QString* error = nullptr);

    void save(const QString& source, const QString& room, const ChatMessage& message);

    /// 取某房间最近 limit 条，按时间正序返回。
    QVector<ChatMessage> loadRecent(const QString& source, const QString& room, int limit = 200);

    void clearRoom(const QString& source, const QString& room);

    /// 每个房间只保留最近 keep 条，防止无限增长。
    void prune(const QString& source, const QString& room, int keep = 500);

    bool isOpen() const { return opened_; }
    QString path() const { return path_; }

private:
    QString connectionName_;
    QString path_;
    bool    opened_ = false;
};
