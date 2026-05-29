#pragma once

#include <QColor>
#include <QPalette>
#include <QString>

namespace UiTheme
{
    enum class ThemeMode { System, Light, Dark };

    struct Tokens {
        const char* Primary;
        const char* PrimaryHover;
        const char* PrimarySoft;
        const char* PrimarySoftBorder;
        const char* PrimaryText;

        const char* Text;
        const char* Subtext;
        const char* SubtextMid;
        const char* TextDark;
        const char* TextMid;
        const char* TextMuted;

        const char* AppBg;
        const char* Surface;
        const char* SurfaceAlt;
        const char* White;
        const char* Border;
        const char* BorderInput;
        const char* InputBg;
        const char* HoverSurface;

        const char* SidebarBg;
        const char* SidebarBgDeep;
        const char* SidebarBorder;
        const char* SidebarText;
        const char* SidebarMuted;

        const char* Success;
        const char* Warning;
        const char* Danger;
        const char* DangerAction;
        const char* DangerHover;
        const char* DangerText;

        const char* WarningSoft;
        const char* WarningBorder;
        const char* WarningText;
        const char* DangerSoft;
        const char* DangerBorder;
        const char* DangerBadgeText;
        const char* NeutralSoft;

        const char* ChartNormal;
        const char* ChartRisk;
        const char* ChartThreshold;
        const char* ChartGrid;
        const char* ChartAxis;
        const char* ChartEmpty;
        const char* ChartBarBg;

        const char* TerminalBg;
        const char* TerminalText;
    };

    // Light-token constants kept for existing UiTheme:: references.
    constexpr const char* Primary      = "#0B73F6";
    constexpr const char* PrimaryHover = "#0757C9";
    constexpr const char* PrimarySoft  = "#EAF2FF";
    constexpr const char* PrimarySoftBorder = "#B7D4FF";
    constexpr const char* PrimaryText = "#084A9E";

    constexpr const char* Text         = "#1D293D";
    constexpr const char* Subtext      = "#62748E";
    constexpr const char* SubtextMid   = "#45556C";
    constexpr const char* TextDark     = "#0F172B";
    constexpr const char* TextMid      = "#314158";
    constexpr const char* TextMuted    = "#90A1B9";

    constexpr const char* AppBg        = "#F6F8FB";
    constexpr const char* Surface      = "#F8FAFC";
    constexpr const char* SurfaceAlt   = "#F1F5F9";
    constexpr const char* White        = "#FFFFFF";
    constexpr const char* Border       = "#E2E8F0";
    constexpr const char* BorderInput  = "#CAD5E2";

    constexpr const char* SidebarBg     = "#061B49";
    constexpr const char* SidebarBgDeep = "#03112E";
    constexpr const char* SidebarBorder = "#12366D";
    constexpr const char* SidebarText   = "#D7E5FF";
    constexpr const char* SidebarMuted  = "#8FA6C7";

    constexpr const char* Success      = "#00BC7D";
    constexpr const char* Warning      = "#F97316";
    constexpr const char* Danger       = "#FF2056";
    constexpr const char* DangerAction = "#EC003F";
    constexpr const char* DangerHover  = "#C5002F";
    constexpr const char* DangerText   = "#EF4444";

    constexpr const char* WarningSoft   = "#FFFAEB";
    constexpr const char* WarningBorder = "#FEDF89";
    constexpr const char* WarningText   = "#B26A00";

    constexpr const char* ChartNormal    = "#B7D4FF";
    constexpr const char* ChartRisk      = "#0757C9";
    constexpr const char* ChartThreshold = "#0B73F6";

    constexpr const char* TerminalBg   = "#0F172B";
    constexpr const char* TerminalText = "#C8D8F0";

    inline const Tokens& lightTokens()
    {
        static const Tokens t {
            Primary, PrimaryHover, PrimarySoft, PrimarySoftBorder, PrimaryText,
            Text, Subtext, SubtextMid, TextDark, TextMid, TextMuted,
            AppBg, Surface, SurfaceAlt, White, Border, BorderInput, "#FFFFFF", SurfaceAlt,
            SidebarBg, SidebarBgDeep, SidebarBorder, SidebarText, SidebarMuted,
            Success, Warning, Danger, DangerAction, DangerHover, DangerText,
            WarningSoft, WarningBorder, WarningText, "#FEF3F2", "#FECACA", "#E34B3F", "#FFFFFF",
            ChartNormal, ChartRisk, ChartThreshold, "#E2E8F0", "#94A3B8", "#94A3B8", PrimarySoft,
            TerminalBg, TerminalText
        };
        return t;
    }

