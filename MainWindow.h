/*
 * 文件名:    MainWindow.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CMainWindow 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CMainWindow.h */

/* 三个页面的调度器：登录 → 主页（资料 + 进房）→ 聊天。 */
/* 同时负责断线自动重连：断开后按固定间隔重试，成功后自动重新登录并回到原房间， */
/* 再由服务端下发的历史消息补齐断线期间的空档。 */
#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include "ChatTransport.h"

class ChatPage;
class CMessageStore;
class CHomePage;
class CLoginPage;
class QLabel;
class QStackedWidget;
class QTimer;

class CMainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit CMainWindow(QWidget* parent = nullptr);
    ~CMainWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onLoginRequested(const QString& host, quint16 port, const QString& name);
    void onBridgeConnected();
    void onLoginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);
    void onProfileUpdated(qint64 userId, const QString& name, int age, const QString& desc);
    void onRoomJoined(const QString& room, bool created, const QVector<MemberInfo_S>& members);
    void onHistoryReceived(const QString& room, const QVector<ChatMessage_S>& messages);
    void onChatReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                        const QString& text, qint64 ts);
    void onVoiceReceived(qint64 seq, qint64 from, const QString& name, const QString& room,
                         const QByteArray& audio, int durationMs, qint64 ts);
    void onSystemMessage(const QString& event, const QString& room, qint64 userId,
                         const QString& name, const QString& oldName,
                         const QString& text, qint64 ts);
    void onServerError(const QString& code, const QString& text);
    void onNetworkError(const QString& message);
    void onDisconnected();
    void onReconnectTimeout();

    void onEditProfile();
    void onJoinRoom(const QString& room);
    void onSendMessage(const QString& text);
    void onLeaveRoom();
    void onLogout();

private:
    static constexpr int kMaxReconnectAttempts = 6;
    static constexpr int kReconnectIntervalMs = 3000;

    void showBanner(const QString& text, bool error);
    void positionBanner();
    void resetToLogin(const QString& reason);

    void scheduleReconnect(const QString& reason);
    void finishReconnect();
    void resetReconnectState();

    ChatTransport* m_pBridge = nullptr;
    CMessageStore*  m_pStore = nullptr;
    QString        m_strTransportName = QStringLiteral("server");
    QStackedWidget* m_pStack = nullptr;
    CLoginPage*      m_pLoginPage = nullptr;
    CHomePage*       m_pHomePage = nullptr;
    ChatPage*       m_pChatPage = nullptr;
    QLabel*         m_pBanner = nullptr;
    QTimer*         m_pReconnectTimer = nullptr;
    QTimer*         m_pBannerTimer = nullptr;

    QString m_strLastHost;
    quint16 m_uLastPort = 0;

    QString m_strPendingHost;
    quint16 m_uPendingPort = 0;
    QString m_strPendingName;
    QString m_strPendingRoom;

    qint64  m_uSelfId = 0;
    QString m_strSelfName;
    int     m_nSelfAge = 0;
    QString m_strSelfDesc;

    bool m_nSuppressDisconnectNotice = false;
    bool m_nReconnecting = false;
    int  m_nReconnectAttempt = 0;
};
