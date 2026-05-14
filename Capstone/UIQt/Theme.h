#pragma once
#include <QString>

namespace UiTheme
{
    // ─────────────────────────────────────────────────────────────────────────
    //  Design Tokens
    //
    //  Blue rebrand: swap Primary / PrimaryHover / PrimarySoft and rebuild.
    //  Everything in appStyle() and all UiTheme:: references in .cpp files
    //  follow automatically.
    // ─────────────────────────────────────────────────────────────────────────

    // Brand / interactive
    constexpr const char* Primary      = "#1E8FFF";
    constexpr const char* PrimaryHover = "#0B68D8";
    constexpr const char* PrimarySoft  = "#EAF4FF";

    // Text
    constexpr const char* Text         = "#1D293D";
    constexpr const char* Subtext      = "#62748E";
    constexpr const char* SubtextMid   = "#45556C";
    constexpr const char* TextDark     = "#0F172B";
    constexpr const char* TextMid      = "#314158";
    constexpr const char* TextMuted    = "#90A1B9";

    // Surface / structure
    constexpr const char* AppBg        = "#F6F8FB";
    constexpr const char* Surface      = "#F8FAFC";
    constexpr const char* SurfaceAlt   = "#F1F5F9";
    constexpr const char* White        = "#FFFFFF";
    constexpr const char* Border       = "#E2E8F0";
    constexpr const char* BorderInput  = "#CAD5E2";

    // Brand extended
    constexpr const char* PrimarySoftBorder = "#B9DCFF";   // border on primary-soft surfaces
    constexpr const char* PrimaryText        = "#0B4EA2";   // dark primary text on PrimarySoft bg

    // Semantic — intentionally fixed regardless of brand recolor
    constexpr const char* Success      = "#00BC7D";
    constexpr const char* Warning      = "#F97316";   // amber: UI-level warning badge / tray dot
    constexpr const char* Danger       = "#FF2056";   // danger accent (status card left border)

    // Danger action — for destructive action buttons (block / deny)
    constexpr const char* DangerAction = "#EC003F";
    constexpr const char* DangerHover  = "#C5002F";
    constexpr const char* DangerText   = "#EF4444";   // danger text in data rows

    // Warning palette
    constexpr const char* WarningSoft   = "#FFFAEB";
    constexpr const char* WarningBorder = "#FEDF89";
    constexpr const char* WarningText   = "#B26A00";

    // Chart palette
    constexpr const char* ChartNormal    = "#B9DCFF";   // normal-range bars / neutral border
    constexpr const char* ChartRisk      = "#0B68D8";   // cumulative risk score line
    constexpr const char* ChartThreshold = "#1E8FFF";   // anomaly bars + threshold dashed line

    // Terminal / raw-data areas
    constexpr const char* TerminalBg   = "#0F172B";
    constexpr const char* TerminalText = "#C8D8F0";

    // ─────────────────────────────────────────────────────────────────────────
    //  App Style Sheet
    //
    //  All  %TokenName%  placeholders are resolved once on first call.
    //  Only the token constants above need to change for a full rebrand.
    // ─────────────────────────────────────────────────────────────────────────
    inline QString appStyle()
    {
        static const QString result = QString(QStringLiteral(R"(
            * {
                color: %Text%;
            }

            QMainWindow, QWidget#centralRoot {
                background: %AppBg%;
            }

            QWidget#sidebar {
                background: %White%;
                border-right: 1px solid %Border%;
            }

            QLabel#brandText {
                font-size: 19px;
                color: %TextDark%;
            }

