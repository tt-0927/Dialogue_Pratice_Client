/*
 * 文件名:    Logger.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  客户端统一日志入口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#pragma once

#include <QString>

namespace core {

/**
 * @brief 日志级别，数值越大越严重。
 */
enum class LogLevel_EN : int {
    /* < 细节（逐条消息、重连尝试） */
    Debug = 0,
    /* < 正常流程（连接、登录、进房） */
    Info  = 1,
    /* < 可恢复异常（配置缺失、缓存不可用） */
    Warn  = 2,
    /* < 需要用户关注的失败 */
    Error = 3,
};

/**
 * @brief 进程级日志器（单例）。
 *
 * @details 用函数内静态对象实现，C++11 起保证线程安全的一次性初始化。
 *          默认只输出到 stderr；调用 setFile() 后同时追加到文件。
 */
class CLogger {
public:
    /**
     * @brief 获取全局唯一实例。
     * @retval 单例引用。
     */
    static CLogger& instance();

    CLogger(const CLogger&) = delete;
    CLogger& operator=(const CLogger&) = delete;

    /**
     * @brief 设置最低输出级别。
     * @param level 低于该级别的日志会被丢弃。
     */
    void setLevel(LogLevel_EN level) { m_stLevel = level; }

    /* 当前最低输出级别。 */
    LogLevel_EN level() const { return m_stLevel; }

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
    void write(LogLevel_EN level, const QString& tag, const QString& message);

private:
    CLogger() = default;

    /* < 最低输出级别 */
    LogLevel_EN m_stLevel = LogLevel_EN::Info;
    /* < 落盘路径，空表示不落盘 */
    QString  m_strFilePath;
};

/* namespace core */
}

/* 写 Debug 级别日志。 */
#define DP_LOG_DEBUG(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Debug, tag, msg)
/* 写 Info 级别日志。 */
#define DP_LOG_INFO(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Info, tag, msg)
/* 写 Warn 级别日志。 */
#define DP_LOG_WARN(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Warn, tag, msg)
/* 写 Error 级别日志。 */
#define DP_LOG_ERROR(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Error, tag, msg)
