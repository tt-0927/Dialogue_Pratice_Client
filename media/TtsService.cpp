/*
 * 文件名:    TtsService.cpp
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CTtsService 实现文件，包含相关接口的具体实现。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CTtsService.cpp */
#include "media/TtsService.h"

#ifdef DP_WITH_TTS

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSoundEffect>
#include <QStandardPaths>
#include <QUrl>
#include <QtDebug>

namespace {
/* 小于这个值说明是半成品 */
constexpr qint64 kWavHeaderSize = 44;
}

/*
 * 功能描述:  执行 CTtsService 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  parent  参数。
 * return:    返回值，含义见功能描述。
 */
CTtsService::CTtsService(QObject* parent) : QObject(parent) {
    /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
    m_pEffect = new QSoundEffect(this);
    m_pEffect->setVolume(1.0);

    connect(m_pEffect, &QSoundEffect::statusChanged, this, [this] {
        if (m_pEffect->status() == QSoundEffect::Error) {
            m_nPendingPlay = false;
            emit failed(QStringLiteral("音频加载失败"));
            return;
        }
        /* setSource 是异步加载，加载完才能 play */
        if (m_nPendingPlay && m_pEffect->status() == QSoundEffect::Ready) {
            m_nPendingPlay = false;
            m_pEffect->play();
        }
    });

    connect(m_pEffect, &QSoundEffect::playingChanged, this, [this] {
        const bool playing = m_pEffect->isPlaying();
        if (speaking_ != playing) {
            speaking_ = playing;
            emit speakingChanged(playing);
        }
    });
}

/*
 * 功能描述:  执行 CTtsService 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
CTtsService::~CTtsService() {
    if (m_pProcess != nullptr) {
        m_pProcess->kill();
        m_pProcess->waitForFinished(1000);
    }
    cleanupPartFile();
}

/*
 * 功能描述:  执行 setPaths 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  piperBin  参数。
 * param [in]  voiceModel  参数。
 * return:    void
 */
void CTtsService::setPaths(const QString& piperBin, const QString& voiceModel) {
    m_strPiperBin = piperBin;
    m_strVoiceModel = voiceModel;
}

/*
 * 功能描述:  执行 isAvailable 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    返回值，含义见功能描述。
 */
bool CTtsService::isAvailable() const {
    return !m_strPiperBin.isEmpty() && QFileInfo::exists(m_strPiperBin)
           && QFileInfo::exists(m_strVoiceModel);
}

/*
 * 功能描述:  执行 cachePathFor 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    返回值，含义见功能描述。
 */
QString CTtsService::cachePathFor(const QString& text) const {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tts");
    QDir().mkpath(dir);

    const QByteArray hash =
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Md5).toHex();
    return dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".wav");
}

/*
 * 功能描述:  执行 speak 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  text  参数。
 * return:    void
 */
void CTtsService::speak(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return;

    if (!isAvailable()) {
        emit failed(QStringLiteral("未安装 Piper（运行 scripts/setup_tts.sh）"));
        return;
    }

    /* 停掉上一段，也清掉正在合成的半成品 */
    stop();

    m_strCurrentText = trimmed;

    const auto hit = m_strCache.constFind(trimmed);
    if (hit != m_strCache.constEnd() && QFileInfo::exists(hit.value())) {
        playFile(hit.value());
        return;
    }

    const QString wav = cachePathFor(trimmed);
    /* 大小要过 WAV 头：历史版本可能留下被 kill 截断的半成品文件 */
    if (QFileInfo::exists(wav) && QFileInfo(wav).size() > kWavHeaderSize) {
        m_strCache.insert(trimmed, wav);
        playFile(wav);
        return;
    }

    m_strPendingPart = wav + QStringLiteral(".part");
    m_strPendingFinal = wav;
    m_strPendingText = trimmed;

    if (m_pProcess == nullptr) {
        /* 特例：Qt 父子对象所有权，控件由父对象自动回收；若改用智能指针会造成双重释放，故按规范保留 new。 */
        m_pProcess = new QProcess(this);
        /* piper 需要同目录下的 espeak-ng-data，工作目录设成二进制所在目录 */
        m_pProcess->setWorkingDirectory(QFileInfo(m_strPiperBin).absolutePath());

        connect(m_pProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
            /* 被 stop() 主动打断：pendingPart_ 已被清空，不报错 */
            if (m_strPendingPart.isEmpty()) return;

            if (status != QProcess::NormalExit || code != 0) {
                cleanupPartFile();
                emit failed(QStringLiteral("语音合成失败（piper 退出码 %1）").arg(code));
                return;
            }
            if (!QFileInfo::exists(m_strPendingPart)
                || QFileInfo(m_strPendingPart).size() <= kWavHeaderSize) {
                cleanupPartFile();
                emit failed(QStringLiteral("语音合成没有生成有效文件"));
                return;
            }

            /* 原子替换：先写 .part，成功后才改名成正式缓存。 */
            /* 否则合成中途被打断会留下半个 WAV，下次被当成缓存命中，播出来只有半秒。 */
            QFile::remove(m_strPendingFinal);
            if (!QFile::rename(m_strPendingPart, m_strPendingFinal)) {
                cleanupPartFile();
                emit failed(QStringLiteral("无法写入语音缓存"));
                return;
            }

            const QString wav = m_strPendingFinal;
            m_strCache.insert(m_strPendingText, wav);
            m_strPendingPart.clear();
            m_strPendingFinal.clear();
            playFile(wav);
        });
    }

    m_pProcess->start(m_strPiperBin, {QStringLiteral("-m"), m_strVoiceModel,
                                QStringLiteral("-f"), m_strPendingPart,
                                QStringLiteral("--length-scale"), QStringLiteral("1.05")});
    if (!m_pProcess->waitForStarted(3000)) {
        cleanupPartFile();
        emit failed(QStringLiteral("无法启动 piper"));
        return;
    }
    m_pProcess->write(trimmed.toUtf8());
    m_pProcess->closeWriteChannel();
}

