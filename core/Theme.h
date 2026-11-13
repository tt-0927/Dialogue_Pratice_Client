/*
 * 文件名:    Theme.h
 * 创建日期:  2026-11-13
 * 作者:      JaTin
 * 功能描述:  Theme 头文件，声明相关类型与接口。
 * 修改记录:  2026-11-13 JaTin 按 C/C++ 编码规范整改注释与命名。
 */

/* Theme.h */

/* Telegram 风格的浅色主题：调色板 + 全局样式表。 */
/* 所有颜色集中在此处，避免样式散落在各个页面里。 */
#pragma once

#include <QColor>
#include <QString>

namespace theme {

/* ---- 调色板 ---- */
inline const QColor accent        ("#3390EC");
inline const QColor accentHover   ("#2A7FD6");
inline const QColor accentPressed ("#246FC0");
inline const QColor textPrimary   ("#101828");
inline const QColor textSecondary ("#6B7785");
inline const QColor border        ("#E3E8EF");
inline const QColor surface       ("#FFFFFF");
inline const QColor appBg         ("#F5F7FA");
inline const QColor chatBg        ("#E9EEF3");
inline const QColor bubbleOut     ("#EEFFDE");
inline const QColor bubbleIn      ("#FFFFFF");
inline const QColor danger        ("#E5484D");
inline const QColor success       ("#22C55E");

/* Telegram 头像配色（按 userId 取模，保证同一用户颜色稳定）。 */
inline QColor avatarColorFor(qint64 id) {
    static const char* palette[] = {
        "#E17076", "#FAA774", "#A695E7", "#7BC862",
        "#6EC9CB", "#65AADD", "#EE7AAE", "#F2A03D",
    };
    const int n = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    qint64 v = id < 0 ? -id : id;
    return QColor(palette[v % n]);
}

inline QString styleSheet() {
    return QStringLiteral(R"(
* {
    font-family: "Noto Sans CJK SC", "Noto Sans", "Microsoft YaHei", sans-serif;
    font-size: 10pt;
    color: #101828;
}

#AppRoot   { background: #F5F7FA; }
#Card      { background: #FFFFFF; border: 1px solid #E3E8EF; border-radius: 16px; }
#TopBar    { background: #FFFFFF; border-bottom: 1px solid #E3E8EF; }

/* 对话框必须显式给背景：QDialog 是顶层窗口，默认不绘制 QSS 背景，
   漏掉这条会渲染成一块纯黑（成员列表对话框踩过）。 */
QDialog { background: #FFFFFF; }

#AppTitle  { font-size: 15pt; font-weight: 700; }
#BigTitle  { font-size: 20pt; font-weight: 700; }
#SubTitle  { color: #6B7785; font-size: 9.5pt; }
#SectionTitle { font-size: 11pt; font-weight: 700; }
#KeyLabel  { color: #6B7785; font-size: 9pt; }
#ValueLabel { font-size: 10pt; font-weight: 600; }
#HintLabel { color: #8A94A6; font-size: 9pt; }
#ErrorText { color: #E5484D; font-size: 9pt; }
#OkText    { color: #22C55E; font-size: 9pt; }

#InfoBox   { background: #F7F9FC; border: 1px solid #EDF1F6; border-radius: 12px; }

QLineEdit#Field, QSpinBox#Field, QPlainTextEdit#Field {
    background: #FFFFFF;
    border: 1px solid #E3E8EF;
    border-radius: 10px;
    padding: 10px 12px;
    selection-background-color: #3390EC;
    selection-color: #FFFFFF;
}
QLineEdit#Field:focus, QSpinBox#Field:focus, QPlainTextEdit#Field:focus {
    border: 1px solid #3390EC;
}

#PrimaryButton {
    background: #3390EC;
    color: #FFFFFF;
    border: none;
    border-radius: 10px;
    padding: 11px 18px;
    font-weight: 600;
}
#PrimaryButton:hover   { background: #2A7FD6; }
#PrimaryButton:pressed { background: #246FC0; }
#PrimaryButton:disabled { background: #A9CBF2; }

#GhostButton {
    background: #F2F5F9;
    color: #101828;
    border: 1px solid #E3E8EF;
    border-radius: 10px;
    padding: 9px 16px;
    font-weight: 600;
}
#GhostButton:hover   { background: #E8EEF6; }
#GhostButton:pressed { background: #DDE6F0; }

