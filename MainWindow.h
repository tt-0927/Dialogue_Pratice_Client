// MainWindow.h
//
// 三个页面的调度器：登录 → 主页（资料 + 进房）→ 聊天。
// 同时负责断线自动重连：断开后按固定间隔重试，成功后自动重新登录并回到原房间，
// 再由服务端下发的历史消息补齐断线期间的空档。
#pragma once

#include <QByteArray>
#include <QMainWindow>
#include <QString>

#include "ChatTransport.h"

class ChatPage;
class MessageStore;
class HomePage;
class LoginPage;
class QLabel;
class QStackedWidget;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onLoginRequested(const QString& host, quint16 port, const QString& name);
    void onBridgeConnected();
    void onLoginSucceeded(qint64 userId, const QString& name, int age, const QString& desc);
    void onProfileUpdated(qint64 userId, const QString& name, int age, const QString& desc);
    void onRoomJoined(const QString& room, bool created, const QVector<MemberInfo>& members);
    void onHistoryReceived(const QString& room, const QVector<ChatMessage>& messages);
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

    ChatTransport* bridge_ = nullptr;
    MessageStore*  store_ = nullptr;
    QString        transportName_ = QStringLiteral("server");
    QStackedWidget* stack_ = nullptr;
    LoginPage*      loginPage_ = nullptr;
    HomePage*       homePage_ = nullptr;
    ChatPage*       chatPage_ = nullptr;
    QLabel*         banner_ = nullptr;
    QTimer*         reconnectTimer_ = nullptr;
    QTimer*         bannerTimer_ = nullptr;

    QString lastHost_;
    quint16 lastPort_ = 0;

    QString pendingHost_;
    quint16 pendingPort_ = 0;
    QString pendingName_;
    QString pendingRoom_;

    qint64  selfId_ = 0;
    QString selfName_;
    int     selfAge_ = 0;
    QString selfDesc_;

    bool suppressDisconnectNotice_ = false;
    bool reconnecting_ = false;
    int  reconnectAttempt_ = 0;
};
