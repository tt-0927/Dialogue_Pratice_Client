/*
 * 文件名:    MessageStore.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMessageStore 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMessageStore.cpp */
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

/*
 * 功能描述:  执行 CMessageStore 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
CMessageStore::~CMessageStore() {
    if (m_nOpened) {
        QSqlDatabase::database(m_strConnectionName).close();
        QSqlDatabase::removeDatabase(m_strConnectionName);
    }
}

/*
 * 功能描述:  执行 open 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  path  参数。
 * param [in]  error  参数。
 * return:    返回值，含义见功能描述。
 */
bool CMessageStore::open(const QString& path, QString* error) {
    m_strPath = path;

    const QFileInfo info(path);
    if (!info.absoluteDir().exists() && !QDir().mkpath(info.absolutePath())) {
        if (error != nullptr) *error = QStringLiteral("无法创建数据目录：%1").arg(info.absolutePath());
        return false;
    }

    m_strConnectionName = QStringLiteral("dp_history_%1").arg(reinterpret_cast<quintptr>(this));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_strConnectionName);
    db.setDatabaseName(path);

    if (!db.open()) {
        if (error != nullptr) *error = db.lastError().text();
        return false;
    }

    QSqlQuery query(db);
    /* 同一台机器可能同时开多个客户端（测试双人对话），共用同一个 db 文件。 */
    /* WAL + busy_timeout 让并发写入排队而不是立刻报 database is locked。 */
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

    m_nOpened = true;
    return true;
}

/*
 * 功能描述:  执行 save 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  source  参数。
 * param [in]  room  参数。
 * param [in]  message  参数。
 * return:    void
 */
void CMessageStore::save(const QString& source, const QString& room, const ChatMessage_S& message) {
    if (!m_nOpened) return;

    QSqlDatabase db = QSqlDatabase::database(m_strConnectionName);
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

/*
 * 功能描述:  执行 loadRecent 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  source  参数。
 * param [in]  room  参数。
 * param [in]  limit  参数。
 * return:    返回值，含义见功能描述。
 */
QVector<ChatMessage_S> CMessageStore::loadRecent(const QString& source, const QString& room, int limit) {
    QVector<ChatMessage_S> result;
    if (!m_nOpened) return result;

    QSqlDatabase db = QSqlDatabase::database(m_strConnectionName);
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
        ChatMessage_S message;
        message.seq = query.value(0).toLongLong();
        message.from = query.value(1).toLongLong();
        message.name = query.value(2).toString();
        message.text = query.value(3).toString();
        message.ts = query.value(4).toLongLong();
        result.append(message);
    }

    /* 查询是倒序，翻回正序给界面 */
    std::reverse(result.begin(), result.end());
    return result;
}

/*
 * 功能描述:  执行 clearRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  source  参数。
 * param [in]  room  参数。
 * return:    void
 */
void CMessageStore::clearRoom(const QString& source, const QString& room) {
    if (!m_nOpened) return;
    QSqlDatabase db = QSqlDatabase::database(m_strConnectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral("DELETE FROM messages WHERE source = ? AND room = ?"));
    query.addBindValue(source);
    query.addBindValue(room);
    query.exec();
}

/*
 * 功能描述:  执行 prune 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  source  参数。
 * param [in]  room  参数。
 * param [in]  keep  参数。
 * return:    void
 */
void CMessageStore::prune(const QString& source, const QString& room, int keep) {
    if (!m_nOpened) return;
    QSqlDatabase db = QSqlDatabase::database(m_strConnectionName);
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
