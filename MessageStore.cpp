// MessageStore.cpp
#include "MessageStore.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <algorithm>

MessageStore::~MessageStore() {
    if (opened_) {
        QSqlDatabase::database(connectionName_).close();
        QSqlDatabase::removeDatabase(connectionName_);
    }
}

bool MessageStore::open(const QString& path, QString* error) {
    path_ = path;

    const QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        if (error != nullptr) *error = QStringLiteral("无法创建数据目录：%1").arg(info.absolutePath());
        return false;
    }

    connectionName_ = QStringLiteral("dp_history_%1").arg(reinterpret_cast<quintptr>(this));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    db.setDatabaseName(path);

    if (!db.open()) {
        if (error != nullptr) *error = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    // 同一台机器可能同时开多个客户端（测试双人对话），共用同一个 db 文件。
    // WAL + busy_timeout 让并发写入排队而不是立刻报 database is locked。
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA busy_timeout=3000"));

    const bool ok = query.exec(QStringLiteral(
        "CREATE TABLE IF NOT EXISTS messages ("
        "  source  TEXT    NOT NULL,"
        "  room    TEXT    NOT NULL,"
        "  seq     INTEGER NOT NULL,"
        "  from_id INTEGER NOT NULL,"
        "  name    TEXT    NOT NULL,"
        "  text    TEXT    NOT NULL,"
        "  ts      INTEGER NOT NULL,"
        "  PRIMARY KEY (source, room, seq)"
        ")"));
    if (!ok) {
        if (error != nullptr) *error = query.lastError().text();
        return false;
    }
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_room_ts ON messages(source, room, ts)"));

    opened_ = true;
    return true;
}

void MessageStore::save(const QString& source, const QString& room, const ChatMessage& message) {
    if (!opened_) return;

    QSqlDatabase db = QSqlDatabase::database(connectionName_);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO messages(source, room, seq, from_id, name, text, ts)"
        " VALUES(?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(source);
    query.addBindValue(room);
    query.addBindValue(message.seq);
    query.addBindValue(message.from);
    query.addBindValue(message.name);
    query.addBindValue(message.text);
    query.addBindValue(message.ts);
    query.exec();
}

QVector<ChatMessage> MessageStore::loadRecent(const QString& source, const QString& room, int limit) {
    QVector<ChatMessage> result;
    if (!opened_) return result;

    QSqlDatabase db = QSqlDatabase::database(connectionName_);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT seq, from_id, name, text, ts FROM messages"
        " WHERE source = ? AND room = ? ORDER BY ts DESC LIMIT ?"));
    query.addBindValue(source);
    query.addBindValue(room);
    query.addBindValue(limit);

    if (!query.exec()) {
        qWarning() << "loadRecent failed:" << query.lastError().text();
        return result;
    }

    while (query.next()) {
        ChatMessage message;
        message.seq = query.value(0).toLongLong();
        message.from = query.value(1).toLongLong();
        message.name = query.value(2).toString();
        message.text = query.value(3).toString();
        message.ts = query.value(4).toLongLong();
        result.append(message);
    }

    // 查询是倒序，翻回正序给界面
    std::reverse(result.begin(), result.end());
    return result;
}

void MessageStore::clearRoom(const QString& source, const QString& room) {
    if (!opened_) return;
    QSqlDatabase db = QSqlDatabase::database(connectionName_);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM messages WHERE source = ? AND room = ?"));
    query.addBindValue(source);
    query.addBindValue(room);
    query.exec();
}

void MessageStore::prune(const QString& source, const QString& room, int keep) {
    if (!opened_) return;
    QSqlDatabase db = QSqlDatabase::database(connectionName_);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "DELETE FROM messages WHERE source = ? AND room = ? AND seq NOT IN ("
        "  SELECT seq FROM messages WHERE source = ? AND room = ? ORDER BY ts DESC LIMIT ?"
        ")"));
    query.addBindValue(source);
    query.addBindValue(room);
    query.addBindValue(source);
    query.addBindValue(room);
    query.addBindValue(keep);
    query.exec();
}
