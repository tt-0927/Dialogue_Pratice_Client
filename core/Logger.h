/**
 * @file       Logger.h
 * @brief      客户端统一日志入口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

#pragma once

#include <QString>

namespace core {

/**
 * @brief   日志级别，数值越大越严重。
 * @author  JaTin
 */
enum class LogLevel_EN : int {
    /**
     * @brief  细节（逐条消息、重连尝试）
     */
    Debug = 0,
    /**
     * @brief  正常流程（连接、登录、进房）
     */
    Info  = 1,
    /**
     * @brief  可恢复异常（配置缺失、缓存不可用）
     */
    Warn  = 2,
    /**
     * @brief  需要用户关注的失败
     */
    Error = 3,
};

/**
 * @brief   进程级日志器（单例）。
 * @details 用函数内静态对象实现，C++11 起保证线程安全的一次性初始化。 默认只输出到 stderr；调用 setFile() 后同时追加到文件。
 * @author  JaTin
 */
class CLogger {
public:
    /**
     * @brief      获取全局唯一实例。
     * @note       本函数无输入参数。
     * @return     单例引用。
     * @author     JaTin
     */
    static CLogger& instance();

    CLogger(const CLogger&) = delete;
    CLogger& operator=(const CLogger&) = delete;

    /**
     * @brief 设置最低输出级别。
     * @param level 低于该级别的日志会被丢弃。
     */
    void setLevel(LogLevel_EN level) { m_stLevel = level; }

    /**
     * @brief  当前最低输出级别。
     */
    LogLevel_EN level() const { return m_stLevel; }

    /**
     * @brief      把日志同时写入文件。
     * @param[in] path           文件路径，父目录需已存在；类型 const QString&。
     * @return     true  文件已打开，后续日志会追加写入；false 打开失败，继续只输出到 stderr。
     * @author     JaTin
     */
    bool setFile(const QString& path);

    /**
     * @brief      写一条日志。
     * @param[in] level          级别；类型 LogLevel_EN。
     * @param[in] tag            模块标签，例如 `net` / `media` / `ui`；类型 const QString&。
     * @param[in] message        正文；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void write(LogLevel_EN level, const QString& tag, const QString& message);

private:
    CLogger() = default;

    /**
     * @brief  最低输出级别
     * @note   类型 LogLevel_EN；类/结构体成员变量。
     */
    LogLevel_EN m_stLevel = LogLevel_EN::Info;
    /**
     * @brief  落盘路径，空表示不落盘
     * @note   类型 QString；类/结构体成员变量。
     */
    QString  m_strFilePath;
};

/**
 * @brief  namespace core。
 */
}

/**
 * @brief  写 Debug 级别日志。
 */
#define DP_LOG_DEBUG(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Debug, tag, msg)
/**
 * @brief  写 Info 级别日志。
 */
#define DP_LOG_INFO(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Info, tag, msg)
/**
 * @brief  写 Warn 级别日志。
 */
#define DP_LOG_WARN(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Warn, tag, msg)
/**
 * @brief  写 Error 级别日志。
 */
#define DP_LOG_ERROR(tag, msg) \
    ::core::CLogger::instance().write(::core::LogLevel_EN::Error, tag, msg)
