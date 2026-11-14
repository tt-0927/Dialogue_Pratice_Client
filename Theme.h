/**
 * @file       Theme.h
 * @brief      Theme 头文件，声明相关类型与接口。
 * @author     JaTin
 * @date       2026-11-13
 * @note       修改记录：2026-11-14 JaTin 统一为 Doxygen 风格注释。
 */

/**
 * @brief  Theme.h。
 */

/**
 * @brief  Telegram 风格的浅色主题：调色板 + 全局样式表。 所有颜色集中在此处，避免样式散落在各个页面里。
 */
#pragma once

#include <QColor>
#include <QString>

namespace theme {

/**
 * @brief      ---- 调色板 ----。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor accent        ("#3390EC");
/**
 * @brief      获取 accenthover相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor accentHover   ("#2A7FD6");
/**
 * @brief      获取 accentpressed相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor accentPressed ("#246FC0");
/**
 * @brief      获取文本 primary。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor textPrimary   ("#101828");
/**
 * @brief      获取文本 secondary。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor textSecondary ("#6B7785");
/**
 * @brief      获取 border相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor border        ("#E3E8EF");
/**
 * @brief      获取 surface相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor surface       ("#FFFFFF");
/**
 * @brief      获取 appbg相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor appBg         ("#F5F7FA");
/**
 * @brief      获取 chatbg相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor chatBg        ("#E9EEF3");
/**
 * @brief      获取消息气泡 out。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor bubbleOut     ("#EEFFDE");
/**
 * @brief      获取消息气泡 in。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor bubbleIn      ("#FFFFFF");
/**
 * @brief      获取 danger相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor danger        ("#E5484D");
/**
 * @brief      获取 success相关的数据并返回处理结果。
 * @note       本函数无输入参数。
 * @return     返回 const QColor 类型的处理结果。
 * @author     JaTin
 */
inline const QColor success       ("#22C55E");

/**
 * @brief      Telegram 头像配色（按 userId 取模，保证同一用户颜色稳定）。
 * @param[in] id             编号；类型 qint64。
 * @return     返回 inline QColor 类型的处理结果。
 * @author     JaTin
 */
inline QColor avatarColorFor(qint64 id) {
    static const char* palette[] = {
        "#E17076", "#FAA774", "#A695E7", "#7BC862",
        "#6EC9CB", "#65AADD", "#EE7AAE", "#F2A03D",
    };
    const int n = static_cast<int>(sizeof(palette) / sizeof(palette[0]));
    qint64 v = id < 0 ? -id : id;
    /**
     * @brief      获取颜色。
     * @note       本函数无输入参数。
     * @return     返回 return 类型的处理结果。
     * @author     JaTin
     */
    return QColor(palette[v % n]);
}

/**
 * @brief      获取样式 sheet。
 * @note       本函数无输入参数。
 * @return     返回构造好的字符串；无内容时返回空字符串。
 * @author     JaTin
 */
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

/**
 * @brief  ---- 聊天页 ----。
 */
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

/**
 * @brief  namespace theme。
 */
}
