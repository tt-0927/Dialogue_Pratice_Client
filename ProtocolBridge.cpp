/**
 * @file       ProtocolBridge.cpp
 * @brief      CProtocolBridge 实现文件，包含相关接口的具体实现。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CProtocolBridge.cpp。
 */
#include "ProtocolBridge.h"

#include "common/Base64.h"

#include <QAbstractSocket>

namespace {

/**
 * @brief      获取 jstr相关的数据并返回处理结果。
 * @param[in] stJ            输入数据；类型 const dp::Json&。
 * @param[in] strKey         缓存键；类型 const char*。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
QString jstr(const dp::Json& stJ, const char* strKey) {
    const auto stIt = stJ.find(strKey);
    if (stIt == stJ.end() || !stIt->is_string()) return QString();
    return QString::fromStdString(stIt->get<std::string>());
}

/**
 * @brief      获取 jint相关的数据并返回处理结果。
 * @param[in] stJ            输入数据；类型 const dp::Json&。
 * @param[in] strKey         缓存键；类型 const char*。
 * @param[in] lDef           输入数据；类型 qint64。
 * @return     返回处理结果码；0 表示成功，负值或 -1 表示失败。
 * @author     JaTin
 */
qint64 jint(const dp::Json& stJ, const char* strKey, qint64 lDef = 0) {
    const auto stIt = stJ.find(strKey);
    if (stIt == stJ.end() || !stIt->is_number_integer()) return lDef;
    return static_cast<qint64>(stIt->get<long long>());
}

/**
 * @brief      获取 jbool相关的数据并返回处理结果。
 * @param[in] stJ            输入数据；类型 const dp::Json&。
 * @param[in] strKey         缓存键；类型 const char*。
 * @param[in] lDef           输入数据；类型 bool。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool jbool(const dp::Json& stJ, const char* strKey, bool lDef = false) {
    const auto stIt = stJ.find(strKey);
    if (stIt == stJ.end() || !stIt->is_boolean()) return lDef;
    return stIt->get<bool>();
}

/**
 * @brief  namespace。
 */
}

/**
 * @brief      构造 CProtocolBridge 对象，完成成员初始化与依赖注入。
 * @param[in] pParent        Qt 父对象指针，用于对象树所有权管理；类型 QObject* pParent) : ChatTransport(。
 * @return     构造/析构函数无返回值。
 * @author     JaTin
 */
CProtocolBridge::CProtocolBridge(QObject* pParent) : ChatTransport(pParent) {
    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pSocket = new QTcpSocket(this);

    connect(m_pSocket, &QTcpSocket::readyRead, this, &CProtocolBridge::onReadyRead);
    connect(m_pSocket, &QTcpSocket::connected, this, &CProtocolBridge::onConnected);
    connect(m_pSocket, &QTcpSocket::disconnected, this, &CProtocolBridge::onDisconnected);
    connect(m_pSocket, &QAbstractSocket::errorOccurred, this, &CProtocolBridge::onSocketError);

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
    m_pPingTimer = new QTimer(this);
    /**
     * @brief  服务端 90s 无帧判定掉线，30s 心跳留足余量。
     */
    m_pPingTimer->setInterval(30000);
    connect(m_pPingTimer, &QTimer::timeout, this, [this] {
        if (m_pSocket->state() == QAbstractSocket::ConnectedState) {
            sendJson(dp::OpCode_EN::Ping, dp::Json::object());
        }
    });

    /**
     * @brief  特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。
     */
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

/**
 * @brief      建立连接 to 服务端。
 * @param[in] strHost        服务器主机地址；类型 const QString&。
 * @param[in] uPort          服务器端口；类型 quint16。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief      断开连接 from 服务端。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::disconnectFromServer() {
    if (m_pSocket->state() == QAbstractSocket::ConnectedState) {
        m_pSocket->disconnectFromHost();
    } else {
        m_pSocket->abort();
    }
}

/**
 * @brief      判断 connected相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     成功返回 true；参数非法或执行失败时返回 false。
 * @author     JaTin
 */
bool CProtocolBridge::isConnected() const {
    return m_pSocket->state() == QAbstractSocket::ConnectedState;
}

/**
 * @brief      连接建立后恢复房间上下文并拉取历史消息。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::onConnected() {
    m_nConnecting = false;
    m_pConnectTimer->stop();
    m_pPingTimer->start();
    emit connected();
}

/**
 * @brief      连接断开时清理会话状态并提示用户重新连接。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::onDisconnected() {
    m_pPingTimer->stop();
    m_pConnectTimer->stop();
    emit disconnected();
}

/**
 * @brief      执行 on 套接字错误。
 * @param[in] stError        错误信息；类型 QAbstractSocket::SocketError。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::onSocketError(QAbstractSocket::SocketError stError) {
    if (stError == QAbstractSocket::RemoteHostClosedError) {
        /**
         * @brief  交给 disconnected() 统一处理，避免重复提示。
         */
        return;
    }
    if (m_nConnecting) {
        m_nConnecting = false;
        m_pConnectTimer->stop();
    }
    emit networkError(m_pSocket->errorString());
}

