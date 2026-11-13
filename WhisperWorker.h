// WhisperWorker.h
//
// 跑在独立线程里的 whisper.cpp 封装：模型只加载一次，之后复用做逐句识别。
// whisper_full() 是同步阻塞的，绝不能放在 UI 线程。
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

struct whisper_context;   // 前置声明，避免 whisper.h 泄漏到界面代码

class WhisperWorker : public QObject {
    Q_OBJECT

public:
    explicit WhisperWorker(QString modelPath, QObject* parent = nullptr);
    ~WhisperWorker() override;

public slots:
    /// 加载模型（1~2 秒），完成后发 initialized。
    void initialize();

    /// 识别一段 16 kHz / 单声道 / 16-bit 的 PCM。
    void transcribe(const QByteArray& pcm);

signals:
    void initialized(bool ok, const QString& message);
    void finished(const QString& text, qint64 elapsedMs);
    void failed(const QString& message);

private:
    QString          modelPath_;
    whisper_context* ctx_ = nullptr;
};