            QPushButton#navButton {
                min-height: 48px;
                padding: 0 20px 0 20px;
                border: none;
                border-radius: 0;
                text-align: left;
                font-size: 15px;
                color: %Subtext%;
                background: transparent;
            }

            QPushButton#navButton:hover {
                background: %PrimarySoft%;
                color: %Primary%;
            }

            QPushButton#navButton[active="true"] {
                background: %PrimarySoft%;
                color: %Primary%;
                border-left: 4px solid %Primary%;
            }

            QPushButton#topNavButton {
                min-height: 64px;
                padding: 0 2px;
                border: none;
                border-radius: 0;
                background: transparent;
                color: %SubtextMid%;
                font-size: 14px;
            }

            QPushButton#topNavButton:hover {
                color: %Primary%;
            }

            QPushButton#topNavButton[active="true"] {
                color: %Primary%;
                border-bottom: 2px solid %Primary%;
            }

            QPushButton#iconButton {
                min-width: 34px;
                max-width: 34px;
                min-height: 34px;
                max-height: 34px;
                border-radius: 17px;
                border: 1px solid %Border%;
                background: %Surface%;
            }

            QWidget#topbar {
                background: %White%;
                border-bottom: 1px solid %Border%;
            }

            QLabel#pageTitle {
                font-size: 24px;
                color: %Text%;
            }

            QLabel#pageSubtitle {
                font-size: 14px;
                color: %Subtext%;
            }

            QFrame#card {
                background: %White%;
                border: 1px solid %Border%;
                border-radius: 9px;
            }

            QWidget#cardHeader {
                border-bottom: 1px solid %Border%;
            }

            QLabel#cardTitle {
                font-size: 14px;
                color: %TextMid%;
            }

            QFrame#statIconCircle {
                background: %White%;
                border: 1px solid %Border%;
                border-radius: 23px;
            }

            QFrame#statusPanel {
                background: %White%;
                border: 1px solid %Border%;
                border-left: 4px solid %Success%;
                border-radius: 6px;
            }

            QFrame#successAccent {
                background: %Success%;
                border: none;
            }

            QFrame#warningAccent {
                background: %Warning%;
                border: none;
            }

            QFrame#dangerAccent {
                background: %Danger%;
                border: none;
            }

            QLabel#statNumber {
                font-size: 24px;
                color: %TextDark%;
            }

            QLabel#mutedText {
                font-size: 12px;
                color: %TextMuted%;
            }

            QLabel#policyDangerText {
                font-size: 13px;
                color: %Primary%;
            }

            QLabel#policyValueText {
                font-size: 14px;
                color: %TextDark%;
            }

            QLabel#policySuccessText {
                font-size: 14px;
                color: #009966;
            }

            QLabel#successBadge {
                padding: 3px 8px;
                border-radius: 6px;
                background: #ECFDF3;
                color: #027A48;
                border: 1px solid #ABEFC6;
                font-size: 11px;
            }

            QLabel#protectionOnBadge {
                min-width: 48px;
                max-width: 48px;
                min-height: 28px;
                max-height: 28px;
                padding: 0;
                border-radius: 7px;
                background: #ECFDF3;
                color: #027A48;
                border: 1px solid #ABEFC6;
                font-size: 11px;
            }

            QLabel#dangerBadge {
                padding: 3px 8px;
                border-radius: 6px;
                background: #FEF3F2;
                color: #E34B3F;
                border: 1px solid #FECACA;
                font-size: 11px;
            }

            QLabel#warningBadge {
                padding: 3px 8px;
                border-radius: 6px;
                background: %WarningSoft%;
                color: %WarningText%;
                border: 1px solid %WarningBorder%;
                font-size: 11px;
            }

            QTableWidget {
                background: %White%;
                border: none;
                gridline-color: %SurfaceAlt%;
                selection-background-color: %PrimarySoft%;
                selection-color: %Text%;
                font-size: 13px;
            }

            QHeaderView::section {
                background: %Surface%;
                color: %Subtext%;
                border: none;
                border-bottom: 1px solid %Border%;
                padding: 10px 12px;
                font-size: 12px;
            }

            QPushButton#primaryButton {
                min-height: 36px;
                padding: 0 16px;
                border-radius: 8px;
                background: %Primary%;
                color: %White%;
                border: none;
            }

            QPushButton#primaryButton:hover {
                background: %PrimaryHover%;
            }

            QPushButton#secondaryButton {
                min-height: 38px;
                padding: 0 18px;
                border-radius: 8px;
                background: %White%;
                color: %TextMid%;
                border: 1px solid %Border%;
            }

            QFrame#themeToggle {
                background: %Surface%;
                border: 1px solid %Border%;
                border-radius: 17px;
            }

            QLabel#notificationBadge {
                background: %Primary%;
                color: %White%;
                border-radius: 7px;
                font-size: 10px;
                padding: 0 4px;
            }

            QLabel#roleBadge {
                background: %Surface%;
                border: 1px solid %Border%;
                border-radius: 8px;
                padding: 6px 12px;
                color: %SubtextMid%;
                font-size: 13px;
            }

            QLabel#neutralBadge {
                padding: 3px 8px;
                border-radius: 6px;
                background: %White%;
                color: %TextMid%;
                border: 1px solid %BorderInput%;
                font-size: 11px;
            }

            QLabel#blockBadge {
                padding: 3px 8px;
                border-radius: 6px;
                background: #FEF3F2;
                color: #E93729;
                border: 1px solid #FECDCA;
                font-size: 11px;
            }

            QLabel#sectionHeading {
                font-size: 14px;
                color: %Text%;
            }

            QLabel#systemInfoLabel {
                font-size: 12px;
                color: %Subtext%;
            }

            QLabel#systemInfoValue {
                font-size: 14px;
                color: %Text%;
            }

            QPushButton#settingsTab {
                min-height: 34px;
                padding: 2px 6px 0px 6px;
                border: none;
                border-bottom: 2px solid transparent;
                background: transparent;
                color: %Subtext%;
                font-size: 14px;
            }

            QPushButton#settingsTab:hover {
                color: %Primary%;
            }

            QPushButton#settingsTab[active="true"] {
                color: %Primary%;
                border-bottom: 2px solid %Primary%;
            }

            QWidget#settingsTabBar {
                background: %Surface%;
                border-bottom: 1px solid %Border%;
            }

            QCheckBox#settingsCheck {
                color: %TextMid%;
                font-size: 14px;
                spacing: 10px;
            }

            QCheckBox#settingsCheck::indicator {
                width: 16px;
                height: 16px;
                border: 1.5px solid %BorderInput%;
                border-radius: 4px;
                background: white;
            }

            QCheckBox#settingsCheck::indicator:checked {
                background: %Primary%;
                border-color: %Primary%;
                image: url(:/icons/check-white.svg);
            }

            QLineEdit#searchInput {
                height: 36px;
                padding-left: 8px;
                border: 1px solid %BorderInput%;
                border-radius: 8px;
                background: white;
                font-size: 14px;
                color: %Text%;
            }

            QFrame#divider {
                background: %Border%;
                min-height: 1px;
                max-height: 1px;
            }

            QComboBox {
                border: 1px solid %BorderInput%;
                border-radius: 8px;
                padding: 6px 32px 6px 12px;
                background: white;
                font-size: 14px;
                color: %Text%;
                min-height: 34px;
            }

            QComboBox::drop-down {
                border: none;
                width: 28px;
            }

            QComboBox::down-arrow {
                width: 12px;
                height: 12px;
            }

            QComboBox QAbstractItemView {
                border: 1px solid %BorderInput%;
                border-radius: 8px;
                selection-background-color: %PrimarySoft%;
                selection-color: %Primary%;
                background: white;
                outline: none;
            }

            QSpinBox#numberInput {
                border: 1px solid %BorderInput%;
                border-radius: 8px;
                padding: 4px 8px;
                background: white;
                font-size: 14px;
                color: %Text%;
                min-height: 34px;
            }

            QSpinBox#numberInput::up-button, QSpinBox#numberInput::down-button {
                width: 0px;
            }
        )"))
        .replace(QStringLiteral("%Primary%"),      QLatin1String(Primary))
        .replace(QStringLiteral("%PrimaryHover%"), QLatin1String(PrimaryHover))
        .replace(QStringLiteral("%PrimarySoft%"),  QLatin1String(PrimarySoft))
        .replace(QStringLiteral("%Text%"),         QLatin1String(Text))
        .replace(QStringLiteral("%Subtext%"),      QLatin1String(Subtext))
        .replace(QStringLiteral("%SubtextMid%"),   QLatin1String(SubtextMid))
        .replace(QStringLiteral("%TextDark%"),     QLatin1String(TextDark))
        .replace(QStringLiteral("%TextMid%"),      QLatin1String(TextMid))
        .replace(QStringLiteral("%TextMuted%"),    QLatin1String(TextMuted))
        .replace(QStringLiteral("%AppBg%"),        QLatin1String(AppBg))
        .replace(QStringLiteral("%Surface%"),      QLatin1String(Surface))
        .replace(QStringLiteral("%SurfaceAlt%"),   QLatin1String(SurfaceAlt))
        .replace(QStringLiteral("%White%"),        QLatin1String(White))
        .replace(QStringLiteral("%Border%"),       QLatin1String(Border))
        .replace(QStringLiteral("%BorderInput%"),  QLatin1String(BorderInput))
        .replace(QStringLiteral("%Success%"),          QLatin1String(Success))
        .replace(QStringLiteral("%Warning%"),          QLatin1String(Warning))
        .replace(QStringLiteral("%Danger%"),           QLatin1String(Danger))
        .replace(QStringLiteral("%WarningSoft%"),      QLatin1String(WarningSoft))
        .replace(QStringLiteral("%WarningBorder%"),    QLatin1String(WarningBorder))
        .replace(QStringLiteral("%WarningText%"),      QLatin1String(WarningText));

        return result;
    }
}