/*
 * 功能描述:  执行 playFile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  path  参数。
 * return:    void
 */
void CTtsService::playFile(const QString& path) {
    if (m_strLoadedPath != path) {
        /* 换文件：setSource 是异步的，等 Ready 再播 */
        m_strLoadedPath = path;
        m_nPendingPlay = true;
        m_pEffect->setSource(QUrl::fromLocalFile(path));
        if (m_pEffect->status() == QSoundEffect::Ready) {
            m_nPendingPlay = false;
            m_pEffect->play();
        }
        return;
    }

    /* 同一个文件再点：play() 在播放中不会重头，必须先 stop */
    m_pEffect->stop();
    m_pEffect->play();
}

/*
 * 功能描述:  执行 stop 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CTtsService::stop() {
    m_nPendingPlay = false;
    if (m_pEffect != nullptr) m_pEffect->stop();

    /* 先清空 pendingPart_ 再 kill，让 finished 回调知道这是主动取消 */
    const QString part = m_strPendingPart;
    m_strPendingPart.clear();
    m_strPendingFinal.clear();

    if (m_pProcess != nullptr && m_pProcess->state() != QProcess::NotRunning) {
        m_pProcess->kill();
        m_pProcess->waitForFinished(500);
    }
    if (!part.isEmpty()) QFile::remove(part);
}

/*
 * 功能描述:  执行 cleanupPartFile 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * return:    void
 */
void CTtsService::cleanupPartFile() {
    if (!m_strPendingPart.isEmpty()) {
        QFile::remove(m_strPendingPart);
        m_strPendingPart.clear();
    }
}

/* !DP_WITH_TTS */
#else

/* 本平台未编译 Piper TTS（Android 交叉编译用 -DDP_WITH_TTS=OFF）。 */
/* isAvailable() 恒为 false，UI 自动隐藏朗读按钮；接口签名保持一致， */
/* 调用方（ChatPage / CMessageBubble）无需任何改动。 */

CTtsService::CTtsService(QObject* parent) : QObject(parent) {}

CTtsService::~CTtsService() = default;

/*
 * 功能描述:  执行 setPaths 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  piperBin  参数。
 * param [in]  voiceModel  参数。
 * return:    void
 */
void CTtsService::setPaths(const QString& piperBin, const QString& voiceModel) {
    m_strPiperBin = piperBin;
    m_strVoiceModel = voiceModel;
}

bool CTtsService::isAvailable() const { return false; }

/*
 * 功能描述:  执行 speak 相关的处理，完成参数校验、核心逻辑与结果返回。
 * 作者:      JaTin
 * param [in]  QString  参数。
 * return:    void
 */
void CTtsService::speak(const QString&) {
    emit failed(QStringLiteral("本平台未编译语音合成（Piper）"));
}

void CTtsService::stop() {}

void CTtsService::playFile(const QString&) {}

QString CTtsService::cachePathFor(const QString&) const { return QString(); }

void CTtsService::cleanupPartFile() {}

/* DP_WITH_TTS */
#endif
