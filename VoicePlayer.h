/**
 * @file       VoicePlayer.h
 * @brief      CVoicePlayer 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  CVoicePlayer.h。
 */

/**
 * @brief  语音消息播放器。全局单例——同一时刻只该有一条语音在响， 点新的会自动停掉上一条（多实例会叠在一起，体验很差）。
 */

/**
 * @brief  播放链路：DPV1 容器 -> Opus 解码 -> 临时 WAV -> QSoundEffect。 解码结果按容器内容哈希缓存成 wav 文件，重复播放不再解码。
 */
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

class QSoundEffect;

/**
 * @brief   C语音消息播放器。
 * @author  JaTin
 */
class CVoicePlayer : public QObject {
    Q_OBJECT

public:
    /**
     * @brief      获取本类的进程内单例对象。
     * @note       本函数无输入参数。
     * @return     成功返回对象指针；未找到或失败时返回 nullptr。
     * @author     JaTin
     */
    static CVoicePlayer* instance();

    /**
     * @brief      播放一段语音；正在播别的会先停掉。
     * @param[in] container      容器；类型 const QByteArray&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void play(const QByteArray& container);
    /**
     * @brief      停止 stop。
     * @note       本函数无输入参数。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void stop();
    /**
     * @brief      判断 playing相关的数据并返回处理结果。
     * @note       本函数无输入参数。
     * @return     成功返回 true；参数非法或执行失败时返回 false。
     * @author     JaTin
     */
    bool isPlaying() const;

    /**
     * @brief  当前正在播放的容器（UI 用它判断哪个气泡该显示暂停图标）。
     */
    QByteArray current() const { return current_; }

signals:
    /**
     * @brief      执行 playingchanged相关的数据并返回处理结果。
     * @param[in] playing        输入参数；类型 bool。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void playingChanged(bool playing);
    /**
     * @brief      执行 failed相关的数据并返回处理结果。
     * @param[in] reason         失败原因描述；类型 const QString&。
     * @return     无返回值；处理结果通过成员状态或信号对外体现。
     * @author     JaTin
     */
    void failed(const QString& reason);

private:
    /**
     * @brief      构造本对象，完成成员初始化与依赖注入。
     * @param[in] parent         Qt 父对象指针，用于对象树所有权管理；类型 QObject*。
     * @return     构造/析构函数无返回值。
     * @author     JaTin
     */
    explicit CVoicePlayer(QObject* parent = nullptr);
    /**
     * @brief      获取 wav 文件路径 for。
     * @param[in] container      容器；类型 const QByteArray&。
     * @return     返回构造好的字符串；无内容时返回空字符串。
     * @author     JaTin
     */
    QString wavPathFor(const QByteArray& container) const;

    /**
     * @brief  视觉效果。 初值 = nullptr。
     * @note   类型 QSoundEffect*；类/结构体成员变量。
     */
    QSoundEffect* m_pEffect = nullptr;
    /**
     * @brief  current。
     * @note   类型 QByteArray；类/结构体成员变量。
     */
    QByteArray    current_;
    /**
     * @brief  speaking。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool          speaking_ = false;
    /**
     * @brief  PendingPlay。 初值 = false。
     * @note   类型 bool；类/结构体成员变量。
     */
    bool          m_nPendingPlay = false;
};
