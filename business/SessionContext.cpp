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
 * 功能描述:  获取instance。
 * 作者:      JaTin
 * return:    返回 CSessionContext& 类型的处理结果，具体含义见功能描述。
 */
CSessionContext& CSessionContext::instance() {
    static CSessionContext stContext;
    return stContext;
}

/*
 * 功能描述:  构造 CSessionContext 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject* parent) : QObject(。
 * return:    无返回值（构造函数或析构函数）。
 */
CSessionContext::CSessionContext(QObject* pParent) : QObject(pParent) {}

/*
 * 功能描述:  设置个人资料。
 * 作者:      JaTin
 * param [in]   lUserId          用户编号，类型 qint64。
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSessionContext::setProfile(qint64 lUserId, const QString& strName, int nAge,
                                const QString& strDesc) {
    if (lUserId != 0) m_uSelfIds.insert(lUserId);

    const bool nChanged = (lUserId != m_uUserId) || (strName != m_strName) || (nAge != m_nAge)
                         || (strDesc != m_strDesc);
    m_uUserId = lUserId;
    m_strName = strName;
    m_nAge = nAge;
    m_strDesc = strDesc;

    if (nChanged) emit profileChanged();
}

/*
 * 功能描述:  清空个人资料。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSessionContext::clearProfile() {
    const bool nHadProfile = (m_uUserId != 0) || !m_strName.isEmpty();

    m_uUserId = 0;
    m_strName.clear();
    m_nAge = 0;
    m_strDesc.clear();
    m_uSelfIds.clear();

    if (nHadProfile) emit profileChanged();
}

/*
 * 功能描述:  设置房间。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSessionContext::setRoom(const QString& strRoom) {
    if (strRoom == m_strRoom) return;
    m_strRoom = strRoom;
    emit roomChanged(m_strRoom);
}

/*
 * 功能描述:  清空房间。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSessionContext::clearRoom() {
    setRoom(QString());
}

/*
 * 功能描述:  设置online。
 * 作者:      JaTin
 * param [in]   nOnline          输入数据，类型 bool。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CSessionContext::setOnline(bool nOnline) {
    if (nOnline == m_nOnline) return;
    m_nOnline = nOnline;
    emit onlineChanged(m_nOnline);
}

/* namespace business */
}
