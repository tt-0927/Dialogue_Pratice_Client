// ProtocolBridge.cpp
#include "ProtocolBridge.h"

#include "common/Base64.h"

#include <QAbstractSocket>

namespace {

QString jstr(const dp::Json& j, const char* key) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return QString();
    return QString::fromStdString(it->get<std::string>());
}

qint64 jint(const dp::Json& j, const char* key, qint64 def = 0) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_number_integer()) return def;
    return static_cast<qint64>(it->get<long long>());
}

bool jbool(const dp::Json& j, const char* key, bool def = false) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_boolean()) return def;
    return it->get<bool>();
}

}  // namespace

ProtocolBridge::ProtocolBridge(QObject* parent) : ChatTransport(parent) {
    socket_ = new QTcpSocket(this);

    connect(socket_, &QTcpSocket::readyRead, this, &ProtocolBridge::onReadyRead);
    connect(socket_, &QTcpSocket::connected, this, &ProtocolBridge::onConnected);
    connect(socket_, &QTcpSocket::disconnected, this, &ProtocolBridge::onDisconnected);
    connect(socket_, &QAbstractSocket::errorOccurred, this, &ProtocolBridge::onSocketError);

    pingTimer_ = new QTimer(this);
    pingTimer_->setInterval(30000);   // 服务端 90s 无帧判定掉线，30s 心跳留足余量
    connect(pingTimer_, &QTimer::timeout, this, [this] {
        if (socket_->state() == QAbstractSocket::ConnectedState) {
            sendJson(dp::OpCode::Ping, dp::Json::object());
        }
    });

    connectTimer_ = new QTimer(this);
    connectTimer_->setSingleShot(true);
    connectTimer_->setInterval(6000);
    connect(connectTimer_, &QTimer::timeout, this, [this] {
        if (!connecting_) return;
        connecting_ = false;
        socket_->abort();
        emit networkError(QStringLiteral("连接超时：请检查服务端地址与端口"));
    });
}

void ProtocolBridge::connectToServer(const QString& host, quint16 port) {
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->abort();
    }
    decoder_ = dp::FrameDecoder();
    connecting_ = true;
    socket_->connectToHost(host, port);
    connectTimer_->start();
}

void ProtocolBridge::disconnectFromServer() {
    if (socket_->state() == QAbstractSocket::ConnectedState) {
        socket_->disconnectFromHost();
    } else {
        socket_->abort();
    }
}

bool ProtocolBridge::isConnected() const {
    return socket_->state() == QAbstractSocket::ConnectedState;
}

void ProtocolBridge::onConnected() {
    connecting_ = false;
    connectTimer_->stop();
    pingTimer_->start();
    emit connected();
}

void ProtocolBridge::onDisconnected() {
    pingTimer_->stop();
    connectTimer_->stop();
    emit disconnected();
}

void ProtocolBridge::onSocketError(QAbstractSocket::SocketError error) {
    if (error == QAbstractSocket::RemoteHostClosedError) {
        return;   // 交给 disconnected() 统一处理，避免重复提示
    }
    if (connecting_) {
        connecting_ = false;
        connectTimer_->stop();
    }
    emit networkError(socket_->errorString());
}

void ProtocolBridge::onReadyRead() {
    const QByteArray data = socket_->readAll();
    decoder_.append(data.constData(), static_cast<size_t>(data.size()));

    for (;;) {
        dp::Frame frame;
        const dp::FrameDecoder::Result r = decoder_.next(frame);
        if (r == dp::FrameDecoder::NeedMore) break;
        if (r == dp::FrameDecoder::Fatal) {
            emit networkError(QStringLiteral("协议错误：%1")
                                  .arg(QString::fromStdString(decoder_.lastError())));
            socket_->abort();
            return;
        }
        handleFrame(frame);
    }
}

