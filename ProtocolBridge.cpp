/*
 * 文件名:    ProtocolBridge.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CProtocolBridge 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CProtocolBridge.cpp */
#include "ProtocolBridge.h"

#include "common/Base64.h"

#include <QAbstractSocket>

namespace {

/*
 * 功能描述:  执行 jstr 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  j  参数。
 * param [in]  key  参数。
 * return:    返回值，含义见功能描述。
 */
QString jstr(const dp::Json& j, const char* key) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return QString();
    return QString::fromStdString(it->get<std::string>());
}

/*
 * 功能描述:  执行 jint 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  j  参数。
 * param [in]  key  参数。
 * param [in]  def  参数。
 * return:    返回值，含义见功能描述。
 */
qint64 jint(const dp::Json& j, const char* key, qint64 def = 0) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) return def;
    return static_cast<qint64>(it->get<long long>());
}

/*
 * 功能描述:  执行 jbool 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  j  参数。
 * param [in]  key  参数。
 * param [in]  false  参数。
 * return:    返回值，含义见功能描述。
 */
bool jbool(const dp::Json& j, const char* key, bool def = false) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_boolean()) return def;
    return it->get<bool>();
}

/* namespace */
}

/*
 * 功能描述:  执行 CProtocolBridge 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CProtocolBridge::CProtocolBridge(QObject* parent) : ChatTransport(parent) {
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
 * 功能描述:  执行 connectToServer 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  host  参数。
 * param [in]  port  参数。
 * return:    void
 */
void CProtocolBridge::connectToServer(const QString& host, quint16 port) {
    if (m_pSocket->state() != QAbstractSocket::UnconnectedState) {
        m_pSocket->abort();
    }
    m_stDecoder = dp::CFrameDecoder();
    m_nConnecting = true;
    m_pSocket->connectToHost(host, port);
    m_pConnectTimer->start();
}

/*
 * 功能描述:  执行 disconnectFromServer 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CProtocolBridge::disconnectFromServer() {
    if (m_pSocket->state() == QAbstractSocket::ConnectedState) {
        m_pSocket->disconnectFromHost();
    } else {
        m_pSocket->abort();
    }
}

/*
 * 功能描述:  执行 isConnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool CProtocolBridge::isConnected() const {
    return m_pSocket->state() == QAbstractSocket::ConnectedState;
}

/*
 * 功能描述:  执行 onConnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CProtocolBridge::onConnected() {
    m_nConnecting = false;
    m_pConnectTimer->stop();
    m_pPingTimer->start();
    emit connected();
}

/*
 * 功能描述:  执行 onDisconnected 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CProtocolBridge::onDisconnected() {
    m_pPingTimer->stop();
    m_pConnectTimer->stop();
    emit disconnected();
}

/*
 * 功能描述:  执行 onSocketError 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  error  参数。
 * return:    void
 */
