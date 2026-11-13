/**
 * @file  Logger.cpp
 * @brief 客户端日志器的实现。
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

/// @brief 级别对应的短名称，用于对齐输出宽度。
const char* levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info:  return "INFO ";
        case LogLevel::Warn:  return "WARN ";
        case LogLevel::Error: return "ERROR";
    }
    return "?????";
}

QMutex g_writeMutex;   ///< 保护文件写入（日志可能来自工作线程）

}  // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

bool Logger::setFile(const QString& path) {
    filePath_ = path;
    // 立刻验证可写，避免等到第一条日志时才发现路径不可用。
    QFile probe(path);
    if (!probe.open(QIODevice::WriteOnly | QIODevice::Append)) {
        filePath_.clear();
        return false;
    }
    probe.close();
    return true;
}

void Logger::write(LogLevel level, const QString& tag, const QString& message) {
    if (static_cast<int>(level) < static_cast<int>(level_)) return;

    const QString line = QStringLiteral("%1 [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime().toString(
                                      QStringLiteral("HH:mm:ss.zzz")))
                             .arg(QString::fromLatin1(levelName(level)))
                             .arg(tag)
                             .arg(message);

    std::fprintf(stderr, "%s\n", line.toUtf8().constData());

    if (filePath_.isEmpty()) return;

    QMutexLocker lock(&g_writeMutex);
    QFile file(filePath_);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) return;
    QTextStream stream(&file);
    stream << line << '\n';
}

}  // namespace core
