/**
 * @file  SessionContext.cpp
 * @brief 会话状态单例的实现。
 */

#include "business/SessionContext.h"

namespace business {

SessionContext& SessionContext::instance() {
    static SessionContext context;
    return context;
}

SessionContext::SessionContext(QObject* parent) : QObject(parent) {}

void SessionContext::setProfile(qint64 userId, const QString& name, int age,
                                const QString& desc) {
    if (userId != 0) selfIds_.insert(userId);

    const bool changed = (userId != userId_) || (name != name_) || (age != age_)
                         || (desc != desc_);
    userId_ = userId;
    name_ = name;
    age_ = age;
    desc_ = desc;

    if (changed) emit profileChanged();
}

void SessionContext::clearProfile() {
    const bool hadProfile = (userId_ != 0) || !name_.isEmpty();

    userId_ = 0;
    name_.clear();
    age_ = 0;
    desc_.clear();
    selfIds_.clear();

    if (hadProfile) emit profileChanged();
}

void SessionContext::setRoom(const QString& room) {
    if (room == room_) return;
    room_ = room;
    emit roomChanged(room_);
}

void SessionContext::clearRoom() {
    setRoom(QString());
}

void SessionContext::setOnline(bool online) {
    if (online == online_) return;
    online_ = online;
    emit onlineChanged(online_);
}

}  // namespace business
