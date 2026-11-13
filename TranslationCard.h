// TranslationCard.h
//
// 输入框上方的译文卡：把用户输入的中文翻成英文，可试听、可一键填入输入框。
#pragma once

#include <QFrame>
#include <QString>

class PlayButton;
class QLabel;
class QPushButton;

class TranslationCard : public QFrame {
    Q_OBJECT

public:
    explicit TranslationCard(QWidget* parent = nullptr);

    void showPending();
    void showTranslation(const QString& english);
    void hideCard();
    void setPlaying(bool playing);

    QString currentText() const { return current_; }

signals:
    void playRequested(const QString& text);
    void useRequested(const QString& text);

private:
    QLabel*      title_ = nullptr;
    QLabel*      body_ = nullptr;
    PlayButton*  play_ = nullptr;
    QPushButton* use_ = nullptr;
    QString      current_;
};
