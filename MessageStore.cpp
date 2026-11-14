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
 * 功能描述:  析构 CMessageStore 对象，释放运行期持有的全部资源。
 * 作者:      JaTin
 * return:    无返回值（构造函数或析构函数）。
 */
CMessageStore::~CMessageStore() {
    if (m_nOpened) {
        QSqlDatabase::database(m_strConnectionName).close();
        QSqlDatabase::removeDatabase(m_strConnectionName);
    }
}

/*
 * 功能描述:  获取open；失败时返回 false。
 * 作者:      JaTin
 * param [in]   strPath          文件路径，类型 const QString&。
 * param [in]   pError           错误信息，类型 QString*。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CMessageStore::open(const QString& strPath, QString* pError) {
    m_strPath = strPath;

    const QFileInfo stInfo(strPath);
    if (!stInfo.absoluteDir().exists() && !QDir().mkpath(stInfo.absolutePath())) {
        if (pError != nullptr) *pError = QStringLiteral("无法创建数据目录：%1").arg(stInfo.absolutePath());
        return false;
    }

    m_strConnectionName = QStringLiteral("dp_history_%1").arg(reinterpret_cast<quintptr>(this));
    QSqlDatabase stDb = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_strConnectionName);
    stDb.setDatabaseName(strPath);

    if (!stDb.open()) {
        if (pError != nullptr) *pError = stDb.lastError().text();
        return false;
    }

    QSqlQuery query(stDb);
    /* 同一台机器可能同时开多个客户端（测试双人对话），共用同一个 db 文件。 */
    /* WAL + busy_timeout 让并发写入排队而不是立刻报 database is locked。 */
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA busy_timeout=3000"));

    const bool nOk = query.exec(QStringLiteral(
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
    if (!nOk) {
        if (pError != nullptr) *pError = query.lastError().text();
        return false;
    }
    query.exec(QStringLiteral(
        "CREATE INDEX IF NOT EXISTS idx_room_ts ON messages(source, room, ts)"));

    m_nOpened = true;
    return true;
}

/*
 * 功能描述:  保存save。
 * 作者:      JaTin
 * param [in]   strSource        配置来源路径，类型 const QString&。
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   stMessage        消息内容，类型 const ChatMessage_S&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageStore::save(const QString& strSource, const QString& strRoom, const ChatMessage_S& stMessage) {
    if (!m_nOpened) return;

    QSqlDatabase stDb = QSqlDatabase::database(m_strConnectionName);
    QSqlQuery query(stDb);
    query.prepare(QStringLiteral(
        "INSERT OR REPLACE INTO messages(source, room, seq, from_id, name, text, ts)"
        " VALUES(?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(strSource);
    query.addBindValue(strRoom);
    query.addBindValue(stMessage.seq);
    query.addBindValue(stMessage.from);
    query.addBindValue(stMessage.name);
    query.addBindValue(stMessage.text);
    query.addBindValue(stMessage.ts);
    query.exec();
}

/*
 * 功能描述:  加载recent。
 * 作者:      JaTin
 * param [in]   strSource        配置来源路径，类型 const QString&。
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   nLimit           输入数据，类型 int。
 * return:    返回 QVector<ChatMessage_S> 类型的处理结果，具体含义见功能描述。
 */
QVector<ChatMessage_S> CMessageStore::loadRecent(const QString& strSource, const QString& strRoom, int nLimit) {
    QVector<ChatMessage_S> aResult;
    if (!m_nOpened) return aResult;

    QSqlDatabase stDb = QSqlDatabase::database(m_strConnectionName);
    QSqlQuery query(stDb);
    query.prepare(QStringLiteral(
        "SELECT seq, from_id, name, text, ts FROM messages"
        " WHERE source = ? AND room = ? ORDER BY ts DESC LIMIT ?"));
    query.addBindValue(strSource);
    query.addBindValue(strRoom);
    query.addBindValue(nLimit);

    if (!query.exec()) {
        qWarning() << "loadRecent failed:" << query.lastError().text();
        return aResult;
    }

    while (query.next()) {
        ChatMessage_S stMessage;
        stMessage.seq = query.value(0).toLongLong();
        stMessage.from = query.value(1).toLongLong();
        stMessage.name = query.value(2).toString();
        stMessage.text = query.value(3).toString();
        stMessage.ts = query.value(4).toLongLong();
        aResult.append(stMessage);
    }

    /* 查询是倒序，翻回正序给界面 */
    std::reverse(aResult.begin(), aResult.end());
    return aResult;
}

/*
 * 功能描述:  清空房间。
 * 作者:      JaTin
 * param [in]   strSource        配置来源路径，类型 const QString&。
 * param [in]   strRoom          房间号，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageStore::clearRoom(const QString& strSource, const QString& strRoom) {
    if (!m_nOpened) return;
    QSqlDatabase stDb = QSqlDatabase::database(m_strConnectionName);
    QSqlQuery query(stDb);
    query.prepare(QStringLiteral("DELETE FROM messages WHERE source = ? AND room = ?"));
    query.addBindValue(strSource);
    query.addBindValue(strRoom);
    query.exec();
}

/*
 * 功能描述:  清理超出保留上限的本地历史记录，控制数据库体积。
 * 作者:      JaTin
 * param [in]   strSource        配置来源路径，类型 const QString&。
 * param [in]   strRoom          房间号，类型 const QString&。
 * param [in]   nKeep            输入数据，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CMessageStore::prune(const QString& strSource, const QString& strRoom, int nKeep) {
    if (!m_nOpened) return;
    QSqlDatabase stDb = QSqlDatabase::database(m_strConnectionName);
    QSqlQuery query(stDb);
    query.prepare(QStringLiteral(
        "DELETE FROM messages WHERE source = ? AND room = ? AND seq NOT IN ("
        "  SELECT seq FROM messages WHERE source = ? AND room = ? ORDER BY ts DESC LIMIT ?"
        ")"));
    query.addBindValue(strSource);
    query.addBindValue(strRoom);
    query.addBindValue(strSource);
    query.addBindValue(strRoom);
    query.addBindValue(nKeep);
    query.exec();
}