/**
 * @brief      读取 onready相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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

/**
 * @brief      处理协议帧。
 * @param[in] stFrame        协议帧；类型 const dp::Frame_S&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
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
        case dp::OpCode_EN::LoginResp:
            emit loginSucceeded(jint(stJ, "userId"), jstr(stJ, "name"),
                                static_cast<int>(jint(stJ, "age")), jstr(stJ, "desc"));
            break;

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

                    /**
                     * @brief  kind 缺省视为 text，兼容没有该字段的旧服务端。
                     */
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
            /**
             * @brief  心跳应答 / 房间列表：客户端当前界面暂不展示。
             */
            break;

        default:
            break;
    }
}

/**
 * @brief      发送 JSON 对象；实现上向目标会话或整个房间下发数据。
 * @param[in] enOp           协议操作码；类型 dp::OpCode_EN。
 * @param[in] stPayload      协议载荷内容；类型 const dp::Json&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::sendJson(dp::OpCode_EN enOp, const dp::Json& stPayload) {
    if (m_pSocket->state() != QAbstractSocket::ConnectedState) return;
    const std::string stFrame = dp::encodeFrame(enOp, stPayload);
    m_pSocket->write(stFrame.data(), static_cast<qint64>(stFrame.size()));
}

/**
 * @brief      完成登录 login；实现上向目标会话或整个房间下发数据。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::login(const QString& strName, int nAge, const QString& strDesc) {
    dp::Json stJ = dp::Json::object();
    stJ["name"] = strName.toStdString();
    stJ["age"] = nAge;
    stJ["desc"] = strDesc.toStdString();
    sendJson(dp::OpCode_EN::Login, stJ);
}

/**
 * @brief      更新个人资料；实现上向目标会话或整个房间下发数据。
 * @param[in] strName        名称或昵称；类型 const QString&。
 * @param[in] nAge           年龄（0 到 150）；类型 int。
 * @param[in] strDesc        个人简介；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::updateProfile(const QString& strName, int nAge, const QString& strDesc) {
    dp::Json stJ = dp::Json::object();
    stJ["name"] = strName.toStdString();
    stJ["age"] = nAge;
    stJ["desc"] = strDesc.toStdString();
    sendJson(dp::OpCode_EN::UpdateProfile, stJ);
}

/**
 * @brief      加入房间；实现上向目标会话或整个房间下发数据。
 * @param[in] strRoom        房间号；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::joinRoom(const QString& strRoom) {
    dp::Json stJ = dp::Json::object();
    stJ["room"] = strRoom.toStdString();
    sendJson(dp::OpCode_EN::JoinRoom, stJ);
}

/**
 * @brief      退出房间；实现上向目标会话或整个房间下发数据。
 * @note       本函数无输入参数。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::leaveRoom() {
    sendJson(dp::OpCode_EN::LeaveRoom, dp::Json::object());
}

/**
 * @brief      发送 chat；实现上向目标会话或整个房间下发数据。
 * @param[in] strText        消息文本；类型 const QString&。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::sendChat(const QString& strText) {
    dp::Json stJ = dp::Json::object();
    stJ["text"] = strText.toStdString();
    sendJson(dp::OpCode_EN::Chat, stJ);
}

/**
 * @brief      发送语音消息；实现上向目标会话或整个房间下发数据。
 * @param[in] byAudio        Opus 编码的音频数据；类型 const QByteArray&。
 * @param[in] nDurationMs    语音时长，单位毫秒；类型 int。
 * @return     无返回值；处理结果通过成员状态或信号对外体现。
 * @author     JaTin
 */
void CProtocolBridge::sendVoice(const QByteArray& byAudio, int nDurationMs) {
    if (byAudio.isEmpty()) return;

    const std::string strRaw(byAudio.constData(), static_cast<size_t>(byAudio.size()));
    dp::Json stJ = dp::Json::object();
    stJ["durationMs"] = nDurationMs;
    stJ["opus"] = dp::base64Encode(strRaw);
    sendJson(dp::OpCode_EN::Voice, stJ);
}
