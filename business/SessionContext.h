/*
 * 文件名:    SessionContext.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  当前会话的全局状态：本人身份、所在房间、在线状态。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
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
class CSessionContext : public QObject {
    Q_OBJECT

public:
    /**
     * @brief 获取全局唯一实例。
     * @retval 单例引用。
     */
    static CSessionContext& instance();

    CSessionContext(const CSessionContext&) = delete;
    CSessionContext& operator=(const CSessionContext&) = delete;

    /* ---- 本人身份 ---- */

    /* 服务端分配的会话 id，0 表示未登录。 */
    qint64 userId() const { return m_uUserId; }

    /* 昵称。 */
    QString name() const { return m_strName; }

    /* 年龄，0 表示未填写。 */
    int age() const { return m_nAge; }

    /* 个人简介。 */
    QString desc() const { return m_strDesc; }

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

    /* 清空本人身份（退出登录时调用）。 */
    void clearProfile();

    /**
     * @brief 判断某条消息是否由本人发出。
     * @param userId 消息里的 `from` 字段。
     * @retval true  本人（含历次重连前使用的 id）。
     * @retval false 他人，或 userId 为 0。
     */
    bool isSelf(qint64 userId) const { return userId != 0 && m_uSelfIds.contains(userId); }

    /* ---- 房间 ---- */

    /* 当前房间号，空串表示不在任何房间。 */
    QString room() const { return m_strRoom; }

    /**
     * @brief 进入房间。
     * @param room 房间号。
     */
    void setRoom(const QString& room);

    /* 离开房间。 */
    void clearRoom();

    /* ---- 连接 ---- */

    /* 是否处于「已连接且已登录」状态。 */
    bool online() const { return m_nOnline; }

    /**
     * @brief 设置在线状态。
     * @param online true 表示连接与登录均正常。
     */
    void setOnline(bool online);

signals:
    /* 本人资料发生变化。 */
    void profileChanged();
    /* 所在房间发生变化（空串表示已离开）。 */
    void roomChanged(const QString& room);
    /* 在线状态发生变化。 */
    void onlineChanged(bool online);

private:
    explicit CSessionContext(QObject* parent = nullptr);

    /* < 当前会话 id */
    qint64        m_uUserId = 0;
    /* < 昵称 */
    QString       m_strName;
    /* < 年龄 */
    int           m_nAge = 0;
    /* < 个人简介 */
    QString       m_strDesc;
    /* < 本人历次使用的 id（重连后 id 会变） */
    QSet<qint64>  m_uSelfIds;
    /* < 当前房间号 */
    QString       m_strRoom;
    /* < 在线状态 */
    bool          m_nOnline = false;
};

/* namespace business */
}
