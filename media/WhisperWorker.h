/*
 * 文件名:    WhisperWorker.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CWhisperWorker 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CWhisperWorker.h */

/* 跑在独立线程里的 whisper.cpp 封装：模型只加载一次，之后复用做逐句识别。 */
/* whisper_full() 是同步阻塞的，绝不能放在 UI 线程。 */
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/* 前置声明，避免 whisper.h 泄漏到界面代码 */
struct whisper_context;

class CWhisperWorker : public QObject {
    Q_OBJECT

public:
    explicit CWhisperWorker(QString modelPath, QObject* parent = nullptr);
    ~CWhisperWorker() override;

public slots:
    /* 加载模型（1~2 秒），完成后发 initialized。 */
    void initialize();

    /* 识别一段 16 kHz / 单声道 / 16-bit 的 PCM。 */
    void transcribe(const QByteArray& pcm);

signals:
    void initialized(bool ok, const QString& message);
    void finished(const QString& text, qint64 elapsedMs);
    void failed(const QString& message);

private:
    QString          m_strModelPath;
    whisper_context* m_pCtx = nullptr;
};
