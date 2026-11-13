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
 * 功能描述:  执行 levelName 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  level  参数。
 * return:    返回值，含义见功能描述。
 */
const char* levelName(LogLevel_EN level) {
    switch (level) {
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
 * 功能描述:  执行 instance 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
CLogger& CLogger::instance() {
    static CLogger logger;
    return logger;
}

/*
 * 功能描述:  执行 setFile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  path  参数。
 * return:    返回值，含义见功能描述。
 */
bool CLogger::setFile(const QString& path) {
    m_strFilePath = path;
    /* 立刻验证可写，避免等到第一条日志时才发现路径不可用。 */
    QFile probe(path);
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_strFilePath.clear();
        return false;
    }
    probe.close();
    return true;
}

/*
 * 功能描述:  执行 write 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  level  参数。
 * param [in]  tag  参数。
 * param [in]  message  参数。
 * return:    void
 */
void CLogger::write(LogLevel_EN level, const QString& tag, const QString& message) {
    if (static_cast<int>(level) < static_cast<int>(m_stLevel)) return;

    const QString line = QStringLiteral("%1 [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString(
                                      QStringLiteral("HH:mm:ss.zzz")))
                             .arg(QString::fromLatin1(levelName(level)))
                             .arg(tag)
                             .arg(message);

    std::fprintf(stderr, "%s\n", line.toUtf8().constData());

    if (m_strFilePath.isEmpty()) return;

    QMutexLocker lock(&g_writeMutex);
    QFile file(m_strFilePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    QTextStream stream(&file);
    stream << line << '\n';
}

/* namespace core */
}
