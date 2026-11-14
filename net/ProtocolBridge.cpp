/*
 * 文件名:    ProtocolBridge.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CProtocolBridge 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CProtocolBridge.cpp */
#include "net/ProtocolBridge.h"

#include "common/Base64.h"

#include <QAbstractSocket>

namespace {

/*
 * 功能描述:  获取jstr。
 * 作者:      JaTin
 * param [in]   stJ              输入数据，类型 const dp::Json&。
 * param [in]   strKey           缓存键，类型 const char*。
 * return:    返回构造好的字符串；无内容时返回空字符串。
 */
QString jstr(const dp::Json& stJ, const char* strKey) {
    const auto stIt = stJ.find(strKey);
    if (stIt == stJ.end() || !stIt->is_string()) return QString();
    return QString::fromStdString(stIt->get<std::string>());
}

/*
 * 功能描述:  获取jint。
 * 作者:      JaTin
 * param [in]   stJ              输入数据，类型 const dp::Json&。
 * param [in]   strKey           缓存键，类型 const char*。
 * param [in]   lDef             输入数据，类型 qint64。
 * return:    返回处理结果码；0 表示成功，负值或 -1 表示失败。
 */
qint64 jint(const dp::Json& stJ, const char* strKey, qint64 lDef = 0) {
    const auto stIt = stJ.find(strKey);
    if (stIt == stJ.end() || !stIt->is_number_integer()) return lDef;
    return static_cast<qint64>(stIt->get<long long>());
}

/*
 * 功能描述:  获取jbool。
 * 作者:      JaTin
 * param [in]   stJ              输入数据，类型 const dp::Json&。
 * param [in]   strKey           缓存键，类型 const char*。
 * param [in]   lDef             输入数据，类型 bool。
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool jbool(const dp::Json& stJ, const char* strKey, bool lDef = false) {
    const auto stIt = stJ.find(strKey);
    if (stIt == stJ.end() || !stIt->is_boolean()) return lDef;
    return stIt->get<bool>();
}

/* namespace */
}

/*
 * 功能描述:  构造 CProtocolBridge 对象，完成成员初始化与依赖注入。
 * 作者:      JaTin
 * param [in]   pParent          Qt 父对象指针，用于对象树所有权管理，类型 QObject* parent) : ChatTransport(。
 * return:    无返回值（构造函数或析构函数）。
 */
CProtocolBridge::CProtocolBridge(QObject* pParent) : ChatTransport(pParent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pSocket = new QTcpSocket(this);

    connect(m_pSocket, &QTcpSocket::readyRead, this, &CProtocolBridge::onReadyRead);
    connect(m_pSocket, &QTcpSocket::connected, this, &CProtocolBridge::onConnected);
    connect(m_pSocket, &QTcpSocket::disconnected, this, &CProtocolBridge::onDisconnected);
    connect(m_pSocket, &QAbstractSocket::errorOccurred, this, &CProtocolBridge::onSocketError);

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pPingTimer = new QTimer(this);
    /* 服务端 90s 无帧判定掉线，30s 心跳留足余量 */
    m_pPingTimer->setInterval(30000);
    connect(m_pPingTimer, &QTimer::timeout, this, [this] {
        if (m_pSocket->state() == QAbstractSocket::ConnectedState) {
            sendJson(dp::OpCode_EN::Ping, dp::Json::object());
        }
    });

    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pConnectTimer = new QTimer(this);
    m_pConnectTimer->setSingleShot(true);
    m_pConnectTimer->setInterval(6000);
    connect(m_pConnectTimer, &QTimer::timeout, this, [this] {
        if (!m_nConnecting) return;
        m_nConnecting = false;
        m_pSocket->abort();
        emit networkError(QStringLiteral("连接超时：请检查服务端地址与端口"));
    });
}

