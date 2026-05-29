#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;

// ---------------------------------------------------------------------------
// TrayWidget - compact system-tray popup (560 x 258 px)
//
// Three security states:
//   Normal  — green  "정상"   / "안전"       badge
//   Warning — orange "경고"   / "주의 필요"  badge
//   Danger  — red    "위험"   / "즉시 대응"  badge
//
// Signal expandRequested() → caller should show the full MainWindow.
// ---------------------------------------------------------------------------
class TrayWidget final : public QWidget
{
    Q_OBJECT

public:
    enum class ThreatLevel { Normal, Warning, Danger };

    explicit TrayWidget(QWidget* parent = nullptr);

    // Data setters (called both during mock init and later with real data)
    void setThreatLevel(ThreatLevel level);
    void setUsbCount(int count);
    void setRecentEvent(const QString& time, const QString& text);

    // Position popup at bottom-right of available screen area (above taskbar)
    void positionNearTray();

    // Re-apply all dynamic styles using current UiTheme::tokens().
    // Called from the constructor and whenever the theme changes.
    void applyTheme();

signals:
    void expandRequested();

private:
    // Dynamic UI elements that need re-styling on theme switch
    QFrame*      container_       = nullptr;
    QLabel*      titleLabel_      = nullptr;
    QLabel*      subtitleLabel_   = nullptr;
    QLabel*      monitoringBadge_ = nullptr;
    QPushButton* expandBtn_       = nullptr;
    QPushButton* closeBtn_        = nullptr;
    QFrame*      usbCard_         = nullptr;
    QFrame*      threatCard_      = nullptr;
    QFrame*      usbIconCircle_   = nullptr;
    QLabel*      usbHeaderLabel_  = nullptr;
    QLabel*      usbCountLabel_   = nullptr;
    QPushButton* realtimeBtn_     = nullptr;
    QLabel*      threatHeaderLabel_ = nullptr;
    QLabel*      threatIconLabel_ = nullptr;
    QLabel*      statusText_      = nullptr;
    QLabel*      mascot_          = nullptr;
    QWidget*     footer_          = nullptr;
    QLabel*      footerLabel_     = nullptr;
    QFrame*      footerDivider_   = nullptr;
    QLabel*      eventText_       = nullptr;
    QLabel*      eventTime_       = nullptr;

    ThreatLevel level_ = ThreatLevel::Warning;
    void applyLevel();
};
