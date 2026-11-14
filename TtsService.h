/**
 * @file       TtsService.h
 * @brief      CTtsService 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CTtsService.h。
 */

/**
 * @brief  Piper 离线语音合成：把英文文本合成 WAV 并播放。 合成结果按文本哈希缓存，同一句只跑一次 Piper。
 */

/**
 * @brief  播放用 QSoundEffect 而不是 QMediaPlayer：QMediaPlayer 在「播放中途 stop() 后立刻 setSource 同一文件再 play()」时会进入 PlayingState 却不出声 （实测：连续点 6 次只有第 1 次有声音）。QSoundEffect 专为短音效设计， 反复 play() 可靠。
 */
#pragma once

#include <QHash>
#include <QObject>
#include <QString>

class QProcess;
class QSoundEffect;

/**
 * @brief   C语音合成业务服务。
 * @author  JaTin
 */
class CTtsService : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CTtsService(QObject* parent = nullptr);
    ~CTtsService() override;

    /**
     * @brief      设置 paths相关的数据并返回处理结果。
     * @param[in] piperBin       输入参数；类型 const QString&。
     * @param[in] voiceModel     输入参数；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void setPaths(const QString& piperBin, const QString& voiceModel);
    /**
     * @brief      判断 available相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool isAvailable() const;

    /**
     * @brief      合成并播放；同一句命中缓存直接播。重复调用会从头重播。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void speak(const QString& text);
    /**
     * @brief      停止 stop。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void stop();
    bool isSpeaking() const { return speaking_; }

    /**
     * @brief  最近一次 speak 的文本（UI 用来高亮对应气泡的按钮）。
     */
    QString currentText() const { return m_strCurrentText; }

signals:
    /**
     * @brief      执行 speakingchanged相关的数据并返回处理结果。
     * @param[in] speaking       输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void speakingChanged(bool speaking);
    /**
     * @brief      执行 failed相关的数据并返回处理结果。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(const QString& reason);

private:
    /**
     * @brief      播放文件。
     * @param[in] path           文件路径；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void playFile(const QString& path);
    /**
     * @brief      获取缓存文件路径 for。
     * @param[in] text           消息文本；类型 const QString&。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    QString cachePathFor(const QString& text) const;
    /**
     * @brief      执行 cleanuppart 文件。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void cleanupPartFile();

    /**
     * @brief  PiperBin。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPiperBin;
    /**
     * @brief  语音消息模型名。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strVoiceModel;
    /**
     * @brief  Process。 初值 = nullptr。
     * @note   类型 QProcess*；类/结构体成员变量。
     */
    QProcess*     m_pProcess = nullptr;
    /**
     * @brief  视觉效果。 初值 = nullptr。
     * @note   类型 QSoundEffect*；类/结构体成员变量。
     */
    QSoundEffect* m_pEffect = nullptr;

    /**
     * @brief  piper 正在写的临时文件（xxx.wav.part）
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingPart;
    /**
     * @brief  合成成功后要改名成的最终缓存路径
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingFinal;
    /**
     * @brief  Pending文本。
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strPendingText;
    /**
     * @brief  当前（或最近）要朗读的文本
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strCurrentText;

    /**
     * @brief  QSoundEffect 当前已加载的文件
     * @note   类型 QString；类/结构体成员变量。
     */
    QString m_strLoadedPath;
    /**
     * @brief  PendingPlay。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    m_nPendingPlay = false;

    /**
     * @brief  text -> wav 路径
     * @note   类型 QHash<QString, QString>；类/结构体成员变量。
     */
    QHash<QString, QString> m_strCache;
    /**
     * @brief  speaking。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool    speaking_ = false;
};
