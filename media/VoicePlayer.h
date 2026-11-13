// VoicePlayer.h
//
// 语音消息播放器。全局单例——同一时刻只该有一条语音在响，
// 点新的会自动停掉上一条（多实例会叠在一起，体验很差）。
//
// 播放链路：DPV1 容器 -> Opus 解码 -> 临时 WAV -> QSoundEffect。
// 解码结果按容器内容哈希缓存成 wav 文件，重复播放不再解码。
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QSoundEffect;

class VoicePlayer : public QObject {
    Q_OBJECT

public:
    static VoicePlayer* instance();

    /// 播放一段语音；正在播别的会先停掉。
    void play(const QByteArray& container);
    void stop();
    bool isPlaying() const;

    /// 当前正在播放的容器（UI 用它判断哪个气泡该显示暂停图标）。
    QByteArray current() const { return current_; }

signals:
    void playingChanged(bool playing);
    void failed(const QString& reason);

private:
    explicit VoicePlayer(QObject* parent = nullptr);
    QString wavPathFor(const QByteArray& container) const;

    QSoundEffect* effect_ = nullptr;
    QByteArray    current_;
    bool          speaking_ = false;
    bool          pendingPlay_ = false;
};
