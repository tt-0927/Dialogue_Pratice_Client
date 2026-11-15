/**
 * @file       ProtocolBridge.h
 * @brief      CProtocolBridge 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释；
 *             2026-11-15 JaTin 增加头像参数与历史房间查询。
 */

/**
 * @brief  CProtocolBridge.h。
 */

/**
 * @brief  自研 TCP 服务端实现（局域网模式）：负责组帧/解帧、JSON 解析，转成 ChatTransport 的信号。 界面层只依赖 ChatTransport，不接触 socket 与字节流。
 */
#pragma once

#include <QObject>
#include <QString>

#include <QTcpSocket>
#include <QTimer>

#include "net/ChatTransport.h"
#include "common/Protocol.h"

/**
 * @brief   C协议协议桥接器。
 * @author  JaTin
 */
class CProtocolBridge : public ChatTransport {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CProtocolBridge(QObject* parent = nullptr);

    /**
     * @brief      建立连接 to 服务端。
     * @param[in] host           服务器主机地址；类型 const QString&。
     * @param[in] port           服务器端口；类型 quint16。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void connectToServer(const QString& host, quint16 port) override;
    /**
     * @brief      断开连接 from 服务端。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void disconnectFromServer() override;
    /**
     * @brief      判断 connected相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool isConnected() const;

    void login(const QString& name, const QString& password, int age,
               const QString& desc) override;
    /**
     * @brief      更新个人资料。
     * @param[in] name           名称或昵称；类型 const QString&。
     * @param[in] age            年龄（0 到 150）；类型 int。
     * @param[in] desc           个人简介；类型 const QString&。
     * @param[in] avatar         头像图片二进制，空表示清除头像；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void updateProfile(const QString& name, int age, const QString& desc,
                       const QByteArray& avatar) override;
    void registerAccount(const QString& username, const QString& password,
                         const QString& nickname, int age, const QString& desc) override;
    /**
     * @brief      加入房间。
     * @param[in] room           房间号；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void joinRoom(const QString& room) override;
    /**
     * @brief      退出房间。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void leaveRoom() override;

    /**
     * @brief      查询当前账号的历史房间。
     * @note       本函数无输入参数。
     * @return     无返回值；结果通过 historyRoomsReceived 信号对外体现。
     * @author     JaTin
     */
    void listHistoryRooms() override;
    /**
     * @brief      发送 chat。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendChat(const QString& text) override;
    /**
     * @brief      发送语音消息。
     * @param[in] audio          Opus 编码的音频数据；类型 const QByteArray&。
     * @param[in] durationMs     语音时长，单位毫秒；类型 int。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendVoice(const QByteArray& audio, int durationMs) override;

private slots:
    /**
     * @brief      读取 onready相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onReadyRead();
    /**
     * @brief      连接建立后恢复房间上下文并拉取历史消息。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onConnected();
    /**
     * @brief      连接断开时清理会话状态并提示用户重新连接。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onDisconnected();
    /**
     * @brief      执行 on 套接字错误。
     * @param[in] error          错误信息；类型 QAbstractSocket::SocketError。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void onSocketError(QAbstractSocket::SocketError error);

private:
    /**
     * @brief      处理协议帧。
     * @param[in] frame          协议帧；类型 const dp::Frame_S&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void handleFrame(const dp::Frame_S& frame);
    /**
     * @brief      发送 JSON 对象。
     * @param[in] op             协议操作码；类型 dp::OpCode_EN。
     * @param[in] payload        协议载荷内容；类型 const dp::Json&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void sendJson(dp::OpCode_EN op, const dp::Json& payload);

    /**
     * @brief  套接字。 初值 = nullptr。
     * @note   类型 QTcpSocket*；类/结构体成员变量。
     */
    QTcpSocket*     m_pSocket = nullptr;
    /**
     * @brief  Ping定时器。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*         m_pPingTimer = nullptr;
    /**
     * @brief  Connect定时器。 初值 = nullptr。
     * @note   类型 QTimer*；类/结构体成员变量。
     */
    QTimer*         m_pConnectTimer = nullptr;
    /**
     * @brief  Decoder。
     * @note   类型 dp::CFrameDecoder；类/结构体成员变量。
     */
    dp::CFrameDecoder m_stDecoder;
    /**
     * @brief  Connecting。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool            m_nConnecting = false;
};
