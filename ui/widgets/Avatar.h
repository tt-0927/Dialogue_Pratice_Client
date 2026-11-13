/*
 * 文件名:    Avatar.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  CAvatar 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* CAvatar.h */

/* 圆形文字头像：取名字首字符，背景色按 userId 稳定映射（Telegram 风格）。 */
#pragma once

#include <QColor>
#include <QString>
#include <QWidget>

class CAvatar : public QWidget {
    Q_OBJECT

public:
    explicit CAvatar(QWidget* parent = nullptr);

    void setText(const QString& text);
    void setColor(const QColor& color);
    void setFontPx(int px);

    static QColor colorForId(qint64 id);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QString m_strText;
    QColor  m_stColor = QColor("#3390EC");
    int     m_nFontPx = 16;
};
