// TtsService.cpp
#include "TtsService.h"

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
constexpr qint64 kWavHeaderSize = 44;   // 小于这个值说明是半成品
}

TtsService::TtsService(QObject* parent) : QObject(parent) {
    effect_ = new QSoundEffect(this);
    effect_->setVolume(1.0);

    connect(effect_, &QSoundEffect::statusChanged, this, [this] {
        if (effect_->status() == QSoundEffect::Error) {
            pendingPlay_ = false;
            emit failed(QStringLiteral("音频加载失败"));
            return;
        }
        // setSource 是异步加载，加载完才能 play
        if (pendingPlay_ && effect_->status() == QSoundEffect::Ready) {
            pendingPlay_ = false;
            effect_->play();
        }
    });

    connect(effect_, &QSoundEffect::playingChanged, this, [this] {
        const bool playing = effect_->isPlaying();
        if (speaking_ != playing) {
            speaking_ = playing;
            emit speakingChanged(playing);
        }
    });
}

TtsService::~TtsService() {
    if (process_ != nullptr) {
        process_->kill();
        process_->waitForFinished(1000);
    }
    cleanupPartFile();
}

void TtsService::setPaths(const QString& piperBin, const QString& voiceModel) {
    piperBin_ = piperBin;
    voiceModel_ = voiceModel;
}

bool TtsService::isAvailable() const {
    return !piperBin_.isEmpty() && QFileInfo::exists(piperBin_)
           && QFileInfo::exists(voiceModel_);
}

QString TtsService::cachePathFor(const QString& text) const {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + QStringLiteral("/tts");
    QDir().mkpath(dir);

    const QByteArray hash =
        QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Md5).toHex();
    return dir + QLatin1Char('/') + QString::fromLatin1(hash) + QStringLiteral(".wav");
}

void TtsService::speak(const QString& text) {
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) return;

    if (!isAvailable()) {
        emit failed(QStringLiteral("未安装 Piper（运行 scripts/setup_tts.sh）"));
        return;
    }

    stop();   // 停掉上一段，也清掉正在合成的半成品

    currentText_ = trimmed;

    const auto hit = cache_.constFind(trimmed);
    if (hit != cache_.constEnd() && QFileInfo::exists(hit.value())) {
        playFile(hit.value());
        return;
    }

    const QString wav = cachePathFor(trimmed);
    // 大小要过 WAV 头：历史版本可能留下被 kill 截断的半成品文件
    if (QFileInfo::exists(wav) && QFileInfo(wav).size() > kWavHeaderSize) {
        cache_.insert(trimmed, wav);
        playFile(wav);
        return;
    }

    pendingPart_ = wav + QStringLiteral(".part");
    pendingFinal_ = wav;
    pendingText_ = trimmed;

    if (process_ == nullptr) {
        process_ = new QProcess(this);
        // piper 需要同目录下的 espeak-ng-data，工作目录设成二进制所在目录
        process_->setWorkingDirectory(QFileInfo(piperBin_).absolutePath());

        connect(process_, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
            // 被 stop() 主动打断：pendingPart_ 已被清空，不报错
            if (pendingPart_.isEmpty()) return;

            if (status != QProcess::NormalExit || code != 0) {
                cleanupPartFile();
                emit failed(QStringLiteral("语音合成失败（piper 退出码 %1）").arg(code));
                return;
            }
            if (!QFileInfo::exists(pendingPart_)
                || QFileInfo(pendingPart_).size() <= kWavHeaderSize) {
                cleanupPartFile();
                emit failed(QStringLiteral("语音合成没有生成有效文件"));
                return;
            }

            // 原子替换：先写 .part，成功后才改名成正式缓存。
            // 否则合成中途被打断会留下半个 WAV，下次被当成缓存命中，播出来只有半秒。
            QFile::remove(pendingFinal_);
            if (!QFile::rename(pendingPart_, pendingFinal_)) {
                cleanupPartFile();
                emit failed(QStringLiteral("无法写入语音缓存"));
                return;
            }

            const QString wav = pendingFinal_;
            cache_.insert(pendingText_, wav);
            pendingPart_.clear();
            pendingFinal_.clear();
            playFile(wav);
        });
    }

    process_->start(piperBin_, {QStringLiteral("-m"), voiceModel_,
                                QStringLiteral("-f"), pendingPart_,
                                QStringLiteral("--length-scale"), QStringLiteral("1.05")});
    if (!process_->waitForStarted(3000)) {
        cleanupPartFile();
        emit failed(QStringLiteral("无法启动 piper"));
        return;
    }
    process_->write(trimmed.toUtf8());
    process_->closeWriteChannel();
}

void TtsService::playFile(const QString& path) {
    if (loadedPath_ != path) {
        // 换文件：setSource 是异步的，等 Ready 再播
        loadedPath_ = path;
        pendingPlay_ = true;
        effect_->setSource(QUrl::fromLocalFile(path));
        if (effect_->status() == QSoundEffect::Ready) {
            pendingPlay_ = false;
            effect_->play();
        }
        return;
    }

    // 同一个文件再点：play() 在播放中不会重头，必须先 stop
    effect_->stop();
    effect_->play();
}

void TtsService::stop() {
    pendingPlay_ = false;
    if (effect_ != nullptr) effect_->stop();

    // 先清空 pendingPart_ 再 kill，让 finished 回调知道这是主动取消
    const QString part = pendingPart_;
    pendingPart_.clear();
    pendingFinal_.clear();

    if (process_ != nullptr && process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(500);
    }
    if (!part.isEmpty()) QFile::remove(part);
}

void TtsService::cleanupPartFile() {
    if (!pendingPart_.isEmpty()) {
        QFile::remove(pendingPart_);
        pendingPart_.clear();
    }
}
