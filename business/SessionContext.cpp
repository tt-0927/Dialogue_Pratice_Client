/*
 * 文件名:    SessionContext.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  会话状态单例的实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

#include "business/SessionContext.h"

namespace business {

/*
 * 功能描述:  执行 instance 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
CSessionContext& CSessionContext::instance() {
    static CSessionContext context;
    return context;
}

CSessionContext::CSessionContext(QObject* parent) : QObject(parent) {}

void CSessionContext::setProfile(qint64 userId, const QString& name, int age,
                                const QString& desc) {
    if (userId != 0) m_uSelfIds.insert(userId);

    const bool changed = (userId != m_uUserId) || (name != m_strName) || (age != m_nAge)
                         || (desc != m_strDesc);
    m_uUserId = userId;
    m_strName = name;
    m_nAge = age;
    m_strDesc = desc;

    if (changed) emit profileChanged();
}

/*
 * 功能描述:  执行 clearProfile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CSessionContext::clearProfile() {
    const bool hadProfile = (m_uUserId != 0) || !m_strName.isEmpty();

    m_uUserId = 0;
    m_strName.clear();
    m_nAge = 0;
    m_strDesc.clear();
    m_uSelfIds.clear();

    if (hadProfile) emit profileChanged();
}

/*
 * 功能描述:  执行 setRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  room  参数。
 * return:    void
 */
void CSessionContext::setRoom(const QString& room) {
    if (room == m_strRoom) return;
    m_strRoom = room;
    emit roomChanged(m_strRoom);
}

/*
 * 功能描述:  执行 clearRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CSessionContext::clearRoom() {
    setRoom(QString());
}

/*
 * 功能描述:  执行 setOnline 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  online  参数。
 * return:    void
 */
void CSessionContext::setOnline(bool online) {
    if (online == m_nOnline) return;
    m_nOnline = online;
    emit onlineChanged(m_nOnline);
}

/* namespace business */
}
