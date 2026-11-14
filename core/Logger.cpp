/**
 * @file       Logger.cpp
 * @brief      客户端日志器的实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
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

/**
 * @brief  级别对应的短名称，用于对齐输出宽度。
 * @details 获取 level 名称。
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

/**
 * @brief  < 保护文件写入（日志可能来自工作线程）。
 */
QMutex g_writeMutex;

/**
 * @brief  namespace。
 */
}

/**
 * @brief      获取本类的进程内单例对象。
 * @note       本函数无输入参数。
 * @return     返回 CLogger& CLogger:: 类型的对象引用或指针，调用方按接口约定使用。
 * @author     JaTin
 */
CLogger& CLogger::instance() {
    static CLogger stLogger;
    return stLogger;
}

/**
 * @brief      设置文件；失败时返回 false。
 * @param[in] strPath        文件路径；类型 const QString&。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool CLogger::setFile(const QString& strPath) {
    m_strFilePath = strPath;
    /**
     * @brief  立刻验证可写，避免等到第一条日志时才发现路径不可用。
     */
    QFile stProbe(strPath);
    if (!stProbe.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_strFilePath.clear();
        return false;
    }
    stProbe.close();
    return true;
}

/**
 * @brief      按日志级别格式化并输出一条日志。
 * @param[in] enLevel        日志级别；类型 LogLevel_EN。
 * @param[in] strTag         输入数据；类型 const QString&。
 * @param[in] strMessage     消息内容；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief  namespace core。
 */
}
