// Avatar.h
//
// 圆形文字头像：取名字首字符，背景色按 userId 稳定映射（Telegram 风格）。
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

class Avatar : public QWidget {
    Q_OBJECT

public:
    explicit Avatar(QWidget* parent = nullptr);

    void setText(const QString& text);
    void setColor(const QColor& color);
    void setFontPx(int px);

    static QColor colorForId(qint64 id);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString text_;
    QColor  color_ = QColor("#3390EC");
    int     fontPx_ = 16;
};