/*
 * 功能描述:  建立连接to 服务端。
 * 作者:      JaTin
 * param [in]   strHost          服务器主机地址，类型 const QString&。
 * param [in]   uPort            服务器端口，类型 quint16。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::connectToServer(const QString& strHost, quint16 uPort) {
    if (m_pSocket->state() != QAbstractSocket::UnconnectedState) {
        m_pSocket->abort();
    }
    m_stDecoder = dp::CFrameDecoder();
    m_nConnecting = true;
    m_pSocket->connectToHost(strHost, uPort);
    m_pConnectTimer->start();
}

/*
 * 功能描述:  断开连接from 服务端。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::disconnectFromServer() {
    if (m_pSocket->state() == QAbstractSocket::ConnectedState) {
        m_pSocket->disconnectFromHost();
    } else {
        m_pSocket->abort();
    }
}

/*
 * 功能描述:  判断connected。
 * 作者:      JaTin
 * return:    成功返回 true；参数非法或执行失败时返回 false。
 */
bool CProtocolBridge::isConnected() const {
    return m_pSocket->state() == QAbstractSocket::ConnectedState;
}

/*
 * 功能描述:  连接建立后恢复房间上下文并拉取历史消息。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::onConnected() {
    m_nConnecting = false;
    m_pConnectTimer->stop();
    m_pPingTimer->start();
    emit connected();
}

/*
 * 功能描述:  连接断开时清理会话状态并提示用户重新连接。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::onDisconnected() {
    m_pPingTimer->stop();
    m_pConnectTimer->stop();
    emit disconnected();
}

/*
 * 功能描述:  执行on 套接字错误。
 * 作者:      JaTin
 * param [in]   stError          错误信息，类型 QAbstractSocket::SocketError。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::onSocketError(QAbstractSocket::SocketError stError) {
    if (stError == QAbstractSocket::RemoteHostClosedError) {
        /* 交给 disconnected() 统一处理，避免重复提示 */
        return;
    }
    if (m_nConnecting) {
        m_nConnecting = false;
        m_pConnectTimer->stop();
    }
    emit networkError(m_pSocket->errorString());
}

