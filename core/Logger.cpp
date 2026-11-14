/*
 * 文件名:    Logger.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  客户端日志器的实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#include "core/Logger.h"

#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <cstdio>

namespace core {
namespace {

/* 级别对应的短名称，用于对齐输出宽度。 */
/*
 * 功能描述:  获取level 名称。
 * 作者:      JaTin
 * param [in]   enLevel          日志级别，类型 LogLevel_EN。
 * return:    成功返回对象指针；未找到或失败时返回 nullptr。
 */
const char* levelName(LogLevel_EN enLevel) {
    switch (enLevel) {
        case LogLevel_EN::Debug: return "DEBUG";
        case LogLevel_EN::Info:  return "INFO ";
        case LogLevel_EN::Warn:  return "WARN ";
        case LogLevel_EN::Error: return "ERROR";
    }
    return "?????";
}

/* < 保护文件写入（日志可能来自工作线程） */
QMutex g_writeMutex;

/* namespace */
}

/*
 * 功能描述:  获取instance。
 * 作者:      JaTin
 * return:    返回 CLogger& 类型的处理结果，具体含义见功能描述。
 */
CLogger& CLogger::instance() {
    static CLogger stLogger;
    return stLogger;
}

/*
 * 功能描述:  设置文件；失败时返回 false。
 * 作者:      JaTin
 * param [in]   strPath          文件路径，类型 const QString&。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CLogger::setFile(const QString& strPath) {
    m_strFilePath = strPath;
    /* 立刻验证可写，避免等到第一条日志时才发现路径不可用。 */
    QFile stProbe(strPath);
    if (!stProbe.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_strFilePath.clear();
        return false;
    }
    stProbe.close();
    return true;
}

/*
 * 功能描述:  按日志级别格式化并输出一条日志。
 * 作者:      JaTin
 * param [in]   enLevel          日志级别，类型 LogLevel_EN。
 * param [in]   strTag           输入数据，类型 const QString&。
 * param [in]   strMessage       消息内容，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CLogger::write(LogLevel_EN enLevel, const QString& strTag, const QString& strMessage) {
    if (static_cast<int>(enLevel) < static_cast<int>(m_stLevel)) return;

    const QString strLine = QStringLiteral("%1 [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString(
                                      QStringLiteral("HH:mm:ss.zzz")))
                             .arg(QString::fromLatin1(levelName(enLevel)))
                             .arg(strTag)
                             .arg(strMessage);

    std::fprintf(stderr, "%s\n", strLine.toUtf8().constData());

    if (m_strFilePath.isEmpty()) return;

    QMutexLocker stLock(&g_writeMutex);
    QFile stFile(m_strFilePath);
    if (!stFile.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    QTextStream stStream(&stFile);
    stStream << strLine << '\n';
}

/* namespace core */
}