void ProtocolBridge::handleFrame(const dp::Frame& frame) {
    dp::Json j = dp::Json::object();
    if (!frame.payload.empty()) {
        j = dp::Json::parse(frame.payload, nullptr, false);
        if (j.is_discarded() || !j.is_object()) {
            emit networkError(QStringLiteral("服务端返回了非法 JSON"));
            return;
        }
    }

    switch (frame.op) {
        case dp::OpCode::LoginResp:
            emit loginSucceeded(jint(j, "userId"), jstr(j, "name"),
                                static_cast<int>(jint(j, "age")), jstr(j, "desc"));
            break;

        case dp::OpCode::ProfileResp:
            emit profileUpdated(jint(j, "userId"), jstr(j, "name"),
                                static_cast<int>(jint(j, "age")), jstr(j, "desc"));
            break;

        case dp::OpCode::RoomState: {
            QVector<MemberInfo> members;
            const auto it = j.find("members");
            if (it != j.end() && it->is_array()) {
                for (const auto& m : *it) {
                    MemberInfo info;
                    info.id = jint(m, "id");
                    info.name = jstr(m, "name");
                    members.push_back(info);
                }
            }
            emit roomJoined(jstr(j, "room"), jbool(j, "created", false), members);
            break;
        }

        case dp::OpCode::ChatPush:
            emit chatReceived(jint(j, "seq"), jint(j, "from"), jstr(j, "name"), jstr(j, "room"),
                              jstr(j, "text"), jint(j, "ts"));
            break;

        case dp::OpCode::VoicePush: {
            const std::string b64 = jstr(j, "opus").toStdString();
            const std::string raw = dp::base64Decode(b64);
            emit voiceReceived(jint(j, "seq"), jint(j, "from"), jstr(j, "name"), jstr(j, "room"),
                               QByteArray(raw.data(), static_cast<int>(raw.size())),
                               static_cast<int>(jint(j, "durationMs")), jint(j, "ts"));
            break;
        }

        case dp::OpCode::HistoryResp: {
            QVector<ChatMessage> messages;
            const auto it = j.find("messages");
            if (it != j.end() && it->is_array()) {
                messages.reserve(static_cast<int>(it->size()));
                for (const auto& m : *it) {
                    ChatMessage msg;
                    msg.seq = jint(m, "seq");
                    msg.from = jint(m, "from");
                    msg.name = jstr(m, "name");
                    msg.ts = jint(m, "ts");

                    // kind 缺省视为 text，兼容没有该字段的旧服务端
                    const std::string kind = jstr(m, "kind").toStdString();
                    if (kind == "voice") {
                        msg.kind = MessageKind::Voice;
                        msg.durationMs = jint(m, "durationMs");
                        const std::string b64 = jstr(m, "opus").toStdString();
                        const std::string raw = dp::base64Decode(b64);
                        msg.audio = QByteArray(raw.data(), static_cast<int>(raw.size()));
                    } else {
                        msg.kind = MessageKind::Text;
                        msg.text = jstr(m, "text");
                    }
                    messages.push_back(msg);
                }
            }
            emit historyReceived(jstr(j, "room"), messages);
            break;
        }

        case dp::OpCode::SystemMsg:
            emit systemMessage(jstr(j, "event"), jstr(j, "room"), jint(j, "userId"),
                               jstr(j, "name"), jstr(j, "oldName"), jstr(j, "text"),
                               jint(j, "ts"));
            break;

        case dp::OpCode::ErrorMsg:
            emit serverError(jstr(j, "code"), jstr(j, "text"));
            break;

        case dp::OpCode::Pong:
        case dp::OpCode::RoomList:
            break;   // 心跳应答 / 房间列表：客户端当前界面暂不展示

        default:
            break;
    }
}

void ProtocolBridge::sendJson(dp::OpCode op, const dp::Json& payload) {
    if (socket_->state() != QAbstractSocket::ConnectedState) return;
    const std::string frame = dp::encodeFrame(op, payload);
    socket_->write(frame.data(), static_cast<qint64>(frame.size()));
}

void ProtocolBridge::login(const QString& name, int age, const QString& desc) {
    dp::Json j = dp::Json::object();
    j["name"] = name.toStdString();
    j["age"] = age;
    j["desc"] = desc.toStdString();
    sendJson(dp::OpCode::Login, j);
}

void ProtocolBridge::updateProfile(const QString& name, int age, const QString& desc) {
    dp::Json j = dp::Json::object();
    j["name"] = name.toStdString();
    j["age"] = age;
    j["desc"] = desc.toStdString();
    sendJson(dp::OpCode::UpdateProfile, j);
}

void ProtocolBridge::joinRoom(const QString& room) {
    dp::Json j = dp::Json::object();
    j["room"] = room.toStdString();
    sendJson(dp::OpCode::JoinRoom, j);
}

void ProtocolBridge::leaveRoom() {
    sendJson(dp::OpCode::LeaveRoom, dp::Json::object());
}

void ProtocolBridge::sendChat(const QString& text) {
    dp::Json j = dp::Json::object();
    j["text"] = text.toStdString();
    sendJson(dp::OpCode::Chat, j);
}

void ProtocolBridge::sendVoice(const QByteArray& audio, int durationMs) {
    if (audio.isEmpty()) return;

    const std::string raw(audio.constData(), static_cast<size_t>(audio.size()));
    dp::Json j = dp::Json::object();
    j["durationMs"] = durationMs;
    j["opus"] = dp::base64Encode(raw);
    sendJson(dp::OpCode::Voice, j);
}
