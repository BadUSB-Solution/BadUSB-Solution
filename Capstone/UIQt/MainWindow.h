#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <QList>
#include <QMainWindow>
#include <QVector>

class QFrame;
class QLabel;
class QPushButton;
class QStackedWidget;
class QSystemTrayIcon;
class QTableWidget;
class QWidget;
class TrayWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
    void closeEvent(QCloseEvent* event) override;

private:
    struct UsbRowData {
        QString name, type, vidpid, status, policy, lastSeen, iconRes, manufacturer;
        uint16_t vid = 0, pid = 0;
        uint64_t fingerprint = 0;
        std::vector<std::wstring> devicePaths;
    };

    QPushButton* addNavButton(const QString& text, int pageIndex, const QString& iconResource);
    QPushButton* addTopNavButton(const QString& text, int pageIndex);
    QWidget* createDashboardPage();
    QWidget* createUsbListPage(bool showTitle = true);
    QWidget* createDashboardUsbListPage();
    QWidget* createLogPage();
    QWidget* createAnalyticsPage();
    QWidget* createSettingsPage();
    QWidget* createHelpPage();
    QWidget* createPolicySummaryPage();
    QWidget* createPlaceholderPage(const QString& title, const QString& subtitle);
    QWidget* createStatCard(const QString& title, const QString& number, const QString& description, const QString& iconResource);
    QWidget* createCard(const QString& title, QWidget* body, QWidget* headerRight = nullptr);
    void registerHidNotifications();
    void enforceBlockedDeviceHook();
    void replacePage(int pageIndex, QWidget* newPage);
    void refreshLiveDataPages();
    void appendNewLogRows();
    void refreshUsbTable();
    void applyUsbDetailPanel(int row);
    void switchPage(int pageIndex);
    void refreshNavState();

    QStackedWidget* pages_ = nullptr;
    QVector<QPushButton*> navButtons_;
    QVector<QPushButton*> topNavButtons_;
    void* hidNotifyHandle_ = nullptr;
    uint64_t lastPromptFingerprint_ = 0;
    bool blockedDeviceHookActive_ = false;
    int currentPage_ = 0;
    int unknownDeviceAction_ = 0;  // 0=dialog, 1=auto-block, 2=auto-allow
    // Incremented on every DBT_DEVICEREMOVECOMPLETE so background fingerprint
    // threads that complete after a removal can detect they are stale.
    int removalSerial_ = 0;
    QTableWidget* logTable_ = nullptr;
    int lastLogLineCount_ = 0;
    bool dashboardStale_ = false;
    bool usbListStale_ = false;

    // System tray
    QSystemTrayIcon* trayIcon_   = nullptr;
    TrayWidget*      trayWidget_ = nullptr;
    void setupTray();
    void refreshTrayWidget();

    // USB list persistent state
    QTableWidget* usbTable_ = nullptr;
    QList<UsbRowData> usbRows_;
    int usbSelectedRow_ = 0;
    QLabel* usbDetName_ = nullptr;
    QLabel* usbDetMfr_ = nullptr;
    QFrame* usbDetStatusDot_ = nullptr;
    QLabel* usbDetStatusLbl_ = nullptr;
    QLabel* usbDetPolicyLbl_ = nullptr;
    QLabel* usbDetTypeV_ = nullptr;
    QLabel* usbDetVidV_ = nullptr;
    QLabel* usbDetLastV_ = nullptr;
};