#LinkButton {
    background: transparent;
    border: none;
    color: #3390EC;
    font-weight: 600;
    padding: 4px 6px;
}
#LinkButton:hover { color: #2A7FD6; }

#Divider { background: #EDF1F6; max-height: 1px; min-height: 1px; border: none; }

/* ---- 聊天页 ---- */
#ChatHeader { background: #FFFFFF; border-bottom: 1px solid #E3E8EF; }
#ChatScroll { background: #E9EEF3; border: none; }
#ChatCanvas { background: #E9EEF3; }
#Composer   { background: #FFFFFF; border-top: 1px solid #E3E8EF; }
#ComposerEdit {
    background: #F2F5F9;
    border: 1px solid #E3E8EF;
    border-radius: 19px;
    padding: 8px 14px;
    font-size: 10pt;
}
#ComposerEdit:focus { border: 1px solid #C9D6E4; }

#BubbleIn  { background: #FFFFFF; border: 1px solid #E6EBF1; border-radius: 14px; }
#BubbleOut { background: #EEFFDE; border: 1px solid #DDF2C9; border-radius: 14px; }
#BubbleDivider { background: #E4EAF1; border: none; max-height: 1px; min-height: 1px; margin-top: 4px; }
#BubbleTranslation { color: #4A5568; font-size: 9pt; background: transparent; }
#BubbleTranscript { color: #2D3A4A; font-size: 9.5pt; background: transparent; }
#CTranslationCard { background: #F7FAFD; border: 1px solid #DCE6F2; border-radius: 10px; }
#CSuggestionBar  { background: #F7FAFD; border: 1px solid #DCE6F2; border-radius: 10px; }
#CSuggestionChip { background: #FFFFFF; border: 1px solid #D8E3F0; border-radius: 8px; }
#CSuggestionChip:hover { background: #EAF2FB; border-color: #B9D4F2; }
#SuggestionEn { color: #1F2A37; font-size: 10pt; background: transparent; border: none; }
#SuggestionZh { color: #7A8797; font-size: 9pt; background: transparent; border: none; }
#BubbleText { background: transparent; font-size: 10pt; }
#TimeLabel { color: #8A94A6; font-size: 8pt; background: transparent; }
#NameLabel { font-size: 9pt; font-weight: 700; background: transparent; }

#SystemPill {
    background: #FFFFFF;
    border: 1px solid #DCE4EE;
    border-radius: 11px;
    padding: 4px 12px;
    color: #5B6675;
    font-size: 9pt;
}
#DatePill {
    background: #E1E9F2;
    border-radius: 11px;
    padding: 4px 14px;
    color: #5B6675;
    font-size: 9pt;
    font-weight: 600;
}

#MemberPanel { background: #FFFFFF; border-left: 1px solid #E3E8EF; }
#MemberRow   { border-radius: 8px; background: transparent; }
#MemberRow:hover { background: #F2F5F9; }

#Banner {
    background: #101828;
    color: #FFFFFF;
    border-radius: 10px;
    padding: 10px 18px;
    font-size: 9.5pt;
    font-weight: 600;
}
#BannerError { background: #E5484D; color: #FFFFFF; border-radius: 10px; padding: 10px 18px; font-size: 9.5pt; font-weight: 600; }

QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: #C6D1DE;
    border-radius: 4px;
    min-height: 32px;
}
QScrollBar::handle:vertical:hover { background: #AEBDCD; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }
)");
}

/* namespace theme */
}
