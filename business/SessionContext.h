/**
 * @file  SessionContext.h
 * @brief 当前会话的全局状态：本人身份、所在房间、在线状态。
 * @details 这些状态原本散落在 MainWindow 与 ChatPage 的成员变量里，
 *          两边各存一份，靠调用方保证同步。收敛到单例后只有一个真相源，
 *          UI 层通过信号被动刷新，不再需要层层传参。
 *
 * @note 只保存「会话级」状态。消息列表属于 ChatModel，
 *       配置属于 AppConfig，不要往这里堆。
 */

#pragma once

#include <QObject>
#include <QSet>
#include <QString>

namespace business {

/**
 * @brief 会话状态单例。
 *
 * @details 生命周期与进程相同，构造后由 `instance()` 唯一持有。
 *          所有改动都会发出对应信号，界面据此刷新。
 */
class SessionContext : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取全局唯一实例。
     * @retval 单例引用。
     */
    static SessionContext& instance();

    SessionContext(const SessionContext&) = delete;
    SessionContext& operator=(const SessionContext&) = delete;

    // ---- 本人身份 ----

    /// @brief 服务端分配的会话 id，0 表示未登录。
    qint64 userId() const { return userId_; }

    /// @brief 昵称。
    QString name() const { return name_; }

    /// @brief 年龄，0 表示未填写。
    int age() const { return age_; }

    /// @brief 个人简介。
    QString desc() const { return desc_; }

    /**
     * @brief 更新本人资料。
     * @param userId 服务端分配的会话 id。
     * @param name   昵称。
     * @param age    年龄，0 表示未填写。
     * @param desc   个人简介。
     * @details 每次登录都会把 id 记入历史集合——断线重连后 id 会变，
     *          但旧消息的 `from` 仍是旧 id，判断「是不是我发的」必须看历史集合。
     */
    void setProfile(qint64 userId, const QString& name, int age, const QString& desc);

    /// @brief 清空本人身份（退出登录时调用）。
    void clearProfile();

    /**
     * @brief 判断某条消息是否由本人发出。
     * @param userId 消息里的 `from` 字段。
     * @retval true  本人（含历次重连前使用的 id）。
     * @retval false 他人，或 userId 为 0。
     */
    bool isSelf(qint64 userId) const { return userId != 0 && selfIds_.contains(userId); }

    // ---- 房间 ----

    /// @brief 当前房间号，空串表示不在任何房间。
    QString room() const { return room_; }

    /**
     * @brief 进入房间。
     * @param room 房间号。
     */
    void setRoom(const QString& room);

    /// @brief 离开房间。
    void clearRoom();

    // ---- 连接 ----

    /// @brief 是否处于「已连接且已登录」状态。
    bool online() const { return online_; }

    /**
     * @brief 设置在线状态。
     * @param online true 表示连接与登录均正常。
     */
    void setOnline(bool online);

signals:
    /// @brief 本人资料发生变化。
    void profileChanged();
    /// @brief 所在房间发生变化（空串表示已离开）。
    void roomChanged(const QString& room);
    /// @brief 在线状态发生变化。
    void onlineChanged(bool online);

private:
    explicit SessionContext(QObject* parent = nullptr);

    qint64        userId_ = 0;      ///< 当前会话 id
    QString       name_;            ///< 昵称
    int           age_ = 0;         ///< 年龄
    QString       desc_;            ///< 个人简介
    QSet<qint64>  selfIds_;         ///< 本人历次使用的 id（重连后 id 会变）
    QString       room_;            ///< 当前房间号
    bool          online_ = false;  ///< 在线状态
};

}  // namespace business
