/**
 * @file  Logger.h
 * @brief 客户端统一日志入口。
 * @details 对 Qt 日志系统做一层薄封装，解决两个问题：
 *          1. 统一格式（时间 + 级别 + 模块标签），便于从终端输出里定位问题；
 *          2. 支持可选的落盘输出，方便让用户回传「出问题时那一段」。
 *
 *          不替代 `qDebug()`——底层仍然走 Qt 的消息处理器，
 *          所以 `QT_LOGGING_RULES` 之类的既有手段依然有效。
 */

#pragma once

#include <QString>

namespace core {

/**
 * @brief 日志级别，数值越大越严重。
 */
enum class LogLevel : int {
    Debug = 0,   ///< 细节（逐条消息、重连尝试）
    Info  = 1,   ///< 正常流程（连接、登录、进房）
    Warn  = 2,   ///< 可恢复异常（配置缺失、缓存不可用）
    Error = 3,   ///< 需要用户关注的失败
};

/**
 * @brief 进程级日志器（单例）。
 *
 * @details 用函数内静态对象实现，C++11 起保证线程安全的一次性初始化。
 *          默认只输出到 stderr；调用 setFile() 后同时追加到文件。
 */
class Logger {
public:
    /**
     * @brief 获取全局唯一实例。
     * @retval 单例引用。
     */
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /**
     * @brief 设置最低输出级别。
     * @param level 低于该级别的日志会被丢弃。
     */
    void setLevel(LogLevel level) { level_ = level; }

    /// @brief 当前最低输出级别。
    LogLevel level() const { return level_; }

    /**
     * @brief 把日志同时写入文件。
     * @param path 文件路径，父目录需已存在。
     * @retval true  文件已打开，后续日志会追加写入。
     * @retval false 打开失败，继续只输出到 stderr。
     */
    bool setFile(const QString& path);

    /**
     * @brief 写一条日志。
     * @param level   级别。
     * @param tag     模块标签，例如 `net` / `media` / `ui`。
     * @param message 正文。
     */
    void write(LogLevel level, const QString& tag, const QString& message);

private:
    Logger() = default;

    LogLevel level_ = LogLevel::Info;   ///< 最低输出级别
    QString  filePath_;                 ///< 落盘路径，空表示不落盘
};

}  // namespace core

/// @brief 写 Debug 级别日志。
#define DP_LOG_DEBUG(tag, msg) \
    ::core::Logger::instance().write(::core::LogLevel::Debug, tag, msg)
/// @brief 写 Info 级别日志。
#define DP_LOG_INFO(tag, msg) \
    ::core::Logger::instance().write(::core::LogLevel::Info, tag, msg)
/// @brief 写 Warn 级别日志。
#define DP_LOG_WARN(tag, msg) \
    ::core::Logger::instance().write(::core::LogLevel::Warn, tag, msg)
/// @brief 写 Error 级别日志。
#define DP_LOG_ERROR(tag, msg) \
    ::core::Logger::instance().write(::core::LogLevel::Error, tag, msg)