/*
 * 功能描述:  读取onready。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::onReadyRead() {
    const QByteArray byData = m_pSocket->readAll();
    m_stDecoder.append(byData.constData(), static_cast<size_t>(byData.size()));

    for (;;) {
        dp::Frame_S stFrame;
        const dp::CFrameDecoder::Result_EN stR = m_stDecoder.next(stFrame);
        if (stR == dp::CFrameDecoder::NeedMore) break;
        if (stR == dp::CFrameDecoder::Fatal) {
            emit networkError(QStringLiteral("协议错误：%1")
                                  .arg(QString::fromStdString(m_stDecoder.lastError())));
            m_pSocket->abort();
            return;
        }
        handleFrame(stFrame);
    }
}

/*
 * 功能描述:  处理协议帧。
 * 作者:      JaTin
 * param [in]   stFrame          协议帧，类型 const dp::Frame_S&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::handleFrame(const dp::Frame_S& stFrame) {
    dp::Json stJ = dp::Json::object();
    if (!stFrame.payload.empty()) {
        stJ = dp::Json::parse(stFrame.payload, nullptr, false);
        if (stJ.is_discarded() || !stJ.is_object()) {
            emit networkError(QStringLiteral("服务端返回了非法 JSON"));
            return;
        }
    }

    switch (stFrame.op) {
        case dp::OpCode_EN::LoginResp: {
            /* 服务端统一下发的 LLM 配置（可选）：先发配置，再发登录成功， */
            /* 保证外部处理 loginSucceeded 时配置已经就位。 */
            const auto stLlmIt = stJ.find("llmConfig");
            if (stLlmIt != stJ.end() && stLlmIt->is_object()) {
                emit llmConfigReceived(QByteArray::fromStdString(stLlmIt->dump()));
            }
            emit loginSucceeded(jint(stJ, "userId"), jstr(stJ, "name"),
                                static_cast<int>(jint(stJ, "age")), jstr(stJ, "desc"));
            break;
        }

        case dp::OpCode_EN::ProfileResp:
            emit profileUpdated(jint(stJ, "userId"), jstr(stJ, "name"),
                                static_cast<int>(jint(stJ, "age")), jstr(stJ, "desc"));
            break;

        case dp::OpCode_EN::RoomState: {
            QVector<MemberInfo_S> aMembers;
            const auto stIt = stJ.find("members");
            if (stIt != stJ.end() && stIt->is_array()) {
                for (const auto& stM : *stIt) {
                    MemberInfo_S stInfo;
                    stInfo.id = jint(stM, "id");
                    stInfo.name = jstr(stM, "name");
                    aMembers.push_back(stInfo);
                }
            }
            emit roomJoined(jstr(stJ, "room"), jbool(stJ, "created", false), aMembers);
            break;
        }

        case dp::OpCode_EN::ChatPush:
            emit chatReceived(jint(stJ, "seq"), jint(stJ, "from"), jstr(stJ, "name"), jstr(stJ, "room"),
                              jstr(stJ, "text"), jint(stJ, "ts"));
            break;

        case dp::OpCode_EN::RegisterResp:
            emit registerSucceeded(jint(stJ, "userId"), jstr(stJ, "username"),
                                   jstr(stJ, "nickname"));
            break;

        case dp::OpCode_EN::VoicePush: {
            const std::string strB64 = jstr(stJ, "opus").toStdString();
            const std::string strRaw = dp::base64Decode(strB64);
            emit voiceReceived(jint(stJ, "seq"), jint(stJ, "from"), jstr(stJ, "name"), jstr(stJ, "room"),
                               QByteArray(strRaw.data(), static_cast<int>(strRaw.size())),
                               static_cast<int>(jint(stJ, "durationMs")), jint(stJ, "ts"));
            break;
        }

        case dp::OpCode_EN::HistoryResp: {
            QVector<ChatMessage_S> aMessages;
            const auto stIt = stJ.find("messages");
            if (stIt != stJ.end() && stIt->is_array()) {
                aMessages.reserve(static_cast<int>(stIt->size()));
                for (const auto& stM : *stIt) {
                    ChatMessage_S stMsg;
                    stMsg.seq = jint(stM, "seq");
                    stMsg.from = jint(stM, "from");
                    stMsg.name = jstr(stM, "name");
                    stMsg.ts = jint(stM, "ts");

                    /* kind 缺省视为 text，兼容没有该字段的旧服务端 */
                    const std::string strKind = jstr(stM, "kind").toStdString();
                    if (strKind == "voice") {
                        stMsg.kind = MessageKind_EN::Voice;
                        stMsg.durationMs = jint(stM, "durationMs");
                        const std::string strB64 = jstr(stM, "opus").toStdString();
                        const std::string strRaw = dp::base64Decode(strB64);
                        stMsg.audio = QByteArray(strRaw.data(), static_cast<int>(strRaw.size()));
                    } else {
                        stMsg.kind = MessageKind_EN::Text;
                        stMsg.text = jstr(stM, "text");
                    }
                    aMessages.push_back(stMsg);
                }
            }
            emit historyReceived(jstr(stJ, "room"), aMessages);
            break;
        }

        case dp::OpCode_EN::SystemMsg:
            emit systemMessage(jstr(stJ, "event"), jstr(stJ, "room"), jint(stJ, "userId"),
                               jstr(stJ, "name"), jstr(stJ, "oldName"), jstr(stJ, "text"),
                               jint(stJ, "ts"));
            break;

        case dp::OpCode_EN::ErrorMsg:
            emit serverError(jstr(stJ, "code"), jstr(stJ, "text"));
            break;

        case dp::OpCode_EN::Pong:
        case dp::OpCode_EN::RoomList:
            /* 心跳应答 / 房间列表：客户端当前界面暂不展示 */
            break;

        default:
            break;
    }
}