    inline const Tokens& darkTokens()
    {
        static const Tokens t {
            "#1683FF", "#49A3FF", "#102F56", "#235D9C", "#9CCBFF",
            "#F3F7FF", "#9FB2CC", "#B8C7DB", "#FFFFFF", "#DCE8F8", "#7F93AE",
            "#07111F", "#0D1828", "#111F33", "#0D1828", "#26364D", "#38506D", "#0B1626", "#162842",
            "#05152D", "#030A18", "#173456", "#D7E5FF", "#89A2C4",
            "#1FD18E", "#F59E0B", "#FF4D73", "#FF3B6B", "#D7194C", "#FF6B86",
            "#332611", "#7A5418", "#F5B95F", "#321421", "#86324C", "#FF8EA6", "#172336",
            "#325E9A", "#48A0FF", "#5EB1FF", "#20364F", "#7890AE", "#8EA4C0", "#173456",
            "#050B14", "#D5E4F7"
        };
        return t;
    }

    inline ThemeMode& activeModeRef()
    {
        static ThemeMode mode = ThemeMode::Light;
        return mode;
    }

    inline ThemeMode resolvedMode(ThemeMode mode)
    {
        return mode == ThemeMode::Dark ? ThemeMode::Dark : ThemeMode::Light;
    }

    inline void setActiveMode(ThemeMode mode)
    {
        activeModeRef() = resolvedMode(mode);
    }

    inline ThemeMode activeMode()
    {
        return activeModeRef();
    }

    inline bool isDark()
    {
        return activeMode() == ThemeMode::Dark;
    }

    inline const Tokens& tokens(ThemeMode mode)
    {
        return resolvedMode(mode) == ThemeMode::Dark ? darkTokens() : lightTokens();
    }

    inline const Tokens& tokens()
    {
        return tokens(activeMode());
    }

    inline QColor color(const char* value)
    {
        return QColor(QString::fromLatin1(value));
    }

    inline QString themeModeKey(ThemeMode mode)
    {
        switch (mode) {
        case ThemeMode::Dark: return QStringLiteral("dark");
        case ThemeMode::Light: return QStringLiteral("light");
        case ThemeMode::System:
        default: return QStringLiteral("system");
        }
    }

    inline ThemeMode themeModeFromKey(const QString& key)
    {
        const QString v = key.trimmed().toLower();
        if (v == QStringLiteral("dark")) return ThemeMode::Dark;
        if (v == QStringLiteral("light")) return ThemeMode::Light;
        return ThemeMode::System;
    }

