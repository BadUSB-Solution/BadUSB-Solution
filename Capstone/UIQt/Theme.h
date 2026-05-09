#pragma once

namespace UiTheme
{
    inline constexpr const char* AppStyle = R"(
        * {
            color: #1D293D;
        }

        QMainWindow, QWidget#centralRoot {
            background: #F6F8FB;
        }

        QWidget#sidebar {
            background: #FFFFFF;
            border-right: 1px solid #E2E8F0;
        }

        QLabel#brandText {
            font-size: 19px;
            color: #0F172B;
        }

        QPushButton#navButton {
            min-height: 48px;
            padding: 0 20px 0 20px;
            border: none;
            border-radius: 0;
            text-align: left;
            font-size: 15px;
            color: #62748E;
            background: transparent;
        }

        QPushButton#navButton:hover {
            background: #FFF7ED;
            color: #FF6900;
        }

        QPushButton#navButton[active="true"] {
            background: #FFF7ED;
            color: #FF6900;
            border-left: 4px solid #FF6900;
        }

        QPushButton#topNavButton {
            min-height: 64px;
            padding: 0 2px;
            border: none;
            border-radius: 0;
            background: transparent;
            color: #45556C;
            font-size: 14px;
        }

        QPushButton#topNavButton:hover {
            color: #FF6900;
        }

        QPushButton#topNavButton[active="true"] {
            color: #FF6900;
            border-bottom: 2px solid #FF6900;
        }

        QPushButton#iconButton {
            min-width: 34px;
            max-width: 34px;
            min-height: 34px;
            max-height: 34px;
            border-radius: 17px;
            border: 1px solid #E2E8F0;
            background: #F8FAFC;
        }

        QWidget#topbar {
            background: #FFFFFF;
            border-bottom: 1px solid #E2E8F0;
        }

        QLabel#pageTitle {
            font-size: 24px;
            color: #1D293D;
        }

        QLabel#pageSubtitle {
            font-size: 14px;
            color: #62748E;
        }

        QFrame#card {
            background: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-radius: 9px;
        }

        QWidget#cardHeader {
            border-bottom: 1px solid #E2E8F0;
        }

        QLabel#cardTitle {
            font-size: 14px;
            color: #314158;
        }

        QFrame#statIconCircle {
            background: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-radius: 23px;
        }

        QFrame#statusPanel {
            background: #FFFFFF;
            border: 1px solid #E2E8F0;
            border-left: 4px solid #00BC7D;
            border-radius: 6px;
        }

        QFrame#successAccent {
            background: #00BC7D;
            border: none;
        }

        QFrame#warningAccent {
            background: #FF6900;
            border: none;
        }

        QFrame#dangerAccent {
            background: #FF2056;
            border: none;
        }

        QLabel#statNumber {
            font-size: 24px;
            color: #0F172B;
        }

        QLabel#mutedText {
            font-size: 12px;
            color: #90A1B9;
        }

        QLabel#policyDangerText {
            font-size: 13px;
            color: #FF6900;
        }

        QLabel#policyValueText {
            font-size: 14px;
            color: #0F172B;
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
            background: #FFFAEB;
            color: #B26A00;
            border: 1px solid #FEDF89;
            font-size: 11px;
        }

        QTableWidget {
            background: #FFFFFF;
            border: none;
            gridline-color: #F1F5F9;
            selection-background-color: #FFF7ED;
            selection-color: #1D293D;
            font-size: 13px;
        }

        QHeaderView::section {
            background: #F8FAFC;
            color: #62748E;
            border: none;
            border-bottom: 1px solid #E2E8F0;
            padding: 10px 12px;
            font-size: 12px;
        }

        QPushButton#primaryButton {
            min-height: 36px;
            padding: 0 16px;
            border-radius: 8px;
            background: #FF6900;
            color: #FFFFFF;
            border: none;
        }

        QPushButton#secondaryButton {
            min-height: 38px;
            padding: 0 18px;
            border-radius: 8px;
            background: #FFFFFF;
            color: #314158;
            border: 1px solid #E2E8F0;
        }

        QFrame#themeToggle {
            background: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 17px;
        }

        QLabel#notificationBadge {
            background: #FF6900;
            color: #FFFFFF;
            border-radius: 7px;
            font-size: 10px;
            padding: 0 4px;
        }

        QLabel#roleBadge {
            background: #F8FAFC;
            border: 1px solid #E2E8F0;
            border-radius: 8px;
            padding: 6px 12px;
            color: #45556C;
            font-size: 13px;
        }

        QLabel#neutralBadge {
            padding: 3px 8px;
            border-radius: 6px;
            background: #FFFFFF;
            color: #314158;
            border: 1px solid #CAD5E2;
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
            color: #1D293D;
        }

        QLabel#systemInfoLabel {
            font-size: 12px;
            color: #62748E;
        }

        QLabel#systemInfoValue {
            font-size: 14px;
            color: #1D293D;
        }

        QPushButton#settingsTab {
            min-height: 34px;
            padding: 2px 6px 0px 6px;
            border: none;
            border-bottom: 2px solid transparent;
            background: transparent;
            color: #62748E;
            font-size: 14px;
        }

        QPushButton#settingsTab:hover {
            color: #FF6900;
        }

        QPushButton#settingsTab[active="true"] {
            color: #FF6900;
            border-bottom: 2px solid #FF6900;
        }

        QWidget#settingsTabBar {
            background: #F8FAFC;
            border-bottom: 1px solid #E2E8F0;
        }

        QCheckBox#settingsCheck {
            color: #314158;
            font-size: 14px;
            spacing: 10px;
        }

        QCheckBox#settingsCheck::indicator {
            width: 16px;
            height: 16px;
            border: 1.5px solid #CAD5E2;
            border-radius: 4px;
            background: white;
        }

        QCheckBox#settingsCheck::indicator:checked {
            background: #FF6900;
            border-color: #FF6900;
            image: url(:/icons/check-white.svg);
        }

        QLineEdit#searchInput {
            height: 36px;
            padding-left: 8px;
            border: 1px solid #CAD5E2;
            border-radius: 8px;
            background: white;
            font-size: 14px;
            color: #1D293D;
        }

        QFrame#divider {
            background: #E2E8F0;
            min-height: 1px;
            max-height: 1px;
        }

        QComboBox {
            border: 1px solid #CAD5E2;
            border-radius: 8px;
            padding: 6px 32px 6px 12px;
            background: white;
            font-size: 14px;
            color: #1D293D;
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
            border: 1px solid #CAD5E2;
            border-radius: 8px;
            selection-background-color: #FFF7ED;
            selection-color: #FF6900;
            background: white;
            outline: none;
        }

        QSpinBox#numberInput {
            border: 1px solid #CAD5E2;
            border-radius: 8px;
            padding: 4px 8px;
            background: white;
            font-size: 14px;
            color: #1D293D;
            min-height: 34px;
        }

        QSpinBox#numberInput::up-button, QSpinBox#numberInput::down-button {
            width: 0px;
        }
    )";
}
