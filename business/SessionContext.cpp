/**
 * @file       SessionContext.cpp
 * @brief      会话状态单例的实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释；
 *             2026-11-15 JaTin 增加头像资料的保存。
 */

#include "business/SessionContext.h"

namespace business {

/**
 * @brief      获取本类的进程内单例对象。
 * @note       本函数无输入参数。
 * @return     返回 CSessionContext& CSessionContext:: 类型的对象引用或指针，调用方按接口约定使用。
 * @author     JaTin
 */
CSessionContext& CSessionContext::instance() {
    static CSessionContext stContext;
    return stContext;
}

/**
 * @brief      构造 CSessionContext 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject* pParent) : QObject(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CSessionContext::CSessionContext(QObject* pParent) : QObject(pParent) {}

/**
 * @brief      设置个人资料。
 * @param[in] lUserId        用户编号；类型 qint64。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @param[in] byAvatar       头像图片二进制，空表示无自定义头像；类型 const QByteArray&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSessionContext::setProfile(qint64 lUserId, const QString& strName, int nAge,
                                const QString& strDesc, const QByteArray& byAvatar) {
    if (lUserId != 0) m_uSelfIds.insert(lUserId);

    const bool nChanged = (lUserId != m_uUserId) || (strName != m_strName) || (nAge != m_nAge)
                         || (strDesc != m_strDesc) || (byAvatar != m_byAvatar);
    m_uUserId = lUserId;
    m_strName = strName;
    m_nAge = nAge;
    m_strDesc = strDesc;
    m_byAvatar = byAvatar;

    if (nChanged) emit profileChanged();
}

/**
 * @brief      清空个人资料。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSessionContext::clearProfile() {
    const bool nHadProfile = (m_uUserId != 0) || !m_strName.isEmpty();

    m_uUserId = 0;
    m_strName.clear();
    m_nAge = 0;
    m_strDesc.clear();
    m_byAvatar.clear();
    m_uSelfIds.clear();

    if (nHadProfile) emit profileChanged();
}

/**
 * @brief      设置房间。
 * @param[in] strRoom        房间号；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSessionContext::setRoom(const QString& strRoom) {
    if (strRoom == m_strRoom) return;
    m_strRoom = strRoom;
    emit roomChanged(m_strRoom);
}

/**
 * @brief      清空房间。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSessionContext::clearRoom() {
    setRoom(QString());
}

/**
 * @brief      设置 online相关的数据并返回处理结果。
 * @param[in] nOnline        输入数据；类型 bool。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CSessionContext::setOnline(bool nOnline) {
    if (nOnline == m_nOnline) return;
    m_nOnline = nOnline;
    emit onlineChanged(m_nOnline);
}

/**
 * @brief  namespace business。
 */
}