    inline QString replaceTokens(QString qss, const Tokens& t)
    {
        return qss
            .replace(QStringLiteral("%Primary%"), QLatin1String(t.Primary))
            .replace(QStringLiteral("%PrimaryHover%"), QLatin1String(t.PrimaryHover))
            .replace(QStringLiteral("%PrimarySoft%"), QLatin1String(t.PrimarySoft))
            .replace(QStringLiteral("%PrimarySoftBorder%"), QLatin1String(t.PrimarySoftBorder))
            .replace(QStringLiteral("%PrimaryText%"), QLatin1String(t.PrimaryText))
            .replace(QStringLiteral("%SidebarBg%"), QLatin1String(t.SidebarBg))
            .replace(QStringLiteral("%SidebarBgDeep%"), QLatin1String(t.SidebarBgDeep))
            .replace(QStringLiteral("%SidebarBorder%"), QLatin1String(t.SidebarBorder))
            .replace(QStringLiteral("%SidebarText%"), QLatin1String(t.SidebarText))
            .replace(QStringLiteral("%SidebarMuted%"), QLatin1String(t.SidebarMuted))
            .replace(QStringLiteral("%Text%"), QLatin1String(t.Text))
            .replace(QStringLiteral("%Subtext%"), QLatin1String(t.Subtext))
            .replace(QStringLiteral("%SubtextMid%"), QLatin1String(t.SubtextMid))
            .replace(QStringLiteral("%TextDark%"), QLatin1String(t.TextDark))
            .replace(QStringLiteral("%TextMid%"), QLatin1String(t.TextMid))
            .replace(QStringLiteral("%TextMuted%"), QLatin1String(t.TextMuted))
            .replace(QStringLiteral("%AppBg%"), QLatin1String(t.AppBg))
            .replace(QStringLiteral("%Surface%"), QLatin1String(t.Surface))
            .replace(QStringLiteral("%SurfaceAlt%"), QLatin1String(t.SurfaceAlt))
            .replace(QStringLiteral("%White%"), QLatin1String(t.White))
            .replace(QStringLiteral("%Border%"), QLatin1String(t.Border))
            .replace(QStringLiteral("%BorderInput%"), QLatin1String(t.BorderInput))
            .replace(QStringLiteral("%InputBg%"), QLatin1String(t.InputBg))
            .replace(QStringLiteral("%HoverSurface%"), QLatin1String(t.HoverSurface))
            .replace(QStringLiteral("%Success%"), QLatin1String(t.Success))
            .replace(QStringLiteral("%Warning%"), QLatin1String(t.Warning))
            .replace(QStringLiteral("%Danger%"), QLatin1String(t.Danger))
            .replace(QStringLiteral("%DangerAction%"), QLatin1String(t.DangerAction))
            .replace(QStringLiteral("%DangerHover%"), QLatin1String(t.DangerHover))
            .replace(QStringLiteral("%DangerText%"), QLatin1String(t.DangerText))
            .replace(QStringLiteral("%DangerSoft%"), QLatin1String(t.DangerSoft))
            .replace(QStringLiteral("%DangerBorder%"), QLatin1String(t.DangerBorder))
            .replace(QStringLiteral("%DangerBadgeText%"), QLatin1String(t.DangerBadgeText))
            .replace(QStringLiteral("%WarningSoft%"), QLatin1String(t.WarningSoft))
            .replace(QStringLiteral("%WarningBorder%"), QLatin1String(t.WarningBorder))
            .replace(QStringLiteral("%WarningText%"), QLatin1String(t.WarningText));
    }

    inline QPalette appPalette(ThemeMode mode)
    {
        const auto& t = tokens(mode);
        QPalette pal;
        pal.setColor(QPalette::Window, color(t.AppBg));
        pal.setColor(QPalette::WindowText, color(t.Text));
        pal.setColor(QPalette::Base, color(t.InputBg));
        pal.setColor(QPalette::AlternateBase, color(t.Surface));
        pal.setColor(QPalette::ToolTipBase, color(t.SurfaceAlt));
        pal.setColor(QPalette::ToolTipText, color(t.Text));
        pal.setColor(QPalette::Text, color(t.Text));
        pal.setColor(QPalette::Button, color(t.Surface));
        pal.setColor(QPalette::ButtonText, color(t.Text));
        pal.setColor(QPalette::BrightText, QColor(Qt::white));
        pal.setColor(QPalette::Highlight, color(t.Primary));
        pal.setColor(QPalette::HighlightedText, QColor(Qt::white));
        pal.setColor(QPalette::PlaceholderText, color(t.TextMuted));
        return pal;
    }

    inline QString appStyle(ThemeMode mode)
    {
        const auto& t = tokens(mode);
        QString qss;
        qss += QStringLiteral(R"(
            * {
                color: %Text%;
                selection-background-color: %Primary%;
                selection-color: #FFFFFF;
            }

            QMainWindow, QWidget#centralRoot, QStackedWidget, QStackedWidget > QWidget {
                background: %AppBg%;
            }

            QWidget#sidebar {
                background: qlineargradient(x1:0, y1:0, x2:0, y2:1,
                                            stop:0 %SidebarBg%, stop:1 %SidebarBgDeep%);
                border-right: 1px solid %SidebarBorder%;
            }

            QLabel#brandText { font-size: 22px; color: #FFFFFF; }

