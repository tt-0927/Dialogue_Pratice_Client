/**
 * @file       SessionContext.h
 * @brief      当前会话的全局状态：本人身份、所在房间、在线状态。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

#pragma once

#include <QObject>
#include <QSet>
#include <QString>

namespace business {

/**
 * @brief   会话状态单例。
 * @details 生命周期与进程相同，构造后由 `instance()` 唯一持有。 所有改动都会发出对应信号，界面据此刷新。
 * @author  JaTin
 */
class CSessionContext : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      获取全局唯一实例。
     * @note       本函数无输入参数。
     * @return     单例引用。
     * @author     JaTin
     */
    static CSessionContext& instance();

    CSessionContext(const CSessionContext&) = delete;
    CSessionContext& operator=(const CSessionContext&) = delete;

    /**
     * @brief  ---- 本人身份 ----。
     */

    /**
     * @brief  服务端分配的会话 id，0 表示未登录。
     */
    qint64 userId() const { return m_uUserId; }

    /**
     * @brief  昵称。
     */
    QString name() const { return m_strName; }

    /**
     * @brief  年龄，0 表示未填写。
     */
    int age() const { return m_nAge; }

    /**
     * @brief  个人简介。
     */
    QString desc() const { return m_strDesc; }

    /**
     * @brief      更新本人资料。
     * @details    每次登录都会把 id 记入历史集合——断线重连后 id 会变， 但旧消息的 `from` 仍是旧 id，判断「是不是我发的」必须看历史集合。
     * @param[in] userId         服务端分配的会话 id；类型 qint64。
     * @param[in] name           昵称；类型 const QString&。
     * @param[in] age            年龄，0 表示未填写；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setProfile(qint64 userId, const QString& name, int age, const QString& desc);

    /**
     * @brief      清空本人身份（退出登录时调用）。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearProfile();

    /**
     * @brief 判断某条消息是否由本人发出。
     * @param userId 消息里的 `from` 字段。
     * @retval true  本人（含历次重连前使用的 id）。
     * @retval false 他人，或 userId 为 0。
     */
    bool isSelf(qint64 userId) const { return userId != 0 && m_uSelfIds.contains(userId); }

    /**
     * @brief  ---- 房间 ----。
     */

    /**
     * @brief  当前房间号，空串表示不在任何房间。
     */
    QString room() const { return m_strRoom; }

    /**
     * @brief      进入房间。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setRoom(const QString& room);

    /**
     * @brief      离开房间。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void clearRoom();

    /**
     * @brief  ---- 连接 ----。
     */

    /**
     * @brief  是否处于「已连接且已登录」状态。
     */
    bool online() const { return m_nOnline; }

    /**
     * @brief      设置在线状态。
     * @param[in] online         true 表示连接与登录均正常；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setOnline(bool online);

signals:
    /**
     * @brief      本人资料发生变化。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void profileChanged();
    /**
     * @brief      所在房间发生变化（空串表示已离开）。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void roomChanged(const QString& room);
    /**
     * @brief      在线状态发生变化。
     * @param[in] online         输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onlineChanged(bool online);

private:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CSessionContext(QObject* parent = nullptr);

    /**
     * @brief  当前会话 id
     * @note   类型 qint64；类/结构体成员变量。
     */
    qint64        m_uUserId = 0;
    /**
     * @brief  昵称
     * @note   类型 QString；类/结构体成员变量。
     */
    QString       m_strName;
    /**
     * @brief  年龄
     * @note   类型 int；类/结构体成员变量。
     */
    int           m_nAge = 0;
    /**
     * @brief  个人简介
     * @note   类型 QString；类/结构体成员变量。
     */
    QString       m_strDesc;
    /**
     * @brief  本人历次使用的 id（重连后 id 会变）
     * @note   类型 QSet<qint64>；类/结构体成员变量。
     */
    QSet<qint64>  m_uSelfIds;
    /**
     * @brief  当前房间号
     * @note   类型 QString；类/结构体成员变量。
     */
    QString       m_strRoom;
    /**
     * @brief  在线状态
     * @note   类型 bool；类/结构体成员变量。
     */
    bool          m_nOnline = false;
};

/**
 * @brief  namespace business。
 */
}