void CProtocolBridge::onSocketError(QAbstractSocket::SocketError error) {
    if (error == QAbstractSocket::RemoteHostClosedError) {
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
 * 功能描述:  执行 onReadyRead 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CProtocolBridge::onReadyRead() {
    const QByteArray data = m_pSocket->readAll();
    m_stDecoder.append(data.constData(), static_cast<size_t>(data.size()));

    for (;;) {
        dp::Frame_S frame;
        const dp::CFrameDecoder::Result_EN r = m_stDecoder.next(frame);
        if (r == dp::CFrameDecoder::NeedMore) break;
        if (r == dp::CFrameDecoder::Fatal) {
            emit networkError(QStringLiteral("协议错误：%1")
                                  .arg(QString::fromStdString(m_stDecoder.lastError())));
            m_pSocket->abort();
            return;
        }
        handleFrame(frame);
    }
}

/*
 * 功能描述:  执行 handleFrame 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  frame  参数。
 * return:    void
 */
void CProtocolBridge::handleFrame(const dp::Frame_S& frame) {
    dp::Json j = dp::Json::object();
    if (!frame.payload.empty()) {
        j = dp::Json::parse(frame.payload, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            emit networkError(QStringLiteral("服务端返回了非法 JSON"));
            return;
        }
    }

    switch (frame.op) {
        case dp::OpCode_EN::LoginResp:
            emit loginSucceeded(jint(j, "userId"), jstr(j, "name"),
                                static_cast<int>(jint(j, "age")), jstr(j, "desc"));
            break;

        case dp::OpCode_EN::ProfileResp:
            emit profileUpdated(jint(j, "userId"), jstr(j, "name"),
                                static_cast<int>(jint(j, "age")), jstr(j, "desc"));
            break;

        case dp::OpCode_EN::RoomState: {
            QVector<MemberInfo_S> members;
            const auto it = j.find("members");
            if (it != j.end() && it->is_array()) {
                for (const auto& m : *it) {
                    MemberInfo_S info;
                    info.id = jint(m, "id");
                    info.name = jstr(m, "name");
                    members.push_back(info);
                }
            }
            emit roomJoined(jstr(j, "room"), jbool(j, "created", false), members);
            break;
        }

        case dp::OpCode_EN::ChatPush:
            emit chatReceived(jint(j, "seq"), jint(j, "from"), jstr(j, "name"), jstr(j, "room"),
                              jstr(j, "text"), jint(j, "ts"));
            break;

        case dp::OpCode_EN::VoicePush: {
            const std::string b64 = jstr(j, "opus").toStdString();
            const std::string raw = dp::base64Decode(b64);
            emit voiceReceived(jint(j, "seq"), jint(j, "from"), jstr(j, "name"), jstr(j, "room"),
                               QByteArray(raw.data(), static_cast<int>(raw.size())),
                               static_cast<int>(jint(j, "durationMs")), jint(j, "ts"));
            break;
        }

        case dp::OpCode_EN::HistoryResp: {
            QVector<ChatMessage_S> messages;
            const auto it = j.find("messages");
            if (it != j.end() && it->is_array()) {
                messages.reserve(static_cast<int>(it->size()));
                for (const auto& m : *it) {
                    ChatMessage_S msg;
                    msg.seq = jint(m, "seq");
                    msg.from = jint(m, "from");
                    msg.name = jstr(m, "name");
                    msg.ts = jint(m, "ts");

                    /* kind 缺省视为 text，兼容没有该字段的旧服务端 */
                    const std::string kind = jstr(m, "kind").toStdString();
                    if (kind == "voice") {
                        msg.kind = MessageKind_EN::Voice;
                        msg.durationMs = jint(m, "durationMs");
                        const std::string b64 = jstr(m, "opus").toStdString();
                        const std::string raw = dp::base64Decode(b64);
                        msg.audio = QByteArray(raw.data(), static_cast<int>(raw.size()));
                    } else {
                        msg.kind = MessageKind_EN::Text;
                        msg.text = jstr(m, "text");
                    }
                    messages.push_back(msg);
                }
            }
            emit historyReceived(jstr(j, "room"), messages);
            break;
        }

        case dp::OpCode_EN::SystemMsg:
            emit systemMessage(jstr(j, "event"), jstr(j, "room"), jint(j, "userId"),
                               jstr(j, "name"), jstr(j, "oldName"), jstr(j, "text"),
                               jint(j, "ts"));
            break;

        case dp::OpCode_EN::ErrorMsg:
            emit serverError(jstr(j, "code"), jstr(j, "text"));
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
 * 功能描述:  执行 sendJson 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  op  参数。
 * param [in]  payload  参数。
 * return:    void
 */
void CProtocolBridge::sendJson(dp::OpCode_EN op, const dp::Json& payload) {
    if (m_pSocket->state() != QAbstractSocket::ConnectedState) return;
    const std::string frame = dp::encodeFrame(op, payload);
    m_pSocket->write(frame.data(), static_cast<qint64>(frame.size()));
}

/*
 * 功能描述:  执行 login 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  name  参数。
 * param [in]  age  参数。
 * param [in]  desc  参数。
 * return:    void
 */
void CProtocolBridge::login(const QString& name, int age, const QString& desc) {
    dp::Json j = dp::Json::object();
    j["name"] = name.toStdString();
    j["age"] = age;
    j["desc"] = desc.toStdString();
    sendJson(dp::OpCode_EN::Login, j);
}

/*
 * 功能描述:  执行 updateProfile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  name  参数。
 * param [in]  age  参数。
 * param [in]  desc  参数。
 * return:    void
 */
void CProtocolBridge::updateProfile(const QString& name, int age, const QString& desc) {
    dp::Json j = dp::Json::object();
    j["name"] = name.toStdString();
    j["age"] = age;
    j["desc"] = desc.toStdString();
    sendJson(dp::OpCode_EN::UpdateProfile, j);
}

/*
 * 功能描述:  执行 joinRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  room  参数。
 * return:    void
 */
void CProtocolBridge::joinRoom(const QString& room) {
    dp::Json j = dp::Json::object();
    j["room"] = room.toStdString();
    sendJson(dp::OpCode_EN::JoinRoom, j);
}

/*
 * 功能描述:  执行 leaveRoom 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CProtocolBridge::leaveRoom() {
    sendJson(dp::OpCode_EN::LeaveRoom, dp::Json::object());
}

/*
 * 功能描述:  执行 sendChat 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CProtocolBridge::sendChat(const QString& text) {
    dp::Json j = dp::Json::object();
    j["text"] = text.toStdString();
    sendJson(dp::OpCode_EN::Chat, j);
}

/*
 * 功能描述:  执行 sendVoice 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  audio  参数。
 * param [in]  durationMs  参数。
 * return:    void
 */
void CProtocolBridge::sendVoice(const QByteArray& audio, int durationMs) {
    if (audio.isEmpty()) return;

    const std::string raw(audio.constData(), static_cast<size_t>(audio.size()));
    dp::Json j = dp::Json::object();
    j["durationMs"] = durationMs;
    j["opus"] = dp::base64Encode(raw);
    sendJson(dp::OpCode_EN::Voice, j);
}