            QPushButton#navButton {
                min-height: 56px; padding: 0 20px 0 22px; border: none; border-radius: 8px;
                text-align: left; font-size: 16px; color: %SidebarText%; background: transparent;
            }
            QPushButton#navButton:hover { background: rgba(215, 229, 255, 0.10); color: #FFFFFF; }
            QPushButton#navButton[active="true"] {
                background: rgba(22, 131, 255, 0.18); color: #FFFFFF;
                border: 1px solid rgba(94, 177, 255, 0.36);
            }

            QPushButton#topNavButton {
                min-height: 64px; padding: 0 6px; border: none; border-radius: 0;
                background: transparent; color: %SubtextMid%; font-size: 14px;
            }
            QPushButton#topNavButton:hover { color: %Primary%; }
            QPushButton#topNavButton[active="true"] {
                color: %PrimaryHover%; font-weight: 600; border-bottom: 2px solid %PrimaryHover%;
            }

            QPushButton#iconButton {
                min-width: 34px; max-width: 34px; min-height: 34px; max-height: 34px;
                border-radius: 17px; border: 1px solid %Border%; background: %Surface%;
            }
            QPushButton#iconButton:hover { background: %HoverSurface%; }

            QWidget#topbar {
                background: %White%;
                border-bottom: 1px solid %Border%;
            }

            QLabel#pageTitle { font-size: 24px; color: %Text%; }
            QLabel#pageSubtitle { font-size: 14px; color: %Subtext%; }

            QFrame#card {
                background: %White%;
                border: 1px solid %Border%;
                border-radius: 9px;
            }
            QWidget#cardHeader { background: transparent; border-bottom: 1px solid %Border%; }
            QLabel#cardTitle { font-size: 14px; color: %TextMid%; }

            QFrame#statIconCircle {
                background: %SurfaceAlt%; border: 1px solid %Border%; border-radius: 23px;
            }
            QFrame#statusPanel {
                background: %White%; border: 1px solid %PrimarySoftBorder%;
                border-left: 4px solid %Primary%; border-radius: 6px;
            }
            QFrame#successAccent { background: %Success%; border: none; }
            QFrame#warningAccent { background: %Warning%; border: none; }
            QFrame#dangerAccent { background: %Danger%; border: none; }

            QLabel#statNumber { font-size: 24px; color: %TextDark%; }
            QLabel#mutedText { font-size: 12px; color: %TextMuted%; }
            QLabel#policyDangerText { font-size: 13px; color: %Primary%; }
            QLabel#policyValueText { font-size: 14px; color: %TextDark%; }
            QLabel#policySuccessText { font-size: 14px; color: %Primary%; }
            QLabel#protectionStatusTitle { font-size: 16px; color: %Text%; }
            QWidget#protectionList { background: %White%; border: none; }
            QWidget#protectionRow {
                background: %White%;
                border-bottom: 1px solid %Border%;
            }
            QWidget#protectionRow[last="true"] { border: none; }
            QScrollArea#protectionScrollArea {
                background: %White%;
                border: none;
            }
            QScrollArea#protectionScrollArea > QWidget > QWidget {
                background: %White%;
            }
            QScrollArea#protectionScrollArea QScrollBar:vertical {
                background: %White%; width: 6px; margin: 0;
            }
            QScrollArea#protectionScrollArea QScrollBar::handle:vertical {
                background: %PrimarySoftBorder%; border-radius: 3px; min-height: 24px;
            }
            QScrollArea#protectionScrollArea QScrollBar::add-line:vertical,
            QScrollArea#protectionScrollArea QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollArea#protectionScrollArea QScrollBar::add-page:vertical,
            QScrollArea#protectionScrollArea QScrollBar::sub-page:vertical {
                background: %White%;
            }
            QWidget#policySummaryLine { background: transparent; border: none; }
            QScrollArea#policySummaryScrollArea {
                background: %White%;
                border: none;
            }
            QScrollArea#policySummaryScrollArea > QWidget > QWidget {
                background: %White%;
            }
            QScrollArea#policySummaryScrollArea QScrollBar:vertical {
                background: %White%; width: 6px; margin: 0;
            }
            QScrollArea#policySummaryScrollArea QScrollBar::handle:vertical {
                background: %PrimarySoftBorder%; border-radius: 3px; min-height: 24px;
            }
            QScrollArea#policySummaryScrollArea QScrollBar::add-line:vertical,
            QScrollArea#policySummaryScrollArea QScrollBar::sub-line:vertical {
                height: 0;
            }
            QScrollArea#policySummaryScrollArea QScrollBar::add-page:vertical,
            QScrollArea#policySummaryScrollArea QScrollBar::sub-page:vertical {
                background: %White%;
            }

            QLabel#successBadge {
                padding: 0; border-radius: 6px; background: %PrimarySoft%;
                color: %PrimaryText%; border: 1px solid %PrimarySoftBorder%;
                font-size: 11px; font-weight: 700;
            }
            QLabel#protectionOnBadge {
                min-width: 48px; max-width: 48px; min-height: 28px; max-height: 28px;
                padding: 0; border-radius: 7px; background: %PrimarySoft%;
                color: %PrimaryText%; border: 1px solid %PrimarySoftBorder%; font-size: 11px;
            }
            QLabel#dangerBadge, QLabel#blockBadge {
                padding: 0; border-radius: 6px; background: %DangerSoft%;
                color: %DangerBadgeText%; border: 1px solid %DangerBorder%;
                font-size: 11px; font-weight: 700;
            }
            QLabel#warningBadge {
                padding: 0; border-radius: 6px; background: %WarningSoft%;
                color: %WarningText%; border: 1px solid %WarningBorder%;
                font-size: 11px; font-weight: 700;
            }
            QLabel#neutralBadge {
                padding: 3px 8px; border-radius: 6px; background: %NeutralSoft%;
                color: %TextMid%; border: 1px solid %BorderInput%; font-size: 11px;
            }
        )");
        qss += QStringLiteral(R"(

            QTableWidget {
                background: %White%; border: none; gridline-color: %Border%;
                selection-background-color: %PrimarySoft%; selection-color: %Text%;
                font-size: 13px; alternate-background-color: %Surface%;
            }
            QTableWidget::viewport { background: %White%; }
            QTableWidget::item { border: none; padding: 0 8px; background: %White%; }
            QWidget#tableCell { background: %White%; border: none; }
            QWidget#tableCell[selected="true"] { background: %White%; border: none; }
            QHeaderView::section {
                background: %Surface%; color: %Subtext%; border: none;
                border-bottom: 1px solid %Border%; padding: 10px 12px;
                font-family: "Pretendard"; font-size: 13px; font-weight: 600;
            }
            QScrollBar:vertical {
                background: transparent; width: 10px; margin: 2px;
            }
            QScrollBar::handle:vertical {
                background: %BorderInput%; border-radius: 5px; min-height: 30px;
            }
            QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
            QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
                height: 0; background: transparent;
            }

            QPushButton#primaryButton {
                min-height: 36px; padding: 0 16px; border-radius: 8px;
                background: %Primary%; color: #FFFFFF; border: none;
                font-size: 14px; font-weight: 600;
            }
            QPushButton#primaryButton:hover { background: %PrimaryHover%; }
            QPushButton#secondaryButton {
                min-height: 38px; padding: 0 18px; border-radius: 8px;
                background: %InputBg%; color: %TextMid%; border: 1px solid %BorderInput%;
                font-size: 14px; font-weight: 600;
            }
            QPushButton#secondaryButton:hover { background: %HoverSurface%; color: %Text%; }
            QPushButton#usbAllowButton {
                min-height: 36px; padding: 0 16px; border-radius: 8px;
                background: %Primary%; color: #FFFFFF; border: none;
                font-size: 14px; font-weight: 600;
            }
            QPushButton#usbAllowButton:hover { background: %PrimaryHover%; }
            QPushButton#usbBlockButton {
                min-height: 36px; padding: 0 16px; border-radius: 8px;
                background: %DangerAction%; color: #FFFFFF; border: none;
                font-size: 14px; font-weight: 600;
            }
            QPushButton#usbBlockButton:hover { background: %DangerHover%; }
            QPushButton#usbPendingButton {
                min-height: 36px; padding: 0 16px; border-radius: 8px;
                background: %InputBg%; color: %TextMid%; border: 1px solid %BorderInput%;
                font-size: 14px; font-weight: 600;
            }
            QPushButton#usbPendingButton:hover { background: %HoverSurface%; color: %Text%; }
            QPushButton#usbAllowButton:disabled,
            QPushButton#usbBlockButton:disabled,
            QPushButton#usbPendingButton:disabled {
                background: %SurfaceAlt%; color: %TextMuted%; border: 1px solid %Border%;
            }
            QPushButton#bufferButton {
                min-height: 40px; padding: 0 18px 0 12px; border-radius: 8px;
                background: %Primary%;
                color: #FFFFFF; border: none; font-size: 14px; font-weight: 600;
                text-align: left;
            }
            QPushButton#bufferButton:hover { background: %PrimaryHover%; }

            QFrame#themeToggle { background: %Surface%; border: 1px solid %Border%; border-radius: 17px; }
            QFrame#themeSegmentGroup {
                background: %SurfaceAlt%;
                border: 1px solid %BorderInput%;
                border-radius: 12px;
            }
            QPushButton#themeSegment {
                min-height: 36px;
                max-height: 36px;
                padding: 0 14px;
                border: none;
                border-radius: 9px;
                background: transparent;
                color: %SubtextMid%;
                font-size: 13px;
                font-weight: 600;
            }
            QPushButton#themeSegment:hover {
                color: %Text%;
                background: %HoverSurface%;
            }
            QPushButton#themeSegment:checked {
                background: %White%;
                color: %TextDark%;
                border: 1px solid %Border%;
            }
            QLabel#notificationBadge {
                background: %Primary%; color: #FFFFFF; border-radius: 7px; font-size: 10px; padding: 0 4px;
            }
            QLabel#roleBadge {
                background: %Surface%; border: 1px solid %Border%; border-radius: 8px;
                padding: 6px 12px; color: %SubtextMid%; font-size: 13px;
            }

            QLabel#sectionHeading { font-size: 15px; color: %TextDark%; }
            QFrame#usbDetailIconBox { border: none; background: transparent; }
            QLabel#usbDetailName { font-size: 16px; color: %Text%; }
            QLabel#usbDetailMfr { font-size: 12px; color: %Subtext%; }
            QWidget#usbDetailInfoRow {
                background: transparent;
                border-bottom: 1px solid %Border%;
            }
            QLabel#usbDetailInfoLabel {
                color: %Subtext%;
                font-size: 13px;
                border: none;
                background: transparent;
            }
            QWidget#usbDetailStatusWrap { background: transparent; border: none; }
            QFrame#usbDetailStatusDot {
                background: %BorderInput%;
                border-radius: 4px;
                border: none;
            }
            QFrame#usbDetailStatusDot[connected="true"] {
                background: %Success%;
            }
            QLabel#usbDetailStatusText {
                color: %SubtextMid%;
                font-size: 14px;
                border: none;
                background: transparent;
            }
            QLabel#usbDetailValue {
                color: %Text%;
                font-size: 13px;
                border: none;
                background: transparent;
            }
            QLabel#usbDetailVidBadge {
                background: %InputBg%;
                border: 1px solid %BorderInput%;
                border-radius: 6px;
                color: %TextMid%;
                padding: 2px 6px;
            }
            QLabel#usbActionFeedback {
                color: %Subtext%;
                font-size: 12px;
                background: transparent;
                border: none;
            }
            QLabel#usbActionFeedback[tone="primary"] { color: %PrimaryText%; }
            QLabel#usbActionFeedback[tone="danger"] { color: %DangerAction%; }
            QWidget#detailPanelHeader {
                background: %Surface%;
                border-bottom: 1px solid %Border%;
            }
            QLabel#detailEventTitle { font-size: 18px; color: %Text%; }
            QLabel#detailEventTime { font-size: 12px; color: %Subtext%; }
            QLabel#detailSectionLabel { font-size: 12px; color: %Subtext%; }
            QLabel#detailDeviceBadge {
                background: %InputBg%; border: 1px solid %BorderInput%;
                border-radius: 6px; padding: 5px 8px; font-size: 13px; color: %TextMid%;
            }
            QLabel#detailDescription { font-size: 14px; color: %Text%; }
            QLabel#detailUserValue { font-size: 14px; color: %Text%; }
            QFrame#detailActionCard { background: transparent; border: none; }
            QLabel#detailActionTitle {
                font-size: 13px; color: %Danger%; background: transparent; border: none;
            }
            QLabel#detailActionDesc {
                font-size: 13px; color: %TextMid%; background: transparent; border: none;
            }
            QFrame#detailButtonArea { border-top: 1px solid %Border%; }
            QLabel#policySummaryLabel { font-size: 14px; color: %SubtextMid%; }
            QLabel#policySummaryValue { font-size: 14px; color: %Text%; }
            QLabel#settingsWarningBadge {
                background: %WarningSoft%; color: %WarningText%; border: 1px solid %WarningBorder%;
                border-radius: 7px; font-size: 11px; padding: 0; font-weight: 700;
            }
            QLabel#settingsSuccessBadge {
                background: %PrimarySoft%; color: %PrimaryText%; border: 1px solid %PrimarySoftBorder%;
                border-radius: 7px; font-size: 11px; padding: 0; font-weight: 700;
            }
        )");
        qss += QStringLiteral(R"(
            QLabel#systemInfoLabel { font-size: 12px; color: %Subtext%; }
            QLabel#systemInfoValue { font-size: 14px; color: %Text%; }
            QFrame#usbStatusDot {
                background: %BorderInput%;
                border-radius: 4px;
                border: none;
            }
            QFrame#usbStatusDot[connected="true"] {
                background: %Success%;
            }
            QLabel#usbStatusText {
                color: %SubtextMid%;
                font-size: 14px;
                border: none;
                background: transparent;
            }
            QLabel#usbCellText {
                color: %TextDark%;
                font-size: 14px;
                font-weight: 600;
                border: none;
                background: transparent;
            }
            QLabel#usbCellMutedText {
                color: %TextMid%;
                font-size: 14px;
                border: none;
                background: transparent;
            }
            QLabel#usbDeviceName {
                color: %TextDark%;
                font-size: 14px;
                font-weight: 700;
                border: none;
                background: transparent;
            }
            QLabel#sessionSectionLabel {
                color: %TextDark%;
                font-size: 13px;
                font-weight: 700;
                background: transparent;
                border: none;
            }
            QLabel#sessionKeyLabel {
                color: %SubtextMid%;
                font-size: 13px;
                background: transparent;
                border: none;
            }
            QLabel#sessionValueLabel {
                color: %TextMid%;
                font-size: 13px;
                font-weight: 600;
                background: transparent;
                border: none;
            }
            QLabel#sessionDangerValue {
                color: %DangerText%;
                font-size: 13px;
                font-weight: 700;
                background: transparent;
                border: none;
            }
            QFrame#sessionWarningBanner {
                background: transparent;
                border: none;
            }
            QLabel#sessionWarningIcon {
                color: %Warning%;
                font-size: 14px;
                background: transparent;
                border: none;
            }
            QLabel#sessionWarningTitle {
                color: %WarningText%;
                font-size: 13px;
                background: transparent;
                border: none;
            }
            QLabel#sessionWarningDesc {
                color: %WarningText%;
                font-size: 12px;
                background: transparent;
                border: none;
            }
            QLabel#logTextMuted {
                color: %Subtext%;
                font-size: 14px;
                border: none;
                background: transparent;
            }
            QLabel#logTextMid {
                color: %SubtextMid%;
                font-size: 14px;
                border: none;
                background: transparent;
            }
            QLabel#logEventText {
                color: %Text%;
                font-size: 14px;
                border: none;
                background: transparent;
            }
            QLabel#logCellIcon { background: transparent; border: none; }
            QLabel#helpH2 {
                color: %TextDark%;
                font-size: 20px;
                font-weight: 700;
                background: transparent;
                border: none;
            }
            QLabel#helpH3 {
                color: %TextDark%;
                font-size: 18px;
                font-weight: 700;
                background: transparent;
                border: none;
            }
            QLabel#helpPara, QLabel#helpBulletText {
                color: %TextMid%;
                font-size: 14px;
                background: transparent;
                border: none;
            }
            QLabel#helpBulletDot {
                color: %SubtextMid%;
                font-size: 16px;
                background: transparent;
                border: none;
            }
            QFrame#helpNote {
                background: %PrimarySoft%;
                border: 1px solid %PrimarySoftBorder%;
                border-radius: 8px;
            }
            QLabel#helpNoteText {
                color: %PrimaryText%;
                font-size: 14px;
                background: transparent;
                border: none;
            }
            QWidget#helpNavItem {
                background: transparent;
                border-radius: 8px;
            }
            QWidget#helpNavItem[active="true"] {
                background: %PrimarySoft%;
            }
            QLabel#helpNavCategory {
                color: %TextMuted%;
                font-size: 10px;
                background: transparent;
                border: none;
            }
            QLabel#helpNavCategory[active="true"] {
                color: %PrimaryText%;
            }
            QLabel#helpNavTitle {
                color: %TextMid%;
                font-size: 14px;
                background: transparent;
                border: none;
            }
            QLabel#helpNavTitle[active="true"] {
                color: %PrimaryText%;
                font-weight: 700;
            }
            QLabel#helpNavChevron {
                color: transparent;
                font-size: 16px;
                background: transparent;
                border: none;
            }
            QLabel#helpNavChevron[active="true"] {
                color: %PrimaryText%;
            }
            QWidget#helpNavFooter {
                background: %Surface%;
                border-top: 1px solid %Border%;
            }

            QPushButton#settingsTab {
                min-height: 34px; padding: 2px 6px 0px 6px; border: none;
                border-bottom: 2px solid transparent; background: transparent;
                color: %Subtext%; font-size: 14px;
            }
            QPushButton#settingsTab:hover { color: %Primary%; }
            QPushButton#settingsTab[active="true"] { color: %Primary%; border-bottom: 2px solid %Primary%; }
            QWidget#settingsTabBar { background: %Surface%; border-bottom: 1px solid %Border%; }

            QCheckBox#settingsCheck { color: %TextMid%; font-size: 14px; spacing: 10px; }
            QCheckBox#settingsCheck::indicator {
                width: 16px; height: 16px; border: 1.5px solid %BorderInput%;
                border-radius: 4px; background: %InputBg%;
            }
            QCheckBox#settingsCheck::indicator:checked {
                background: %Primary%; border-color: %Primary%; image: url(:/icons/check-white.svg);
            }
            QRadioButton#settingsRadio {
                min-height: 38px;
                padding: 0 14px;
                border-radius: 8px;
                border: 1px solid %BorderInput%;
                background: %InputBg%;
                color: %TextMid%;
                font-size: 14px;
                font-weight: 600;
                spacing: 0;
            }
            QRadioButton#settingsRadio::indicator {
                width: 0;
                height: 0;
                border: none;
                background: transparent;
            }
            QRadioButton#settingsRadio:hover {
                background: %HoverSurface%;
                color: %Text%;
            }
            QRadioButton#settingsRadio:checked {
                background: %PrimarySoft%;
                color: %PrimaryText%;
                border: 1px solid %PrimarySoftBorder%;
            }
            QWidget#settingsSectionDivider {
                background: transparent;
                border: none;
            }
            QWidget#settingsFormRow {
                background: transparent;
                border: none;
            }
            QLabel#settingsFormLabel {
                color: %TextMid%;
                font-size: 14px;
                background: transparent;
                border: none;
            }

            QLineEdit#searchInput, QLineEdit {
                height: 36px; padding-left: 8px; border: 1px solid %BorderInput%;
                border-radius: 8px; background: %InputBg%; font-size: 14px; color: %Text%;
            }
            QWidget#searchBox {
                background: %InputBg%;
                border: 1px solid %BorderInput%;
                border-radius: 8px;
            }
            QWidget#searchBox QLineEdit#searchInput {
                height: 34px;
                padding-left: 0;
                border: none;
                border-radius: 0;
                background: transparent;
                font-size: 14px;
                color: %Text%;
            }
            QLineEdit:focus { border-color: %Primary%; }

            QFrame#divider { background: %Border%; min-height: 1px; max-height: 1px; }

            QComboBox {
                border: 1px solid %BorderInput%; border-radius: 8px;
                padding: 6px 32px 6px 12px; background: %InputBg%;
                font-size: 14px; color: %Text%; min-height: 34px;
            }
            QComboBox:hover { border-color: %PrimarySoftBorder%; }
            QComboBox::drop-down { border: none; width: 28px; }
            QComboBox QAbstractItemView {
                border: 1px solid %BorderInput%; border-radius: 8px;
                selection-background-color: %PrimarySoft%; selection-color: %Text%;
                background: %InputBg%; color: %Text%; outline: none;
            }

            QSpinBox#numberInput, QSpinBox {
                border: 1px solid %BorderInput%; border-radius: 8px; padding: 4px 8px;
                background: %InputBg%; font-size: 14px; color: %Text%; min-height: 34px;
            }
            QSpinBox#numberInput::up-button, QSpinBox#numberInput::down-button {
                width: 0px;
            }
        )");
        return replaceTokens(qss, t).replace(QStringLiteral("%NeutralSoft%"), QLatin1String(t.NeutralSoft));
    }

    inline QString appStyle()
    {
        return appStyle(activeMode());
    }
}