/*
 * 功能描述:  发送JSON 对象；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * param [in]   enOp             协议操作码，类型 dp::OpCode_EN。
 * param [in]   stPayload        协议载荷内容，类型 const dp::Json&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::sendJson(dp::OpCode_EN enOp, const dp::Json& stPayload) {
    if (m_pSocket->state() != QAbstractSocket::ConnectedState) return;
    const std::string stFrame = dp::encodeFrame(enOp, stPayload);
    m_pSocket->write(stFrame.data(), static_cast<qint64>(stFrame.size()));
}

/*
 * 功能描述:  完成登录login；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   strPassword      登录密码，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::login(const QString& strName, const QString& strPassword, int nAge,
                          const QString& strDesc) {
    dp::Json stJ = dp::Json::object();
    stJ["name"] = strName.toStdString();
    stJ["age"] = nAge;
    stJ["desc"] = strDesc.toStdString();
    /* 空密码不带字段：服务端据此区分「账号登录」与「匿名登录」 */
    if (!strPassword.isEmpty()) stJ["password"] = strPassword.toStdString();
    sendJson(dp::OpCode_EN::Login, stJ);
}

/*
 * 功能描述:  更新个人资料；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * param [in]   strName          名称或昵称，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::updateProfile(const QString& strName, int nAge, const QString& strDesc) {
    dp::Json stJ = dp::Json::object();
    stJ["name"] = strName.toStdString();
    stJ["age"] = nAge;
    stJ["desc"] = strDesc.toStdString();
    sendJson(dp::OpCode_EN::UpdateProfile, stJ);
}

/*
 * 功能描述:  注册账号；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * param [in]   strUsername      登录用户名，类型 const QString&。
 * param [in]   strPassword      登录密码，类型 const QString&。
 * param [in]   strNickname      用户昵称，类型 const QString&。
 * param [in]   nAge             年龄（0 到 150），类型 int。
 * param [in]   strDesc          个人简介，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::registerAccount(const QString& strUsername, const QString& strPassword,
                                     const QString& strNickname, int nAge, const QString& strDesc) {
    dp::Json stJ = dp::Json::object();
    stJ["username"] = strUsername.toStdString();
    stJ["password"] = strPassword.toStdString();
    /* 可选项：空值不发，让服务端用默认规则填充 */
    if (!strNickname.isEmpty()) stJ["nickname"] = strNickname.toStdString();
    if (nAge > 0) stJ["age"] = nAge;
    if (!strDesc.isEmpty()) stJ["desc"] = strDesc.toStdString();
    sendJson(dp::OpCode_EN::Register, stJ);
}

/*
 * 功能描述:  加入房间；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * param [in]   strRoom          房间号，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::joinRoom(const QString& strRoom) {
    dp::Json stJ = dp::Json::object();
    stJ["room"] = strRoom.toStdString();
    sendJson(dp::OpCode_EN::JoinRoom, stJ);
}

/*
 * 功能描述:  退出房间；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::leaveRoom() {
    sendJson(dp::OpCode_EN::LeaveRoom, dp::Json::object());
}

/*
 * 功能描述:  发送chat；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * param [in]   strText          消息文本，类型 const QString&。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::sendChat(const QString& strText) {
    dp::Json stJ = dp::Json::object();
    stJ["text"] = strText.toStdString();
    sendJson(dp::OpCode_EN::Chat, stJ);
}

/*
 * 功能描述:  发送语音消息；实现上向目标会话或整个房间下发数据。
 * 作者:      JaTin
 * param [in]   byAudio          Opus 编码的音频数据，类型 const QByteArray&。
 * param [in]   nDurationMs      语音时长，单位毫秒，类型 int。
 * return:    无返回值；处理结果通过成员状态或信号对外体现。
 */
void CProtocolBridge::sendVoice(const QByteArray& byAudio, int nDurationMs) {
    if (byAudio.isEmpty()) return;

    const std::string strRaw(byAudio.constData(), static_cast<size_t>(byAudio.size()));
    dp::Json stJ = dp::Json::object();
    stJ["durationMs"] = nDurationMs;
    stJ["opus"] = dp::base64Encode(strRaw);
    sendJson(dp::OpCode_EN::Voice, stJ);
}
