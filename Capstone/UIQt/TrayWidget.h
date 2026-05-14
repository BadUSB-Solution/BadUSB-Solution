#pragma once

#include <QWidget>

class QFrame;
class QLabel;
class QPushButton;

// ---------------------------------------------------------------------------
// TrayWidget — compact system-tray popup (470 × 249 px)
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

signals:
    void expandRequested();

private:
    // Dynamic UI elements
    QLabel*      usbCountLabel_  = nullptr;
    QLabel*      threatIconLabel_ = nullptr;
    QLabel*      statusDot_      = nullptr;
    QLabel*      statusText_     = nullptr;
    QPushButton* statusBadge_    = nullptr;
    QLabel*      eventText_      = nullptr;
    QLabel*      eventTime_      = nullptr;

    ThreatLevel level_ = ThreatLevel::Warning;
    void applyLevel();
};
