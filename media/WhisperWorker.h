/**
 * @file       WhisperWorker.h
 * @brief      CWhisperWorker 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CWhisperWorker.h。
 */

/**
 * @brief  跑在独立线程里的 whisper.cpp 封装：模型只加载一次，之后复用做逐句识别。 whisper_full() 是同步阻塞的，绝不能放在 UI 线程。
 */
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

/**
 * @brief  前置声明，避免 whisper.h 泄漏到界面代码。
 */
struct whisper_context;

/**
 * @brief   C本地语音识别Worker。
 * @author  JaTin
 */
class CWhisperWorker : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] modelPath      输入参数；类型 QString。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CWhisperWorker(QString modelPath, QObject* parent = nullptr);
    ~CWhisperWorker() override;

public slots:
    /**
     * @brief      加载模型（1~2 秒），完成后发 initialized。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void initialize();

    /**
     * @brief      识别一段 16 kHz / 单声道 / 16-bit 的 PCM。
     * @param[in] pcm            输入参数；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void transcribe(const QByteArray& pcm);

signals:
    /**
     * @brief      执行 initialized相关的数据并返回处理结果。
     * @param[in] ok             输入参数；类型 bool。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void initialized(bool ok, const QString& message);
    /**
     * @brief      执行 finished相关的数据并返回处理结果。
     * @param[in] text           消息文本；类型 const QString&。
     * @param[in] elapsedMs      输入参数；类型 qint64。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void finished(const QString& text, qint64 elapsedMs);
    /**
     * @brief      执行 failed相关的数据并返回处理结果。
     * @param[in] message        消息内容；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(const QString& message);

private:
    /**
     * @brief  模型名文件路径。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString          m_strModelPath;
    /**
     * @brief  服务端共享上下文引用，必须比本对象存活更久。 初值 = nullptr。
     * @note   类型 whisper_context*；类/结构体成员变量。
     */
    whisper_context* m_pCtx = nullptr;
};
