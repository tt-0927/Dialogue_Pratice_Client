/*
 * 文件名:    TranslationCard.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CTranslationCard 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CTranslationCard.h */

/* 输入框上方的译文卡：把用户输入的中文翻成英文，可试听、可一键填入输入框。 */
#pragma once

#include <QFrame>
#include <QString>

class CPlayButton;
class QLabel;
class QPushButton;

class CTranslationCard : public QFrame {
    Q_OBJECT

public:
    explicit CTranslationCard(QWidget* parent = nullptr);

    void showPending();
    void showTranslation(const QString& english);
    void hideCard();
    void setPlaying(bool playing);

    QString currentText() const { return current_; }

signals:
    void playRequested(const QString& text);
    void useRequested(const QString& text);

private:
    QLabel*      m_pTitle = nullptr;
    QLabel*      m_pBody = nullptr;
    CPlayButton*  m_pPlay = nullptr;
    QPushButton* m_pUse = nullptr;
    QString      current_;
};
