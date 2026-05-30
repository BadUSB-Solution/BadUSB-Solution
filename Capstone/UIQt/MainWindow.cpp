// UTF-8
#include "MainWindow.h"
#include "Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QAction>
#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QProcess>
#include <QMenu>
#include <QMessageBox>
#include <QStringConverter>
#include <QTextStream>
#include <QRegularExpression>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFrame>
#include <QFont>
#include <QFontDatabase>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QAbstractItemView>
#include <QHash>
#include <QList>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPixmap>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSize>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QSettings>
#include <QStringList>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QDesktopServices>
#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbt.h>
#include <hidsdi.h>
#endif

#include "TrayWidget.h"
#include "app_paths.h"
#include "hid_fingerprint.h"
#include "hid_scan.h"
#include "compound_scorer.h"
#include "hotkey_scorer.h"
#include "key_decoder.h"
#include "keyboard_hook.h"
#include "logger.h"
#include "policy.h"
#include "settings.h"
#include "device_blocker.h"
#include "shell_detector.h"
#include "threat_matcher.h"
#include "typing_analyzer.h"
#include "url_classifier.h"

#include <QPointer>
#include <QSystemTrayIcon>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <chrono>
#include <thread>
#include <utility>
#include <vector>

#ifdef Q_OS_WIN
static constexpr UINT WM_APP_NEWLOG = WM_APP + 1;
#endif

// Tracks the currently open input buffer monitor dialog and the device being analysed.
// Both are written/read only on the main thread, so no lock is needed.
static QPointer<QDialog>      g_activeBufferDlg;
static std::shared_ptr<UsbItem> g_dlgSharedDevice;
static QVector<double>        g_threatScoreTrend;
static QVector<QPair<double, bool>> g_inputSpeedTrend;

namespace
{
    // Translation helper for use inside this anonymous namespace.
    // tr() requires a Q_OBJECT class context; this routes through the
    // "MainWindow" translation context so strings share that .ts catalog.
    inline QString trMW(const char* s) {
        return QCoreApplication::translate("MainWindow", s);
    }

    enum class FontRole { Regular, Medium, SemiBold, Bold };

    QFont pretendardFont(FontRole role);

    struct LiveAnalysisSnapshot {
        bool hasInput = false;
        int keyDownCount = 0;
        double averageIntervalMs = 0.0;
        KeyDecoder::DecodeResult decoded;
        ShellDetector::Result shell{};
        ThreatMatcher::ThreatResult threat{};
        HotkeyScorer::HotkeyResult hotkey{};
        TypingAnalyzer::Result typing{};
        UrlClassifier::Result urls{};
        double typingScore = 0.0;
        double urlScore = 0.0;
        double keysPerSecond = 0.0;
        CompoundScorer::Verdict verdict{};
    };

    bool confirmPolicyChange(QWidget* parent, const QString& deviceName, bool allow)
    {
        const QString name = deviceName.trimmed().isEmpty()
            ? trMW("선택한 장치")
            : deviceName.trimmed();

        QDialog dlg(parent);
        dlg.setModal(true);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg.setFixedWidth(438);
        dlg.setObjectName("confirmPolicyDialog");
        const auto& t = UiTheme::tokens();
        dlg.setStyleSheet(QString(
            "QDialog#confirmPolicyDialog { background: %Surface%; border: 1px solid %Border%; border-radius: 12px; }"
            "QLabel#confirmTitle { color: %TextDark%; font-size: 17px; font-weight: 700; background: transparent; }"
            "QLabel#confirmSubtitle { color: %Subtext%; font-size: 12px; background: transparent; }"
            "QLabel#confirmDevice { color: %TextDark%; font-size: 14px; font-weight: 600; background: %SurfaceAlt%;"
            " border: 1px solid %Border%; border-radius: 8px; padding: 10px 12px; }"
            "QLabel#confirmBody { color: %Subtext%; font-size: 13px; line-height: 150%; background: transparent; }"
            "QPushButton#confirmClose { color: %TextMuted%; background: transparent; border: none; font-size: 18px; }"
            "QPushButton#confirmClose:hover { color: %TextDark%; }"
            "QPushButton#confirmCancel { color: %TextMid%; background: %Surface%; border: 1px solid %BorderInput%;"
            " border-radius: 8px; padding: 9px 18px; font-size: 13px; font-weight: 600; }"
            "QPushButton#confirmCancel:hover { background: %SurfaceAlt%; border-color: %TextMuted%; }"
            "QPushButton#confirmAction { color: #FFFFFF; background: %Action%; border: none;"
            " border-radius: 8px; padding: 9px 22px; font-size: 13px; font-weight: 700; }"
            "QPushButton#confirmAction:hover { background: %ActionHover%; }"
        )
        .replace(QStringLiteral("%Surface%"), QLatin1String(t.White))
        .replace(QStringLiteral("%Border%"), QLatin1String(t.Border))
        .replace(QStringLiteral("%TextDark%"), QLatin1String(t.TextDark))
        .replace(QStringLiteral("%Subtext%"), QLatin1String(t.Subtext))
        .replace(QStringLiteral("%SurfaceAlt%"), QLatin1String(t.Surface))
        .replace(QStringLiteral("%TextMuted%"), QLatin1String(t.TextMuted))
        .replace(QStringLiteral("%TextMid%"), QLatin1String(t.TextMid))
        .replace(QStringLiteral("%BorderInput%"), QLatin1String(t.BorderInput))
        .replace(QStringLiteral("%Action%"), allow ? QLatin1String(t.Primary) : QLatin1String(t.DangerAction))
        .replace(QStringLiteral("%ActionHover%"), allow ? QLatin1String(t.PrimaryHover) : QLatin1String(t.DangerHover)));

        auto* root = new QVBoxLayout(&dlg);
        root->setContentsMargins(22, 20, 22, 20);
        root->setSpacing(16);

        auto* header = new QHBoxLayout;
        header->setSpacing(12);

        auto* iconWrap = new QFrame;
        iconWrap->setFixedSize(42, 42);
        iconWrap->setStyleSheet(QString(
            "QFrame { background: %1; border: 1px solid %2; border-radius: 10px; }"
        ).arg(allow ? t.PrimarySoft : t.DangerSoft,
              allow ? t.PrimarySoftBorder : t.DangerBorder));
        auto* iconLayout = new QHBoxLayout(iconWrap);
        iconLayout->setContentsMargins(0, 0, 0, 0);
        auto* icon = new QLabel;
        icon->setFixedSize(20, 20);
        icon->setPixmap(QIcon(allow ? ":/icons/status-allowed.svg"
                                    : ":/icons/status-blocked.svg").pixmap(20, 20));
        iconLayout->addWidget(icon, 0, Qt::AlignCenter);
        header->addWidget(iconWrap);

        auto* titleCol = new QVBoxLayout;
        titleCol->setSpacing(3);
        auto* title = new QLabel(allow ? trMW("장치 허용 확인")
                                       : trMW("장치 차단 확인"));
        title->setObjectName("confirmTitle");
        auto* subtitle = new QLabel(allow ? trMW("신뢰 목록에 등록하기 전에 한 번 더 확인합니다.")
                                          : trMW("차단 정책을 적용하기 전에 한 번 더 확인합니다."));
        subtitle->setObjectName("confirmSubtitle");
        titleCol->addWidget(title);
        titleCol->addWidget(subtitle);
        header->addLayout(titleCol, 1);

        auto* closeBtn = new QPushButton(QStringLiteral("×"));
        closeBtn->setObjectName("confirmClose");
        closeBtn->setFixedSize(28, 28);
        closeBtn->setCursor(Qt::PointingHandCursor);
        header->addWidget(closeBtn, 0, Qt::AlignTop);
        root->addLayout(header);

        auto* deviceLabel = new QLabel(name);
        deviceLabel->setObjectName("confirmDevice");
        deviceLabel->setWordWrap(true);
        root->addWidget(deviceLabel);

        auto* body = new QLabel(allow
            ? trMW("허용된 장치는 이후 연결 시 키보드 후킹과 입력 차단이 적용되지 않습니다.\n신뢰할 수 있는 장치인지 확인한 뒤 진행하세요.")
            : trMW("차단된 장치는 블랙리스트에 등록되며 이후 연결 시 입력 차단 상태가 적용됩니다.\n현재 사용 중인 정상 장치가 아닌지 확인한 뒤 진행하세요."));
        body->setObjectName("confirmBody");
        body->setWordWrap(true);
        root->addWidget(body);

        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancelBtn = new QPushButton(trMW("취소"));
        cancelBtn->setObjectName("confirmCancel");
        cancelBtn->setCursor(Qt::PointingHandCursor);
        auto* actionBtn = new QPushButton(allow ? trMW("허용")
                                                : trMW("차단"));
        actionBtn->setObjectName("confirmAction");
        actionBtn->setCursor(Qt::PointingHandCursor);
        buttons->addWidget(cancelBtn);
        buttons->addWidget(actionBtn);
        root->addLayout(buttons);

        bool accepted = false;
        QObject::connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        QObject::connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
        QObject::connect(actionBtn, &QPushButton::clicked, &dlg, [&] {
            accepted = true;
            dlg.accept();
        });

        dlg.exec();
        return accepted;
    }

    QString reenableAllowedDevice(const UsbItem& item, const std::wstring& source)
    {
        if (item.vid == 0 && item.pid == 0) {
            Logger::Add(L"[Qt] " + source + L" re-enable skipped: missing VID/PID");
            return trMW("허용 목록에 저장했습니다. VID/PID 정보가 없어 장치 재활성화는 건너뛰었습니다.");
        }

        std::wstring enableDetail;
        const DeviceBlocker::EnableResult enableResult =
            DeviceBlocker::EnableByVidPid(item.vid, item.pid, &enableDetail);
        Logger::Add(L"[Qt] " + source + L" re-enable result=" +
            std::to_wstring(static_cast<int>(enableResult)) + L", " + enableDetail);

        switch (enableResult) {
        case DeviceBlocker::EnableResult::Enabled:
            return trMW("허용 목록에 저장하고 장치 재활성화를 요청했습니다.");
        case DeviceBlocker::EnableResult::NotFound:
            return trMW("허용 목록에 저장했습니다. 현재 재활성화할 장치를 찾지 못했습니다.");
        case DeviceBlocker::EnableResult::Failed:
        default:
            return trMW("허용 목록에 저장했지만 장치 재활성화 요청은 실패했습니다.");
        }
    }

    QString blockCurrentDevice(const UsbItem& item, const std::wstring& source)
    {
        if (item.devicePath.empty()) {
            Logger::Add(L"[Qt] " + source + L" block skipped: missing device path");
            return trMW("차단 목록에 저장했습니다. 장치 경로가 없어 현재 연결 차단은 건너뛰었습니다.");
        }

        std::wstring blockDetail;
        const DeviceBlocker::BlockResult blockResult =
            DeviceBlocker::BlockByDevicePath(item.devicePath, &blockDetail);
        Logger::Add(L"[Qt] " + source + L" block result=" +
            std::to_wstring(static_cast<int>(blockResult)) + L", " + blockDetail);

        switch (blockResult) {
        case DeviceBlocker::BlockResult::Disabled:
            return trMW("차단 목록에 저장하고 현재 장치를 비활성화했습니다.");
        case DeviceBlocker::BlockResult::Ejected:
            return trMW("차단 목록에 저장하고 현재 장치 제거를 요청했습니다.");
        case DeviceBlocker::BlockResult::NotFound:
            return trMW("차단 목록에 저장했지만 현재 장치 노드를 찾지 못했습니다.");
        case DeviceBlocker::BlockResult::Failed:
        default:
            return trMW("차단 목록에 저장했지만 현재 장치 차단 요청은 실패했습니다. 관리자 권한으로 실행했는지 확인하세요.");
        }
    }

    QString scoreText(double score, double maxScore = 10.0)
    {
        return QString("%1 / %2")
            .arg(score, 0, 'f', 1)
            .arg(maxScore, 0, 'f', 1);
    }

    QString levelText(CompoundScorer::Level level)
    {
        switch (level) {
        case CompoundScorer::SAFE:   return QStringLiteral("SAFE");
        case CompoundScorer::NORMAL: return QStringLiteral("NORMAL");
        case CompoundScorer::WARN:   return QStringLiteral("WARN");
        case CompoundScorer::DANGER: return QStringLiteral("DANGER");
        default:                     return QStringLiteral("-");
        }
    }

    struct PageHelpContent {
        QString title;
        QString summary;
        QStringList steps;
    };

    PageHelpContent helpContentForPage(int page)
    {
        switch (page) {
        case 0:
            return {
                trMW("대시보드 도움말"),
                trMW("현재 보호 상태, 최근 이벤트, 연결 장치와 운영 정책을 한 화면에서 확인합니다."),
                {
                    trMW("상단 카드는 허용/차단/승인 대기/보안 이벤트 수를 요약합니다."),
                    trMW("보호 상태 개요는 주요 보안 모듈의 동작 여부를 보여줍니다."),
                    trMW("운영 정책 요약에서 미등록 장치 처리 방식과 분석 임계값을 확인합니다.")
                }
            };
        case 1:
            return {
                trMW("USB 목록 도움말"),
                trMW("탐지된 HID 장치를 확인하고 허용 또는 차단 정책을 적용합니다."),
                {
                    trMW("장치를 선택하면 오른쪽 상세 정보에서 VID/PID와 정책 상태를 확인합니다."),
                    trMW("신뢰 가능한 장치는 허용 목록에 등록합니다."),
                    trMW("의심 장치는 차단 목록에 등록하고 필요하면 장치 연결을 종료합니다.")
                }
            };
        case 2:
            return {
                trMW("로그 도움말"),
                trMW("장치 탐지, 정책 변경, 차단 이벤트 기록을 확인합니다."),
                {
                    trMW("검색과 필터로 필요한 이벤트만 좁혀봅니다."),
                    trMW("차단 또는 허용 이벤트는 정책 적용 여부를 확인하는 기준이 됩니다."),
                    trMW("실제 테스트 후 이벤트가 없으면 장치가 HID로 정상 인식됐는지 먼저 확인합니다.")
                }
            };
        case 3:
            return {
                trMW("행위 분석 도움말"),
                trMW("입력 패턴, 명령 실행, URL, 단축키 조합을 종합해 위험도를 분석합니다."),
                {
                    trMW("탐지 기준별 위험도는 각 알고리즘 점수를 0-10 범위로 표시합니다."),
                    trMW("입력 속도 그래프는 자동 입력으로 의심되는 패턴을 확인하는 용도입니다."),
                    trMW("위험 점수가 임계값을 넘으면 차단 권장 상태로 판단합니다.")
                }
            };
        case 4:
            return {
                trMW("설정 도움말"),
                trMW("미등록 장치 처리 방식과 행위 분석 임계값 같은 운영 정책을 조정합니다."),
                {
                    trMW("자동 차단은 테스트 전 관리자 권한 실행이 필요합니다."),
                    trMW("임계값을 낮추면 민감하게 탐지하고, 높이면 오탐 가능성이 줄어듭니다."),
                    trMW("저장 후 대시보드 정책 요약에 반영되는지 확인합니다.")
                }
            };
        case 5:
        default:
            return {
                trMW("도움말"),
                trMW("현재 화면의 주요 사용 흐름을 안내합니다."),
                {
                    trMW("각 페이지에서 F1을 누르면 해당 화면의 빠른 도움말이 열립니다."),
                    trMW("상세 설명은 도움말 페이지의 항목별 문서를 확인합니다."),
                    trMW("팝오버 바깥을 클릭하면 도움말이 닫힙니다.")
                }
            };
        }
    }

    void showPageHelpPopover(QWidget* parent, int page)
    {
        if (!parent)
            return;

        const PageHelpContent content = helpContentForPage(page);
        const auto& t = UiTheme::tokens();
        auto* popover = new QDialog(parent, Qt::Popup | Qt::FramelessWindowHint);
        popover->setAttribute(Qt::WA_DeleteOnClose);
        popover->setFixedWidth(390);
        popover->setStyleSheet(QString(
            "QDialog { background: %1; border: 1px solid %2; border-radius: 12px; }"
            "QLabel { background: transparent; border: none; }"
        ).arg(t.White, t.Border));

        auto* root = new QVBoxLayout(popover);
        root->setContentsMargins(18, 16, 18, 16);
        root->setSpacing(12);

        auto* header = new QHBoxLayout;
        header->setSpacing(10);
        auto* icon = new QLabel("?");
        icon->setFixedSize(28, 28);
        icon->setAlignment(Qt::AlignCenter);
        icon->setFont(pretendardFont(FontRole::Bold));
        icon->setStyleSheet(QString("QLabel { color: %1; background: %2; border: 1px solid %3;"
                            " border-radius: 14px; font-size: 15px; }")
                            .arg(t.Primary, t.PrimarySoft, t.PrimarySoftBorder));
        auto* title = new QLabel(content.title);
        title->setFont(pretendardFont(FontRole::SemiBold));
        title->setStyleSheet(QString("font-size: 15px; color: %1;").arg(t.Text));
        header->addWidget(icon);
        header->addWidget(title, 1);
        auto* closeBtn = new QPushButton("×");
        closeBtn->setFixedSize(28, 28);
        closeBtn->setCursor(Qt::PointingHandCursor);
        closeBtn->setStyleSheet(QString("QPushButton { color: %1; background: %2; border: none;"
                                " border-radius: 14px; font-size: 16px; }"
                                "QPushButton:hover { background: %3; color: %4; }")
                                .arg(t.Subtext, t.Surface, t.Border, t.Text));
        QObject::connect(closeBtn, &QPushButton::clicked, popover, &QDialog::close);
        header->addWidget(closeBtn);
        root->addLayout(header);

        auto* summary = new QLabel(content.summary);
        summary->setWordWrap(true);
        summary->setFont(pretendardFont(FontRole::Regular));
        summary->setStyleSheet(QString("font-size: 12px; color: %1; line-height: 150%;").arg(t.Subtext));
        root->addWidget(summary);

        for (int i = 0; i < content.steps.size(); ++i) {
            auto* row = new QWidget;
            row->setObjectName("helpStepRow");
            row->setStyleSheet(QString("QWidget#helpStepRow { background: %1; border: 1px solid %2; border-radius: 8px; }"
                               "QWidget#helpStepRow QLabel { background: transparent; border: none; }")
                               .arg(t.Surface, t.Border));
            auto* rowL = new QHBoxLayout(row);
            rowL->setContentsMargins(10, 9, 10, 9);
            rowL->setSpacing(10);
            auto* num = new QLabel(QString::number(i + 1));
            num->setFixedSize(20, 20);
            num->setAlignment(Qt::AlignCenter);
            num->setFont(pretendardFont(FontRole::SemiBold));
            num->setStyleSheet(QString("QLabel { color: %1; background: %2; border-radius: 10px; font-size: 11px; }")
                               .arg(t.Primary, t.PrimarySoft));
            auto* text = new QLabel(content.steps[i]);
            text->setWordWrap(true);
            text->setFont(pretendardFont(FontRole::Medium));
            text->setStyleSheet(QString("font-size: 12px; color: %1;").arg(t.TextMid));
            rowL->addWidget(num, 0, Qt::AlignTop);
            rowL->addWidget(text, 1);
            root->addWidget(row);
        }

        auto* hint = new QLabel("F1 · 현재 화면 도움말");
        hint->setFont(pretendardFont(FontRole::Regular));
        hint->setStyleSheet(QString("font-size: 11px; color: %1;").arg(t.TextMuted));
        root->addWidget(hint, 0, Qt::AlignRight);

        const QPoint globalTopRight = parent->mapToGlobal(QPoint(parent->width() - popover->width() - 32, 88));
        popover->move(globalTopRight);
        popover->show();
    }

    bool notifyOnRiskEnabled()
    {
        return Policy::GetSettings().notifyOnRisk;
    }

    bool notificationSoundEnabled()
    {
        return Policy::GetSettings().notificationSound;
    }

    int notificationDurationMs()
    {
        return std::clamp(Policy::GetSettings().notificationDurationSec, 1, 60) * 1000;
    }

    void showRiskTrayMessage(QSystemTrayIcon* trayIcon, const QString& title,
                             const QString& message, QSystemTrayIcon::MessageIcon icon)
    {
        if (!trayIcon)
            return;
        if (notificationSoundEnabled())
            QApplication::beep();
        trayIcon->showMessage(title, message, icon, notificationDurationMs());
    }

#ifdef Q_OS_WIN
    void setWindowsAutoStart(bool enabled)
    {
        // HKCU\Run cannot launch apps that require UAC elevation (requireAdministrator manifest).
        // Use Task Scheduler with HIGHEST_AVAILABLE privilege instead — no UAC prompt at login.
        const QString appName = QStringLiteral("Abang-All");
        const QString exe = QDir::toNativeSeparators(QApplication::applicationFilePath());

        if (enabled) {
            const QString xml = QStringLiteral(
                "<?xml version=\"1.0\" encoding=\"UTF-16\"?>"
                "<Task version=\"1.2\" xmlns=\"http://schemas.microsoft.com/windows/2004/02/mit/task\">"
                "<Triggers><LogonTrigger><Enabled>true</Enabled></LogonTrigger></Triggers>"
                "<Principals><Principal id=\"Author\">"
                "<LogonType>InteractiveToken</LogonType>"
                "<RunLevel>HighestAvailable</RunLevel>"
                "</Principal></Principals>"
                "<Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>"
                "<DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>"
                "<StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>"
                "<ExecutionTimeLimit>PT0S</ExecutionTimeLimit>"
                "<Priority>7</Priority></Settings>"
                "<Actions Context=\"Author\">"
                "<Exec><Command>\"%1\"</Command></Exec>"
                "</Actions></Task>").arg(exe);

            // Write XML to temp file then import via schtasks
            const QString tmpXml = QDir::tempPath() + QStringLiteral("/abangall_task.xml");
            QFile f(tmpXml);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
                f.write(xml.toUtf8());
                f.close();
            }
            QProcess::execute(QStringLiteral("schtasks"),
                {QStringLiteral("/Create"), QStringLiteral("/TN"), appName,
                 QStringLiteral("/XML"), tmpXml, QStringLiteral("/F")});
            QFile::remove(tmpXml);
        } else {
            QProcess::execute(QStringLiteral("schtasks"),
                {QStringLiteral("/Delete"), QStringLiteral("/TN"), appName,
                 QStringLiteral("/F")});
        }

        // Also clean up any legacy HKCU\Run entry
        QSettings runKey(
            QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
            QSettings::NativeFormat);
        runKey.remove(appName);
    }
#endif

    int currentBufferedKeyDownCount()
    {
        int count = 0;
        const auto keys = KeyboardHook::GetBufferedKeys();
        for (const auto& key : keys) {
            if ((key.flags & LLKHF_UP) == 0)
                ++count;
        }
        return count;
    }

    LiveAnalysisSnapshot buildLiveAnalysisSnapshot()
    {
        LiveAnalysisSnapshot snapshot;
        const auto keys = KeyboardHook::GetBufferedKeys();
        snapshot.hasInput = !keys.empty();
        if (!snapshot.hasInput) {
            snapshot.verdict = CompoundScorer::Combine(0.0, 0.0, 0.0, 0.0);
            return snapshot;
        }

        DWORD prevDownTime = 0;
        double intervalSum = 0.0;
        int intervalCount = 0;
        for (const auto& key : keys) {
            if ((key.flags & LLKHF_UP) != 0)
                continue;
            snapshot.keyDownCount++;
            if (prevDownTime != 0 && key.time >= prevDownTime) {
                intervalSum += static_cast<double>(key.time - prevDownTime);
                intervalCount++;
            }
            prevDownTime = key.time;
        }
        if (intervalCount > 0)
            snapshot.averageIntervalMs = intervalSum / intervalCount;

        snapshot.typing = TypingAnalyzer::Analyze(keys);
        snapshot.typingScore = snapshot.typing.score;
        if (snapshot.typing.meanMs > 0.0)
            snapshot.keysPerSecond = 1000.0 / snapshot.typing.meanMs;
        else if (snapshot.averageIntervalMs > 0.0)
            snapshot.keysPerSecond = 1000.0 / snapshot.averageIntervalMs;

        snapshot.decoded = KeyDecoder::Decode(keys);
        snapshot.urls = UrlClassifier::Classify(snapshot.decoded);
        snapshot.urlScore = snapshot.urls.final_score;
        snapshot.shell = ShellDetector::Detect(snapshot.decoded);
        snapshot.threat = ThreatMatcher::Analyze(snapshot.decoded, static_cast<int>(snapshot.shell.shell));
        snapshot.hotkey = HotkeyScorer::Analyze(snapshot.decoded.hotkeys);
        snapshot.verdict = CompoundScorer::Combine(
            snapshot.threat.session_score,
            snapshot.urlScore,
            snapshot.typingScore,
            snapshot.hotkey.final_score
        );
        return snapshot;
    }

    void appendThreatTrendSample(double score)
    {
        g_threatScoreTrend.append(std::clamp(score, 0.0, 10.0));
        while (g_threatScoreTrend.size() > 36)
            g_threatScoreTrend.removeFirst();
    }

    void appendInputSpeedTrendSample(double keysPerSecond, bool isAnomaly)
    {
        g_inputSpeedTrend.append(qMakePair(std::clamp(keysPerSecond, 0.0, 16.0), isAnomaly));
        while (g_inputSpeedTrend.size() > 30)
            g_inputSpeedTrend.removeFirst();
    }

    QStringList blockReasonLines(const LiveAnalysisSnapshot& snapshot)
    {
        QStringList reasons;
        if (snapshot.typing.classification == TypingAnalyzer::TYPING_AUTOMATED
            || snapshot.typingScore >= 6.0) {
            reasons << trMW("자동 입력으로 의심되는 빠르고 균일한 키 입력이 감지되었습니다.");
        }
        if (!snapshot.threat.commands.empty() || snapshot.threat.session_score >= 3.0) {
            reasons << trMW("실행/명령 입력 패턴이 감지되었습니다.");
        }
        if (!snapshot.hotkey.matches.empty() || snapshot.hotkey.final_score >= 3.0) {
            reasons << trMW("위험 단축키 조합이 포함되어 있습니다.");
        }
        if (!snapshot.urls.items.empty() && snapshot.urlScore > 0.0) {
            reasons << trMW("의심 URL 또는 실행 파일 URL 패턴이 포함되어 있습니다.");
        }
        if (reasons.isEmpty()) {
            reasons << trMW("여러 입력 신호가 조합되어 위험 점수가 상승했습니다.");
        }
        return reasons;
    }

    QString blockReasonHtml(const QStringList& reasons)
    {
        QString html;
        for (const QString& reason : reasons) {
            html += QString(
                "<div style='margin-bottom:4px;'>"
                "<span style='color:%2; font-weight:800;'>•</span>"
                "<span style='color:%3;'>&nbsp;&nbsp;%1</span>"
                "</div>").arg(reason.toHtmlEscaped(),
                              QLatin1String(UiTheme::tokens().Warning),
                              QLatin1String(UiTheme::tokens().SubtextMid));
        }
        return html;
    }

    QString fromWString(const std::wstring& value)
    {
        return QString::fromStdWString(value);
    }

    std::wstring toWString(const QString& value)
    {
        return value.toStdWString();
    }

    QString hex4(uint16_t value)
    {
        return QString("%1").arg(value, 4, 16, QChar('0')).toUpper();
    }

    QString vidPidText(uint16_t vid, uint16_t pid)
    {
        return hex4(vid) + ":" + hex4(pid);
    }

    QString policyTextForItem(const UsbItem& item)
    {
        const Policy::PolicyStatus status = Policy::Evaluate(item);
        if (status == Policy::PolicyStatus::Whitelisted) return "허용";
        if (status == Policy::PolicyStatus::Blacklisted) return "차단";
        return "승인 대기";
    }

    QString nowTimestamp()
    {
        return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    }

    QString nowTime()
    {
        return QDateTime::currentDateTime().toString("HH:mm:ss");
    }

    QString keyNameForRecord(const KeyboardHook::KeyRecord& record)
    {
#ifdef Q_OS_WIN
        LONG lParam = static_cast<LONG>(record.scanCode << 16);
        if (record.flags & LLKHF_EXTENDED)
            lParam |= (1 << 24);

        wchar_t name[128]{};
        if (GetKeyNameTextW(lParam, name, static_cast<int>(std::size(name))) > 0)
            return QString::fromWCharArray(name).toUpper();
#endif
        return QString("VK_%1").arg(record.vkCode, 2, 16, QChar('0')).toUpper();
    }

    QString formatBufferedKeysForView()
    {
        const auto keys = KeyboardHook::GetBufferedKeys();
        if (keys.empty())
            return trMW("입력 대기 중...\n미등록 HID 장치에서 키 입력이 감지되면 이 영역에 실시간으로 표시됩니다.");

        QStringList lines;
        const int start = std::max<int>(0, static_cast<int>(keys.size()) - 200);
        lines.reserve(static_cast<int>(keys.size()) - start + 2);

        // Compute wall-clock base once so timestamps are stable across refreshes.
        // k.time is GetMessageTime() in ms (same epoch as GetTickCount).
        const QDateTime nowWall = QDateTime::currentDateTime();
        const qint64 nowTick   = static_cast<qint64>(GetTickCount());

        DWORD prevDownTime = 0;
        int downIntervals = 0;
        double intervalSum = 0.0;

        for (int i = start; i < static_cast<int>(keys.size()); ++i) {
            const auto& k = keys[i];
            const bool keyUp = (k.flags & LLKHF_UP) != 0;

            // Skip KEY_UP events — they duplicate KEY_DOWN entries and add noise.
            // Timing analysis only uses KEY_DOWN intervals anyway.
            if (keyUp) continue;

            if (prevDownTime != 0 && k.time >= prevDownTime) {
                intervalSum += static_cast<double>(k.time - prevDownTime);
                downIntervals++;
            }
            prevDownTime = k.time;

            const qint64 msOffset = static_cast<qint64>(k.time) - nowTick;
            const QString ts = nowWall.addMSecs(msOffset).toString("yyyy-MM-dd HH:mm:ss");
            lines << QString("%1  KEY_DOWN  VK=0x%2  SC=0x%3  %4")
                .arg(ts)
                .arg(k.vkCode, 2, 16, QChar('0')).toUpper()
                .arg(k.scanCode, 2, 16, QChar('0')).toUpper()
                .arg(keyNameForRecord(k));
        }

        if (downIntervals > 0) {
            const double avgMs = intervalSum / downIntervals;
            if (avgMs < 50.0) {
                lines << QString("[ANOMALY] 비정상 입력 속도 감지: %1ms/key")
                    .arg(avgMs, 0, 'f', 1);
            }
        }

        return lines.join('\n');
    }

    QString formatNormalizedKeysForView()
    {
        const auto keys = KeyboardHook::GetBufferedKeys();
        if (keys.empty())
            return "정규화할 입력이 없습니다.\n미등록 HID 장치에서 키 입력이 감지되면 정규화 결과를 확인할 수 있습니다.";

        const auto decoded = KeyDecoder::Decode(keys);
        QStringList lines;
        lines << "[NORMALIZED INPUT]";

        if (!decoded.fullText.empty()) {
            lines << fromWString(decoded.fullText);
        } else if (!decoded.lines.empty()) {
            for (size_t i = 0; i < decoded.lines.size(); ++i)
                lines << QString("[%1] %2").arg(i + 1).arg(fromWString(decoded.lines[i].text));
        } else {
            lines << "(정규화된 텍스트 없음)";
        }

        if (!decoded.hotkeys.empty()) {
            lines << "";
            lines << "[HOTKEYS]";
            for (const auto& hotkey : decoded.hotkeys) {
                QString line = QString("- %1 @ line %2")
                    .arg(fromWString(hotkey.combo))
                    .arg(static_cast<qulonglong>(hotkey.lineIndex + 1));
                if (hotkey.submenuKey)
                    line += QString(" submenu=%1").arg(QChar(hotkey.submenuKey));
                lines << line;
            }
        }

        if (!decoded.commands.empty()) {
            lines << "";
            lines << "[COMMANDS]";
            for (const auto& command : decoded.commands) {
                QStringList args;
                for (const auto& arg : command.args)
                    args << fromWString(arg);
                lines << QString("- %1 %2").arg(fromWString(command.command), args.join(' ')).trimmed();
            }
        }

        if (!decoded.lines.empty()) {
            lines << "";
            lines << "[URLS]";
            bool hasUrl = false;
            for (const auto& line : decoded.lines) {
                for (const auto& url : line.urls) {
                    hasUrl = true;
                    lines << QString("- %1").arg(fromWString(url.resolved.empty() ? url.raw : url.resolved));
                }
            }
            if (!hasUrl)
                lines << "(감지된 URL 없음)";
        }

        return lines.join('\n');
    }

    QString formatHexKeysForView()
    {
        const auto keys = KeyboardHook::GetBufferedKeys();
        if (keys.empty())
            return trMW("입력 대기 중...\n16진수 형식으로 표시됩니다.");

        QStringList lines;
        const int start = std::max<int>(0, static_cast<int>(keys.size()) - 200);
        lines.reserve(static_cast<int>(keys.size()) - start + 1);

        for (int i = start; i < static_cast<int>(keys.size()); ++i) {
            const auto& k = keys[i];
            const bool keyUp = (k.flags & LLKHF_UP) != 0;
            lines << QString("%1  %2  %3  %4  %5  %6")
                .arg(QString::number(k.time, 16).rightJustified(8, '0').toUpper())
                .arg(keyUp ? "UP  " : "DOWN")
                .arg(QString("VK:%1").arg(k.vkCode, 2, 16, QChar('0')).toUpper())
                .arg(QString("SC:%1").arg(k.scanCode, 2, 16, QChar('0')).toUpper())
                .arg(QString("FL:%1").arg(k.flags, 8, 16, QChar('0')).toUpper())
                .arg(keyNameForRecord(k));
        }

        return lines.join('\n');
    }

    QFont pretendardFont(FontRole role)
    {
        QString family = QStringLiteral("Pretendard");
        if (!QFontDatabase::families().contains(family))
            family = QApplication::font().family();
        if (family.isEmpty())
            family = QStringLiteral("Pretendard");
        QFont f(family);
        f.setStyleStrategy(QFont::PreferAntialias);
        f.setHintingPreference(QFont::PreferNoHinting);
        switch (role) {
        case FontRole::Medium:
            f.setWeight(QFont::Medium);
            break;
        case FontRole::SemiBold:
            f.setWeight(QFont::DemiBold);
            break;
        case FontRole::Bold:
            f.setWeight(QFont::Bold);
            break;
        default:
            f.setWeight(QFont::Normal);
            break;
        }
        return f;
    }

    FontRole labelFontRole(const char* n)
    {
        if (!n) return FontRole::Regular;
        const QString name = QString::fromLatin1(n);
        if (name == "pageTitle" || name == "brandText" || name == "statNumber")
            return FontRole::Bold;
        if (name == "cardTitle" || name.endsWith("Badge") || name.startsWith("policy")
            || name == "roleBadge" || name == "sectionHeading")
            return FontRole::SemiBold;
        if (name == "notificationBadge" || name == "systemInfoValue")
            return FontRole::Bold;
        return FontRole::Regular;
    }

    QLabel* label(const QString& text, const char* objectName = nullptr)
    {
        auto* item = new QLabel(text);
        item->setFont(pretendardFont(labelFontRole(objectName)));
        if (objectName) item->setObjectName(objectName);
        return item;
    }

    QFrame* cardFrame()
    {
        auto* frame = new QFrame;
        frame->setObjectName("card");
        return frame;
    }

    QLabel* iconLabel(const QString& resource, int size)
    {
        auto* icon = new QLabel;
        icon->setFixedSize(size, size);
        const int pixmapSize = size > 2 ? size - 2 : 1;
        icon->setPixmap(QIcon(resource).pixmap(pixmapSize, pixmapSize));
        icon->setAlignment(Qt::AlignCenter);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        return icon;
    }

    void setCellWidgetBackground(QWidget* widget, const QColor& color)
    {
        if (!widget) return;
        widget->setObjectName("tableCell");
        widget->setProperty("selected", color != UiTheme::color(UiTheme::tokens().White));
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
        widget->update();
    }

    QColor cellBackground()
    {
        return UiTheme::color(UiTheme::tokens().White);
    }

    QColor selectedCellBackground()
    {
        return cellBackground();
    }

    QSize policyBadgeSize(const QString& /*text*/)
    {
        // 허용/차단/승인 대기/기록/확인 필요 모두 동일한 너비로 통일
        return QSize(76, 26);
    }

    QString themedLabelStyle(const char* color, int size = 14)
    {
        return QString("QLabel { color: %1; font-size: %2px; border: none; background: transparent; }")
            .arg(QLatin1String(color))
            .arg(size);
    }

    void configureTable(QTableWidget* table)
    {
        table->setFont(pretendardFont(FontRole::Regular));
        table->horizontalHeader()->setFont(pretendardFont(FontRole::SemiBold));
        table->setShowGrid(false);
        table->setAlternatingRowColors(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->verticalHeader()->setVisible(false);
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        table->horizontalHeader()->setFixedHeight(44);
        table->verticalHeader()->setDefaultSectionSize(52);
        table->setFrameShape(QFrame::NoFrame);
    }

    QWidget* makeBadgeCell(const QString& text, const char* badgeName)
    {
        auto* badge = new QLabel(text);
        badge->setObjectName(badgeName);
        badge->setAlignment(Qt::AlignCenter);
        badge->setFont(pretendardFont(FontRole::SemiBold));
        badge->setFixedSize(policyBadgeSize(text));
        badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* container = new QWidget;
        container->setAttribute(Qt::WA_TransparentForMouseEvents);
        setCellWidgetBackground(container, cellBackground());
        auto* l = new QHBoxLayout(container);
        l->setContentsMargins(6, 0, 6, 0);
        l->addWidget(badge, 0, Qt::AlignLeft | Qt::AlignVCenter);
        l->addStretch();
        return container;
    }

    void applyCompactPolicyBadge(QLabel* badge, const QString& text)
    {
        if (!badge) return;

        const bool isAllow = text == QStringLiteral("허용");
        const bool isBlock = text == QStringLiteral("차단");

        const QString display = isAllow ? trMW("허용")
                              : isBlock ? trMW("차단")
                              : trMW("승인 대기");
        badge->setText(display);
        badge->setObjectName(isAllow ? "successBadge" : (isBlock ? "dangerBadge" : "warningBadge"));
        badge->setAlignment(Qt::AlignCenter);
        badge->setFixedSize(policyBadgeSize(display));
        badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        badge->style()->unpolish(badge);
        badge->style()->polish(badge);
    }

    QFrame* makeDivider()
    {
        auto* div = new QFrame;
        div->setObjectName("divider");
        return div;
    }

    QWidget* makeUsbStatusCell(const QString& status)
    {
        auto* w = new QWidget;
        w->setAttribute(Qt::WA_TransparentForMouseEvents);
        setCellWidgetBackground(w, cellBackground());
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(10, 0, 6, 0);
        l->setSpacing(6);
        auto* dot = new QFrame;
        dot->setObjectName("usbStatusDot");
        const bool connected = (status == "연결됨");
        dot->setProperty("connected", connected);
        dot->setFixedSize(8, 8);
        auto* txt = new QLabel(connected ? trMW("연결됨") : trMW("연결 안됨"));
        txt->setObjectName("usbStatusText");
        txt->setAttribute(Qt::WA_TransparentForMouseEvents);
        txt->setFont(pretendardFont(FontRole::Regular));
        l->addWidget(dot);
        l->addWidget(txt);
        l->addStretch();
        return w;
    }

    QWidget* makeUsbTextCell(const QString& text, const QString& color, const QFont& font, int leftPadding = 10)
    {
        auto* w = new QWidget;
        w->setAttribute(Qt::WA_TransparentForMouseEvents);
        setCellWidgetBackground(w, cellBackground());
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(leftPadding, 0, 8, 0);
        l->setSpacing(0);
        auto* txt = new QLabel(text);
        const auto& t = UiTheme::tokens();
        txt->setObjectName(color == QLatin1String(t.Text) ? "usbCellText" : "usbCellMutedText");
        txt->setAttribute(Qt::WA_TransparentForMouseEvents);
        txt->setFont(font);
        l->addWidget(txt);
        l->addStretch();
        return w;
    }

    // Search box with leading icon
    QWidget* makeSearchBox(const QString& placeholder, int w = 256)
    {
        auto* box = new QWidget;
        box->setObjectName("searchBox");
        box->setFixedHeight(38);
        if (w > 0) box->setFixedWidth(w);
        auto* bl = new QHBoxLayout(box);
        bl->setContentsMargins(10, 0, 10, 0);
        bl->setSpacing(6);
        auto* icon = new QLabel;
        icon->setFixedSize(16, 16);
        icon->setPixmap(QIcon(":/icons/action-search.svg").pixmap(16, 16));
        bl->addWidget(icon);
        auto* input = new QLineEdit;
        input->setObjectName("searchInput");
        input->setPlaceholderText(placeholder);
        input->setFont(pretendardFont(FontRole::Regular));
        input->setFrame(false);
        bl->addWidget(input, 1);
        return box;
    }

    // ---- Toggle switch pill widget ----
    class ToggleSwitchWidget : public QWidget
    {
        bool on_;
    public:
        explicit ToggleSwitchWidget(bool checked = true, QWidget* parent = nullptr)
            : QWidget(parent), on_(checked)
        {
            setFixedSize(44, 24);
            setCursor(Qt::PointingHandCursor);
        }
        bool isChecked() const { return on_; }
        void setChecked(bool v) { on_ = v; update(); }
    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(on_ ? QColor(UiTheme::tokens().Primary) : QColor(UiTheme::tokens().BorderInput));
            p.drawRoundedRect(rect(), 12, 12);
            // White thumb circle
            const int cx = on_ ? width() - 13 : 13;
            p.setBrush(QColor("#FFFFFF"));
            p.drawEllipse(QPoint(cx, height() / 2), 9, 9);
        }
        void mousePressEvent(QMouseEvent*) override
        {
            on_ = !on_;
            update();
        }
    };

    class DotPulseLoader : public QWidget
    {
        QString color_ = "#60A5FA";
        int phase_ = 0;
        bool animated_ = true;

    public:
        explicit DotPulseLoader(QWidget* parent = nullptr) : QWidget(parent)
        {
            setFixedSize(48, 18);
            setAttribute(Qt::WA_TransparentForMouseEvents);
        }

        void setLoading(const QString& color, int phase)
        {
            color_ = color;
            phase_ = phase;
            animated_ = true;
            update();
        }

        void setComplete(const QString& color)
        {
            color_ = color;
            animated_ = false;
            update();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);

            const QColor base(color_);
            const int phase = phase_ % 6;
            const QVector<QPointF> centers = {
                QPointF(8, height() / 2.0),
                QPointF(24, height() / 2.0),
                QPointF(40, height() / 2.0)
            };
            const double wave[6] = {0.0, 0.35, 0.95, 0.55, 0.15, 0.0};

            for (int i = 0; i < centers.size(); ++i) {
                QColor dot = base;
                double radius = 3.8;
                double yOffset = 0.0;
                double opacity = animated_ ? 0.36 : 0.72;

                if (animated_) {
                    const int localPhase = (phase + 6 - i * 2) % 6;
                    const double lift = wave[localPhase];
                    radius = 3.8 + lift * 1.2;
                    yOffset = -5.0 * lift;
                    opacity = 0.36 + lift * 0.42;
                }

                dot.setAlphaF(opacity);
                p.setBrush(dot);
                QPointF center = centers[i];
                center.setY(center.y() + yOffset);
                p.drawEllipse(center, radius, radius);
            }
        }
    };

    // ---- Input speed trend widget ----
    class BarChartWidget : public QWidget
    {
    public:
        explicit BarChartWidget(QWidget* parent = nullptr) : QWidget(parent)
        {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            data_ = g_inputSpeedTrend;
        }

        void setData(const QVector<QPair<double, bool>>& data)
        {
            data_ = data;
            update();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            const QVector<QPair<double, bool>> data = data_;

            const double maxV = 20.0;
            const int lm = 42, rm = 20, tm = 14, bm = 10;
            const int cw = width() - lm - rm;
            const int ch = height() - tm - bm;

            const QStringList yLabels = {"0", "5", "10", "15", "20"};
            for (int i = 0; i < 5; ++i) {
                const double ratio = i / 4.0;
                const int y = tm + ch - static_cast<int>(ratio * ch);
                p.setPen(QPen(UiTheme::color(UiTheme::tokens().ChartGrid), 1, Qt::DashLine));
                p.drawLine(lm, y, lm + cw, y);
                p.setPen(UiTheme::color(UiTheme::tokens().ChartAxis));
                QFont f("Inter", 9);
                p.setFont(f);
                p.drawText(0, y - 7, lm - 4, 14, Qt::AlignRight | Qt::AlignVCenter, yLabels[i]);
            }

            if (data.isEmpty()) {
                p.setPen(UiTheme::color(UiTheme::tokens().ChartEmpty));
                QFont emptyFont = pretendardFont(FontRole::Medium);
                emptyFont.setPointSize(10);
                p.setFont(emptyFont);
                p.drawText(lm, tm, cw, ch, Qt::AlignCenter, trMW("입력 대기 중"));
                return;
            }

            const int thresholdY = tm + ch - static_cast<int>((8.0 / maxV) * ch);
            p.setPen(QPen(UiTheme::color(UiTheme::tokens().ChartAxis), 1.3, Qt::DashLine));
            p.drawLine(lm, thresholdY, lm + cw, thresholdY);

            const int n = data.size();
            if (n > 0) {
                QVector<QPointF> pts;
                pts.reserve(n);
                for (int i = 0; i < n; ++i) {
                    const double x = n == 1 ? lm + cw : lm + (static_cast<double>(i) / (n - 1)) * cw;
                    const double y = tm + ch - (std::clamp(data[i].first, 0.0, maxV) / maxV) * ch;
                    pts.append(QPointF(x, y));
                }

                QPainterPath fillPath;
                fillPath.moveTo(pts.first());
                for (int i = 1; i < pts.size(); ++i)
                    fillPath.lineTo(pts[i]);
                fillPath.lineTo(pts.last().x(), tm + ch);
                fillPath.lineTo(pts.first().x(), tm + ch);
                fillPath.closeSubpath();
                p.fillPath(fillPath, QColor(11, 115, 246, 26));

                QPainterPath linePath;
                linePath.moveTo(pts.first());
                for (int i = 1; i < pts.size(); ++i)
                    linePath.lineTo(pts[i]);
                p.setPen(QPen(QColor(UiTheme::tokens().ChartRisk), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPath(linePath);

                for (int i = 0; i < pts.size(); ++i) {
                    const bool anom = data[i].second;
                    p.setPen(QPen(UiTheme::color(UiTheme::tokens().White), 2));
                    p.setBrush(QColor(anom ? "#7C3AED" : UiTheme::tokens().ChartRisk));
                    p.drawEllipse(pts[i], anom ? 4.5 : 3.5, anom ? 4.5 : 3.5);
                }
            }
        }

    private:
        QVector<QPair<double, bool>> data_;
    };

    // ---- Line chart widget (위험 점수 추이) ----
    class LineChartWidget : public QWidget
    {
    public:
        explicit LineChartWidget(QWidget* parent = nullptr) : QWidget(parent)
        {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
            data_ = g_threatScoreTrend;
        }

        void setData(const QVector<double>& data)
        {
            data_ = data;
            update();
        }
    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            const QVector<double> data = data_;
            const double maxV = 10.0;
            const int lm = 48, rm = 18, tm = 12, bm = 10;
            const int cw = width() - lm - rm;
            const int ch = height() - tm - bm;

            // Grid + Y labels
            const QStringList yLbls = {"0.0", "2.5", "5.0", "7.5", "10.0"};
            for (int i = 0; i < yLbls.size(); ++i) {
                const double ratio = i / static_cast<double>(yLbls.size() - 1);
                const int y = tm + ch - static_cast<int>(ratio * ch);
                p.setPen(QPen(UiTheme::color(UiTheme::tokens().ChartGrid), 1));
                p.drawLine(lm, y, lm + cw, y);
                p.setPen(UiTheme::color(UiTheme::tokens().ChartAxis));
                QFont f("Inter", 9); p.setFont(f);
                p.drawText(0, y - 7, lm - 4, 14, Qt::AlignRight | Qt::AlignVCenter, yLbls[i]);
            }

            // Threshold dashed line at 8.0
            {
                const int ty = tm + ch - static_cast<int>((8.0 / maxV) * ch);
                p.setPen(QPen(QColor(UiTheme::tokens().ChartThreshold), 1.5, Qt::DashLine));
                p.drawLine(lm, ty, lm + cw, ty);
            }

            // Build polyline points
            const int n = data.size();
            QVector<QPointF> pts;
            pts.reserve(n);
            for (int i = 0; i < n; ++i) {
                const double x = lm + (n > 1 ? (static_cast<double>(i) / (n - 1)) * cw : 0.0);
                const double y = tm + ch - (data[i] / maxV) * ch;
                pts.append(QPointF(x, y));
            }

            // Draw red line
            p.setPen(QPen(QColor(UiTheme::tokens().ChartRisk), 2));
            for (int i = 1; i < pts.size(); ++i)
                p.drawLine(pts[i-1], pts[i]);

            // Crossing dot near index 15 (14:05)
            if (!pts.isEmpty() && *std::max_element(data.cbegin(), data.cend()) > 0.05) {
                const int cpIndex = std::distance(data.cbegin(), std::max_element(data.cbegin(), data.cend()));
                const QPointF& cp = pts[std::clamp(cpIndex, 0, static_cast<int>(pts.size()) - 1)];
                p.setPen(QPen(UiTheme::color(UiTheme::tokens().White), 2));
                p.setBrush(QColor(UiTheme::tokens().ChartThreshold));
                p.drawEllipse(cp, 5.0, 5.0);
            }

        }

    private:
        QVector<double> data_;
    };

    class RiskBreakdownWidget : public QWidget
    {
    public:
        explicit RiskBreakdownWidget(QWidget* parent = nullptr) : QWidget(parent)
        {
            setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        }

        void setScores(double command, double url, double typing, double hotkey)
        {
            scores_ = {
                {trMW("명령어"), std::clamp(command, 0.0, 10.0), QColor("#2563C7")},
                {QStringLiteral("URL"), std::clamp(url, 0.0, 10.0), QColor("#7C3AED")},
                {trMW("입력 속도"), std::clamp(typing, 0.0, 10.0), QColor("#0891B2")},
                {trMW("핫키"), std::clamp(hotkey, 0.0, 10.0), QColor("#DB2777")},
            };
            update();
        }

        double averageScore() const
        {
            if (scores_.isEmpty())
                return 0.0;
            double sum = 0.0;
            for (const auto& score : scores_)
                sum += score.value;
            return sum / scores_.size();
        }

    protected:
        void paintEvent(QPaintEvent*) override
        {
            QPainter p(this);
            p.setRenderHint(QPainter::Antialiasing);

            const int leftLabelW = 78;
            const int rightValueW = 54;
            const int top = 18;
            const int rowH = 34;
            const int gap = 10;
            const int barH = 12;
            const int barX = leftLabelW + 12;
            const int barW = width() - barX - rightValueW - 18;

            if (scores_.isEmpty()) {
                p.setPen(UiTheme::color(UiTheme::tokens().ChartEmpty));
                p.setFont(QFont("Inter", 10));
                p.drawText(rect(), Qt::AlignCenter, trMW("분석 대기 중..."));
                return;
            }

            for (int i = 0; i < scores_.size(); ++i) {
                const auto& item = scores_[i];
                const int y = top + i * (rowH + gap);
                const int barY = y + 12;

                QFont labelFont = pretendardFont(FontRole::Bold);
                labelFont.setPointSize(10);
                p.setPen(UiTheme::color(UiTheme::tokens().SubtextMid));
                p.setFont(labelFont);
                p.drawText(0, y, leftLabelW, rowH, Qt::AlignRight | Qt::AlignVCenter, item.label);

                p.setPen(Qt::NoPen);
                p.setBrush(UiTheme::color(UiTheme::tokens().ChartBarBg));
                p.drawRoundedRect(QRectF(barX, barY, barW, barH), 6, 6);

                const double filled = (item.value / 10.0) * barW;
                p.setBrush(item.color);
                p.drawRoundedRect(QRectF(barX, barY, filled, barH), 6, 6);

                p.setPen(UiTheme::color(UiTheme::tokens().Text));
                p.setFont(labelFont);
                p.drawText(barX + barW + 10, y, rightValueW, rowH,
                           Qt::AlignLeft | Qt::AlignVCenter,
                           QString("%1").arg(item.value, 0, 'f', 1));
            }
        }

    private:
        struct ScoreItem {
            QString label;
            double value;
            QColor color;
        };

        QVector<ScoreItem> scores_;
    };

    class DialogDragFilter : public QObject {
        QDialog* dialog_;
        bool dragging_ = false;
        QPoint dragOffset_;

    public:
        explicit DialogDragFilter(QDialog* dialog) : QObject(dialog), dialog_(dialog) {}

        bool eventFilter(QObject*, QEvent* event) override
        {
            if (!dialog_)
                return false;

            if (event->type() == QEvent::MouseButtonPress) {
                auto* mouse = static_cast<QMouseEvent*>(event);
                if (mouse->button() == Qt::LeftButton) {
                    dragging_ = true;
                    dragOffset_ = mouse->globalPosition().toPoint() - dialog_->frameGeometry().topLeft();
                    return true;
                }
            } else if (event->type() == QEvent::MouseMove && dragging_) {
                auto* mouse = static_cast<QMouseEvent*>(event);
                dialog_->move(mouse->globalPosition().toPoint() - dragOffset_);
                return true;
            } else if (event->type() == QEvent::MouseButtonRelease) {
                dragging_ = false;
            }

            return false;
        }
    };

    // ---- Buffer Monitor Dialog ----
    void showBufferMonitorDialog(QWidget* parent, const UsbItem& device = UsbItem(),
                                 std::function<void()> onPolicyChanged = {},
                                 std::function<void()> onOpenAnalytics = {})
    {
        const bool hasDevice = device.fingerprint != 0;
        const QString deviceName = hasDevice
            ? fromWString(device.name.empty() ? L"Unknown USB Device" : device.name)
            : trMW("장치 확인 중...");
        const QString vidPid = hasDevice
            ? QString("0x%1 / 0x%2").arg(hex4(device.vid), hex4(device.pid))
            : trMW("분석 중...");
        const QString detectedAt = nowTimestamp();

        auto* dlg = new QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setWindowTitle(trMW("장치 위험도 모니터링"));
        // Non-modal: WindowStaysOnTopHint keeps it visible.
        // setModal(true) was blocking all input to the main window even when
        // the dialog was opened manually without a suspected device.
        dlg->setModal(false);
        dlg->setFixedSize(1024, 790);
        {
            const auto& dt = UiTheme::tokens();
            dlg->setStyleSheet(QString(
                "QDialog { background: %1; border-radius: 12px; }"
                "QFrame#dlgCard { background: %2; border: 1px solid %3; border-radius: 8px; }"
                "QFrame#dlgInfoRow { border-bottom: 1px solid %4; }"
                "QTextEdit { background: %5; color: %6; font-family: Consolas, monospace;"
                "            font-size: 12px; border: none; padding: 12px; }"
                "QCheckBox { color: %7; font-size: 13px; spacing: 6px; }"
                "QCheckBox::indicator { width: 15px; height: 15px; border: 1.5px solid %8;"
                "                       border-radius: 4px; background: %9; }"
                "QCheckBox::indicator:checked { background: %10; border-color: %10;"
                "                               image: url(:/icons/check-white.svg); }"
            ).arg(dt.White, dt.Surface, dt.Border, dt.SurfaceAlt,
                  dt.TerminalBg, dt.TerminalText,
                  dt.SubtextMid, dt.BorderInput, dt.InputBg, dt.Primary));
        }

        // Hook ownership:
        // - Device detected: hook should be active (pre-installed by nativeEvent or installed here).
        //   Take ownership so the destroyed handler can release it if needed.
        // - Manual open (no device): if hook happens to be stuck from a previous event,
        //   also take ownership so closing the dialog cleans it up.
        bool hookStartedByDialog = false;
        if (hasDevice) {
            if (KeyboardHook::IsActive()) {
                hookStartedByDialog = true; // take ownership of pre-installed hook
            } else {
                KeyboardHook::ClearBuffer();
                hookStartedByDialog = KeyboardHook::Install();
            }
        } else {
            // Manual open: if hook is already active (stuck from a previous event),
            // take ownership so closing this dialog will release it.
            if (KeyboardHook::IsActive()) {
                hookStartedByDialog = true;
                Logger::Add(L"[Qt] Buffer monitor: taking ownership of stuck hook.");
            }
        }
        // Shared device state — filled immediately if fingerprint is known, or injected
        // later by nativeEvent after background fingerprinting completes.
        g_dlgSharedDevice = std::make_shared<UsbItem>(device);
        auto sharedDevice = g_dlgSharedDevice;

        // 기존 Win32 UX에 맞춰 버퍼 모니터를 닫으면 키 입력을 즉시 복구한다.
        // 차단/승인대기 장치가 아직 꽂혀 있더라도 사용자가 팝업을 닫은 것은
        // 디버그/시연 흐름에서 "모니터링 종료"로 간주한다.
        g_activeBufferDlg = dlg;
        QObject::connect(dlg, &QObject::destroyed, [hookStartedByDialog, sharedDevice] {
            g_activeBufferDlg = nullptr;
            g_dlgSharedDevice = nullptr;

            if (hookStartedByDialog && KeyboardHook::IsActive()) {
                KeyboardHook::Uninstall();
                Logger::Add(L"[Qt] Buffer monitor closed; keyboard input restored.");
            }
        });

        auto* root = new QVBoxLayout(dlg);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // ── Header ───────────────────────────────────────────────────────────
        auto* hdr = new QWidget;
        hdr->setFixedHeight(61);
        hdr->setCursor(Qt::SizeAllCursor);
        {
            const auto& ht = UiTheme::tokens();
            hdr->setStyleSheet(QString("QWidget { background: %1; border-bottom: 1px solid %2; }")
                               .arg(ht.White, ht.Border));
        }
        hdr->installEventFilter(new DialogDragFilter(dlg));
        auto* hdrL = new QHBoxLayout(hdr);
        hdrL->setContentsMargins(24, 0, 16, 0);
        hdrL->setSpacing(10);
        hdrL->addWidget(iconLabel(":/icons/brand-mark.png", 20));
        auto* title = new QLabel(trMW("장치 위험도 모니터링"));
        title->setFont(pretendardFont(FontRole::SemiBold));
        title->setStyleSheet(QString("font-size: 16px; color: %1;").arg(UiTheme::tokens().Text));
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        hdrL->addWidget(title);
        hdrL->addStretch();
        auto* closeBtn = new QPushButton("✕");
        closeBtn->setFixedSize(34, 34);
        closeBtn->setStyleSheet(QString(
            "QPushButton { background: %1; border: none; border-radius: 17px;"
            "              color: %2; font-size: 14px; }"
            "QPushButton:hover { background: %3; color: %4; }"
        ).arg(UiTheme::tokens().SurfaceAlt, UiTheme::tokens().Subtext,
              UiTheme::tokens().Border, UiTheme::tokens().Text));
        QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
        hdrL->addWidget(closeBtn);
        root->addWidget(hdr);

        // ── Body scroll area ─────────────────────────────────────────────────
        auto* body = new QWidget;
        auto* bodyL = new QVBoxLayout(body);
        bodyL->setContentsMargins(24, 16, 24, 0);
        bodyL->setSpacing(12);
        QTextEdit* bufText = nullptr;
        QStackedWidget* monitorStack = nullptr;
        QPushButton* bufferToggleBtn = nullptr;
        auto normalizedMode = std::make_shared<bool>(false);
        std::function<void()> updateBufferText;
        QLabel* statusBadge = nullptr;
        QLabel* scanImage = nullptr;
        QLabel* scanBadge = nullptr;
        QLabel* scanTitle = nullptr;
        QLabel* scanDescription = nullptr;
        QList<QLabel*> scanRowLabels;
        QList<DotPulseLoader*> scanRowDots;
        std::function<void()> updateScanStatus;

        // Top: two info cards side by side
        {
            auto* topRow = new QHBoxLayout;
            topRow->setSpacing(16);

            // Left card: 연결된 장치 정보
            {
                auto* card = new QFrame;
                card->setObjectName("dlgCard");
                auto* cl = new QVBoxLayout(card);
                cl->setContentsMargins(0, 0, 0, 0);
                cl->setSpacing(0);

                auto* ch = new QWidget;
                ch->setFixedHeight(40);
                ch->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;"
                                  "border-top-left-radius: 8px; border-top-right-radius: 8px;")
                                  .arg(UiTheme::tokens().White, UiTheme::tokens().Border));
                auto* chl = new QHBoxLayout(ch);
                chl->setContentsMargins(16, 0, 16, 0);
                auto* chTitle = new QLabel(trMW("연결된 장치 정보"));
                chTitle->setFont(pretendardFont(FontRole::SemiBold));
                chTitle->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent; border: none;")
                                       .arg(UiTheme::tokens().TextMid));
                chl->addWidget(chTitle);
                cl->addWidget(ch);

                auto* cbody = new QWidget;
                cbody->setStyleSheet("background: transparent;");
                auto* cbl = new QVBoxLayout(cbody);
                cbl->setContentsMargins(16, 12, 16, 12);
                cbl->setSpacing(0);

                auto addDevRow = [&](const QString& key, const QString& val,
                             const QString& objName = {}, bool last = false) -> QLabel* {
                    auto* row = new QFrame;
                    row->setObjectName("dlgInfoRow");
                    if (last) row->setStyleSheet("QFrame#dlgInfoRow { border: none; }");
                    auto* rl = new QHBoxLayout(row);
                    rl->setContentsMargins(0, 8, 0, 8);
                    rl->setSpacing(0);
                    auto* k = new QLabel(key);
                    k->setFont(pretendardFont(FontRole::Regular));
                    k->setStyleSheet(QString("color: %1; font-size: 13px;").arg(UiTheme::tokens().Subtext));
                    k->setFixedWidth(100);
                    auto* v = new QLabel(val);
                    v->setFont(pretendardFont(FontRole::Medium));
                    v->setStyleSheet(QString("color: %1; font-size: 13px;").arg(UiTheme::tokens().Text));
                    if (!objName.isEmpty()) v->setObjectName(objName);
                    rl->addWidget(k);
                    rl->addWidget(v, 1);
                    cbl->addWidget(row);
                    return v;
                };

                addDevRow(trMW("장치 이름"),  deviceName, "dlg_device_name");
                addDevRow(trMW("탐지 인터페이스"),  "HID");
                addDevRow(trMW("탐지 경로"),  "HID Interface");
                addDevRow(trMW("첫 감지 시간"), detectedAt);

                // 상태 badge row
                auto* stRow = new QFrame;
                stRow->setStyleSheet("QFrame { border: none; }");
                auto* stL = new QHBoxLayout(stRow);
                stL->setContentsMargins(0, 8, 0, 0);
                stL->setSpacing(0);
                auto* stKey = new QLabel(trMW("상태"));
                stKey->setFont(pretendardFont(FontRole::Regular));
                stKey->setStyleSheet(QString("color: %1; font-size: 13px;").arg(UiTheme::tokens().Subtext));
                stKey->setFixedWidth(100);
                auto* stBadge = new QLabel(trMW("자동 차단됨"));
                statusBadge = stBadge;
                stBadge->setObjectName("blockBadge");
                stBadge->setFont(pretendardFont(FontRole::SemiBold));
                stBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
                stL->addWidget(stKey);
                stL->addWidget(stBadge);
                stL->addStretch();
                cbl->addWidget(stRow);

                cl->addWidget(cbody, 1);
                topRow->addWidget(card, 1);
            }

            // Right card: 승인/차단 안내
            {
                auto* card = new QFrame;
                card->setObjectName("dlgCard");
                auto* cl = new QVBoxLayout(card);
                cl->setContentsMargins(0, 0, 0, 0);
                cl->setSpacing(0);

                auto* ch = new QWidget;
                ch->setFixedHeight(40);
                ch->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;"
                                  "border-top-left-radius: 8px; border-top-right-radius: 8px;")
                                  .arg(UiTheme::tokens().White, UiTheme::tokens().Border));
                auto* chl = new QHBoxLayout(ch);
                chl->setContentsMargins(16, 0, 16, 0);
                auto* chTitle = new QLabel(trMW("승인/차단 안내"));
                chTitle->setFont(pretendardFont(FontRole::SemiBold));
                chTitle->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent; border: none;")
                                       .arg(UiTheme::tokens().TextMid));
                chl->addWidget(chTitle);
                cl->addWidget(ch);

                auto* cbody = new QWidget;
                cbody->setStyleSheet("background: transparent;");
                auto* cbl = new QVBoxLayout(cbody);
                cbl->setContentsMargins(16, 14, 16, 14);
                cbl->setSpacing(10);

                auto addGuideRow = [&](const QString& iconRes, const QString& color,
                                       const QString& title, const QString& desc) {
                    auto* row = new QHBoxLayout;
                    row->setSpacing(12);
                    auto* ico = iconLabel(iconRes, 20);
                    row->addWidget(ico, 0, Qt::AlignTop);
                    auto* texts = new QVBoxLayout;
                    texts->setSpacing(2);
                    auto* t = new QLabel(title);
                    t->setFont(pretendardFont(FontRole::SemiBold));
                    t->setStyleSheet(QString("color: %1; font-size: 14px;").arg(color));
                    auto* d = new QLabel(desc);
                    d->setFont(pretendardFont(FontRole::Regular));
                    d->setStyleSheet(QString("color: %1; font-size: 12px;").arg(UiTheme::tokens().Subtext));
                    d->setWordWrap(true);
                    texts->addWidget(t);
                    texts->addWidget(d);
                    row->addLayout(texts, 1);
                    cbl->addLayout(row);
                };

                addGuideRow(":/icons/status-allowed.svg", UiTheme::tokens().Primary, trMW("허용"),
                            trMW("이 장치를 신뢰할 수 있는 장치로 등록하고 연결을 허용합니다."));
                addGuideRow(":/icons/status-blocked.svg", UiTheme::tokens().DangerAction, trMW("차단"),
                            trMW("이 장치를 차단하고 연결을 즉시 종료합니다."));

                cbl->addStretch();

                cl->addWidget(cbody, 1);
                topRow->addWidget(card, 1);
            }

            bodyL->addLayout(topRow);
        }

        // Scan status / input buffer stack
        {
            monitorStack = new QStackedWidget;

            auto* scanCard = new QFrame;
            scanCard->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }")
                                    .arg(UiTheme::tokens().White, UiTheme::tokens().Border));
            auto* scanL = new QVBoxLayout(scanCard);
            scanL->setContentsMargins(0, 0, 0, 0);
            scanL->setSpacing(0);

            auto* scanHeader = new QWidget;
            scanHeader->setFixedHeight(40);
            scanHeader->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;"
                                      "border-top-left-radius: 8px; border-top-right-radius: 8px;")
                                      .arg(UiTheme::tokens().White, UiTheme::tokens().Border));
            auto* scanHeaderL = new QHBoxLayout(scanHeader);
            scanHeaderL->setContentsMargins(16, 0, 16, 0);
            auto* scanHeaderTitle = new QLabel(trMW("검사 상태"));
            scanHeaderTitle->setFont(pretendardFont(FontRole::SemiBold));
            scanHeaderTitle->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent; border: none;")
                                           .arg(UiTheme::tokens().Text));
            scanHeaderL->addWidget(scanHeaderTitle);
            scanL->addWidget(scanHeader);

            auto* scanBody = new QWidget;
            scanBody->setStyleSheet("background: transparent; border: none;");
            auto* scanBodyL = new QHBoxLayout(scanBody);
            scanBodyL->setContentsMargins(28, 18, 28, 18);
            scanBodyL->setSpacing(28);

            scanImage = new QLabel;
            scanImage->setFixedSize(330, 250);
            scanImage->setAlignment(Qt::AlignCenter);
            scanImage->setStyleSheet("background: transparent; border: none;");
            scanBodyL->addWidget(scanImage, 0, Qt::AlignVCenter);

            auto* scanTexts = new QVBoxLayout;
            scanTexts->setSpacing(10);
            scanTexts->addStretch();

            scanBadge = new QLabel(trMW("검사중"));
            scanBadge->setFont(pretendardFont(FontRole::SemiBold));
            scanBadge->setFixedHeight(30);
            scanBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            scanBadge->setStyleSheet(QString("QLabel { color: #FFFFFF; background: %1; border: none;"
                                     " border-radius: 8px; padding: 0 18px; font-size: 13px; }")
                                     .arg(UiTheme::tokens().Primary));
            scanTexts->addWidget(scanBadge, 0, Qt::AlignLeft);

            scanTitle = new QLabel(trMW("의심 장치를 검사하고 있습니다."));
            scanTitle->setFont(pretendardFont(FontRole::Bold));
            scanTitle->setTextFormat(Qt::RichText);
            scanTitle->setStyleSheet(QString("font-size: 22px; color: %1; background: transparent; border: none;")
                                     .arg(UiTheme::tokens().Text));
            scanTexts->addWidget(scanTitle);

            scanDescription = new QLabel(trMW("입력 패턴과 장치 정보를 분석하여 안전 여부를 확인하는 중입니다."));
            scanDescription->setFont(pretendardFont(FontRole::Regular));
            scanDescription->setStyleSheet(QString("font-size: 12px; color: %1; background: transparent; border: none;")
                                           .arg(UiTheme::tokens().Subtext));
            scanDescription->setWordWrap(true);
            scanTexts->addWidget(scanDescription);

            auto makeScanRow = [&](const QString& iconText, const QString& text) {
                auto* row = new QFrame;
                row->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }")
                                   .arg(UiTheme::tokens().White, UiTheme::tokens().Border));
                auto* rowL = new QHBoxLayout(row);
                rowL->setContentsMargins(14, 0, 14, 0);
                rowL->setSpacing(10);
                auto* icon = new QLabel(iconText);
                icon->setFont(pretendardFont(FontRole::SemiBold));
                icon->setStyleSheet(QString("font-size: 18px; color: %1; background: transparent; border: none;")
                                    .arg(UiTheme::tokens().Primary));
                icon->setFixedWidth(24);
                auto* label = new QLabel(text);
                label->setFont(pretendardFont(FontRole::SemiBold));
                label->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent; border: none;")
                                     .arg(UiTheme::tokens().Text));
                auto* dots = new DotPulseLoader;
                rowL->addWidget(icon);
                rowL->addWidget(label, 1);
                rowL->addWidget(dots);
                row->setFixedHeight(44);
                scanRowLabels << label;
                scanRowDots << dots;
                scanTexts->addWidget(row);
            };

            makeScanRow("⌕", trMW("입력 패턴 분석 중"));
            makeScanRow(">_", trMW("명령/실행 패턴 확인 중"));
            makeScanRow("◎", trMW("URL 및 위험 조합 확인 중"));

            auto* scanNote = new QLabel(trMW("마지막 입력 후 3초 동안 추가 입력이 없으면 검사 결과를 안내합니다."));
            scanNote->setFont(pretendardFont(FontRole::Regular));
            scanNote->setStyleSheet(QString("font-size: 12px; color: %1; background: transparent; border: none;")
                                    .arg(UiTheme::tokens().Subtext));
            scanNote->setWordWrap(true);
            scanTexts->addWidget(scanNote);
            scanTexts->addStretch();
            scanBodyL->addLayout(scanTexts, 1);
            scanL->addWidget(scanBody, 1);
            monitorStack->addWidget(scanCard);

            auto* card = new QFrame;
            card->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }")
                                .arg(UiTheme::tokens().White, UiTheme::tokens().Border));
            auto* cl = new QVBoxLayout(card);
            cl->setContentsMargins(0, 0, 0, 0);
            cl->setSpacing(0);

            // Card header
            auto* ch = new QWidget;
            ch->setFixedHeight(44);
            ch->setStyleSheet(QString("background: %1; border-bottom: 1px solid %2;"
                              "border-top-left-radius: 8px; border-top-right-radius: 8px;")
                              .arg(UiTheme::tokens().Surface, UiTheme::tokens().Border));
            auto* chl = new QHBoxLayout(ch);
            chl->setContentsMargins(16, 0, 12, 0);
            auto* chTitle = new QLabel(trMW("입력 버퍼 (읽기 전용)"));
            chTitle->setFont(pretendardFont(FontRole::SemiBold));
            chTitle->setStyleSheet(QString("font-size: 13px; color: %1; background: transparent; border: none;")
                                   .arg(UiTheme::tokens().TextMid));
            chl->addWidget(chTitle);
            chl->addStretch();
            auto makeBufferHeaderBtn = [](const QString& text) {
                auto* btn = new QPushButton(text);
                btn->setFont(pretendardFont(FontRole::Medium));
                btn->setFixedHeight(30);
                btn->setMinimumWidth(66);
                btn->setStyleSheet(QString(
                    "QPushButton { background: %1; color: %2; border: 1px solid %3;"
                    " border-radius: 6px; padding: 0 12px; font-size: 12px; }"
                    "QPushButton:hover { background: %4; color: %5; }"
                ).arg(UiTheme::tokens().White, UiTheme::tokens().SubtextMid,
                      UiTheme::tokens().Border, UiTheme::tokens().SurfaceAlt,
                      UiTheme::tokens().Text));
                return btn;
            };
            auto* copyBtn = makeBufferHeaderBtn(trMW("복사"));
            auto* normalizeBtn = makeBufferHeaderBtn(trMW("정규화"));
            auto* clearBtn = makeBufferHeaderBtn(trMW("지우기"));
            chl->addWidget(copyBtn);
            chl->addWidget(normalizeBtn);
            chl->addWidget(clearBtn);
            cl->addWidget(ch);

            // Text area
            bufText = new QTextEdit;
            bufText->setReadOnly(true);
            bufText->setMinimumHeight(110);
            bufText->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
            bufText->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            bufText->setFont(QFont("Consolas", 13));
            bufText->setStyleSheet(
                QString("QTextEdit { background: %1; color: %2; border: none; padding: 14px; }"
                        "QScrollBar:vertical { background: #111A2D; width: 12px; margin: 0; }"
                        "QScrollBar::handle:vertical { background: #334155; border-radius: 6px; min-height: 28px; }"
                        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }")
                .arg(UiTheme::TerminalBg, UiTheme::TerminalText)
            );
            auto lastBufferText = std::make_shared<QString>();
            updateBufferText = [bufText, normalizedMode, lastBufferText] {
                const QScrollBar* sb = bufText->verticalScrollBar();
                const bool atBottom = !sb || sb->value() >= sb->maximum() - 4;
                QString text;
                if (*normalizedMode)
                    text = formatNormalizedKeysForView();
                else
                    text = formatBufferedKeysForView();
                if (text == *lastBufferText)
                    return;
                *lastBufferText = text;
                bufText->setPlainText(text);
                if (atBottom)
                    bufText->moveCursor(QTextCursor::End);
            };
            updateBufferText();

            auto scanPhase = std::make_shared<int>(0);
            auto lastScanKeyCount = std::make_shared<int>(currentBufferedKeyDownCount());
            auto idleScanTicks = std::make_shared<int>(0);
            updateScanStatus = [scanImage, scanBadge, scanTitle, scanDescription, statusBadge,
                                scanRowLabels, scanRowDots, scanPhase, lastScanKeyCount, idleScanTicks] {
                if (!scanImage || !scanBadge || !scanTitle || !scanDescription)
                    return;

                const LiveAnalysisSnapshot snapshot = buildLiveAnalysisSnapshot();
                constexpr double blockThreshold = 8.0;
                const int keyCount = currentBufferedKeyDownCount();
                if (keyCount != *lastScanKeyCount) {
                    *lastScanKeyCount = keyCount;
                    *idleScanTicks = 0;
                } else {
                    *idleScanTicks = (*idleScanTicks >= 22) ? 22 : (*idleScanTicks + 1);
                }

                const bool inputIdleForResult = snapshot.hasInput && *idleScanTicks >= 22;
                const bool isDanger = inputIdleForResult && snapshot.verdict.final_score >= blockThreshold;
                const bool isSafe = inputIdleForResult && !isDanger;

                auto setMascot = [scanImage](const QString& res) {
                    scanImage->setPixmap(QPixmap(res).scaled(
                        scanImage->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                };
                auto setBadgeStyle = [](QLabel* label, const QString& bg) {
                    label->setStyleSheet(QString("QLabel { color: #FFFFFF; background: %1; border: none;"
                                                 " border-radius: 8px; padding: 0 18px; font-size: 13px; }").arg(bg));
                };
                auto setStatusBadge = [](QLabel* label, const QString& text, const QString& color) {
                    if (!label) return;
                    label->setText(text);
                    label->setStyleSheet(QString("QLabel { color: %1; background: transparent; border: none;"
                                                 " font-size: 13px; }").arg(color));
                };
                auto setDots = [&scanRowDots](const QString& text, const QString& color, bool animated) {
                    int filled = static_cast<int>(text.count(QChar(0x25CF)));
                    if (filled < 1) filled = 1;
                    if (filled > 6) filled = 6;
                    for (auto* dots : scanRowDots) {
                        if (!dots) continue;
                        if (animated)
                            dots->setLoading(color, filled - 1);
                        else
                            dots->setComplete(color);
                    }
                };

                if (isDanger) {
                    const auto& st = UiTheme::tokens();
                    setMascot(":/icons/detect_bad.png");
                    scanBadge->setText(trMW("검사 완료"));
                    setBadgeStyle(scanBadge, st.DangerAction);
                    scanTitle->setText(trMW("검사 결과 <span style='color:%1;'>위험</span>").arg(st.DangerAction));
                    scanTitle->setStyleSheet(QString("font-size: 22px; color: %1; background: transparent; border: none;").arg(st.DangerAction));
                    scanDescription->setText(trMW("이 장치에서 위험한 입력 패턴이 감지되어 차단을 권장합니다."));
                    setStatusBadge(statusBadge, trMW("자동 차단됨"), st.DangerAction);

                    QStringList reasons = blockReasonLines(snapshot);
                    while (reasons.size() < 3)
                        reasons << trMW("위험 입력 조합이 감지되었습니다.");
                    for (int i = 0; i < scanRowLabels.size() && i < 3; ++i)
                        scanRowLabels[i]->setText(reasons[i]);
                    setDots("●●●", st.DangerAction, false);
                    return;
                }

                if (isSafe) {
                    const auto& st = UiTheme::tokens();
                    setMascot(":/icons/detect_good.png");
                    scanBadge->setText(trMW("검사 완료"));
                    setBadgeStyle(scanBadge, st.Primary);
                    scanTitle->setText(trMW("검사 결과 <span style='color:%1;'>안전</span>").arg(st.Primary));
                    scanTitle->setStyleSheet(QString("font-size: 22px; color: %1; background: transparent; border: none;").arg(st.Text));
                    scanDescription->setText(trMW("이 장치는 현재까지 확인된 위험 패턴이 없어 안전한 장치로 판단되었습니다."));
                    setStatusBadge(statusBadge, trMW("안전"), st.Primary);
                    const QStringList safeRows = {
                        trMW("비정상 입력 패턴 없음"),
                        trMW("명령/실행 패턴 미검출"),
                        trMW("위험 URL 및 조합 미검출")
                    };
                    for (int i = 0; i < scanRowLabels.size() && i < safeRows.size(); ++i)
                        scanRowLabels[i]->setText(safeRows[i]);
                    setDots("●●●", st.TextMuted, false);
                    return;
                }

                const auto& st = UiTheme::tokens();
                setMascot(":/icons/detecting.png");
                scanBadge->setText(trMW("검사중"));
                setBadgeStyle(scanBadge, st.Primary);
                scanTitle->setText(trMW("의심 장치를 검사하고 있습니다."));
                scanTitle->setStyleSheet(QString("font-size: 22px; color: %1; background: transparent; border: none;").arg(st.Text));
                scanDescription->setText(trMW("입력이 멈춘 뒤 3초 동안 패턴을 종합해 안전 여부를 확인합니다."));
                setStatusBadge(statusBadge, trMW("검사중"), st.PrimaryHover);
                const QStringList scanningRows = {
                    trMW("입력 패턴 분석 중"),
                    trMW("명령/실행 패턴 확인 중"),
                    trMW("URL 및 위험 조합 확인 중")
                };
                for (int i = 0; i < scanRowLabels.size() && i < scanningRows.size(); ++i)
                    scanRowLabels[i]->setText(scanningRows[i]);

                const int activeCount = ((*scanPhase)++ % 6) + 1;
                QString dots;
                for (int i = 0; i < 6; ++i)
                    dots += i < activeCount ? "●" : "○";
                setDots(dots, st.PrimaryHover, true);
            };
            updateScanStatus();

            auto* bufferTimer = new QTimer(dlg);
            bufferTimer->setInterval(140);
            QObject::connect(bufferTimer, &QTimer::timeout, dlg, [updateBufferText, updateScanStatus] {
                updateBufferText();
                if (updateScanStatus)
                    updateScanStatus();
            });
            bufferTimer->start();

            QObject::connect(copyBtn, &QPushButton::clicked, dlg, [bufText] {
                if (bufText)
                    QApplication::clipboard()->setText(bufText->toPlainText());
            });

            QObject::connect(normalizeBtn, &QPushButton::clicked, dlg,
                [normalizedMode, normalizeBtn, updateBufferText] {
                    *normalizedMode = !*normalizedMode;
                    normalizeBtn->setText(*normalizedMode ? trMW("원본 보기") : trMW("정규화"));
                    updateBufferText();
                });

            QObject::connect(clearBtn, &QPushButton::clicked, dlg, [updateBufferText, normalizedMode] {
                *normalizedMode = false;
                KeyboardHook::ClearBuffer();
                updateBufferText();
            });
            cl->addWidget(bufText, 1);

            monitorStack->addWidget(card);
            monitorStack->setCurrentIndex(0);
            bodyL->addWidget(monitorStack, 1);
        }

        root->addWidget(body, 1);

        // ── Footer ───────────────────────────────────────────────────────────
        auto* footer = new QWidget;
        footer->setFixedHeight(68);
        footer->setStyleSheet(QString("QWidget { background: %1; border-top: 1px solid %2; }")
                              .arg(UiTheme::tokens().White, UiTheme::tokens().Border));
        auto* fl = new QHBoxLayout(footer);
        fl->setContentsMargins(24, 0, 24, 0);
        fl->setSpacing(8);

        auto mkFooterBtn = [](const QString& text) {
            auto* btn = new QPushButton(text);
            btn->setFont(pretendardFont(FontRole::Medium));
            btn->setFixedHeight(36);
            btn->setStyleSheet(QString("QPushButton { background: %1; color: %2; border: 1px solid %3;"
                               "border-radius: 8px; padding: 0 14px; font-size: 13px; }"
                               "QPushButton:hover { background: %4; }")
                               .arg(UiTheme::tokens().White, UiTheme::tokens().TextMid,
                                    UiTheme::tokens().Border, UiTheme::tokens().Surface));
            return btn;
        };

        auto* analyticsBtn = mkFooterBtn(trMW("행위 분석 보기"));
        bufferToggleBtn = mkFooterBtn(trMW("입력 버퍼 보기"));
        fl->addWidget(analyticsBtn);
        fl->addWidget(bufferToggleBtn);
        fl->addStretch();

        QObject::connect(analyticsBtn, &QPushButton::clicked, dlg, [dlg, onOpenAnalytics] {
            if (onOpenAnalytics)
                onOpenAnalytics();
            dlg->showMinimized();
        });

        QObject::connect(bufferToggleBtn, &QPushButton::clicked, dlg,
            [monitorStack, bufferToggleBtn, updateBufferText] {
                if (!monitorStack || !bufferToggleBtn)
                    return;
                const bool showBuffer = monitorStack->currentIndex() == 0;
                monitorStack->setCurrentIndex(showBuffer ? 1 : 0);
                bufferToggleBtn->setText(showBuffer ? trMW("검사 상태 보기") : trMW("입력 버퍼 보기"));
                if (showBuffer && updateBufferText)
                    updateBufferText();
            });

        // Action buttons as clickable QWidget (QPushButton can't host child labels reliably)
        auto mkActionBtn = [dlg](const QString& text, const QString& sub,
                                  const QString& bg, const QString& hoverBg, int w) -> QWidget* {
            auto* container = new QWidget;
            container->setObjectName("bufferActionButton");
            container->setFixedSize(w, 44);
            container->setCursor(Qt::PointingHandCursor);
            container->setStyleSheet(QString(
                "QWidget#bufferActionButton { background: %1; border: none; border-radius: 8px; }"
                "QWidget#bufferActionButton:hover { background: %2; }"
                "QWidget#bufferActionButton QLabel { border: none; }"
            ).arg(bg, hoverBg));
            auto* vl = new QVBoxLayout(container);
            vl->setContentsMargins(8, 6, 8, 6);
            vl->setSpacing(1);
            auto* mainLbl = new QLabel(text);
            mainLbl->setFont(pretendardFont(FontRole::SemiBold));
            mainLbl->setStyleSheet("color: #FFFFFF; font-size: 14px; background: transparent;");
            mainLbl->setAlignment(Qt::AlignCenter);
            auto* subLbl = new QLabel(sub);
            subLbl->setFont(pretendardFont(FontRole::Regular));
            subLbl->setStyleSheet("color: rgba(255,255,255,0.75); font-size: 10px; background: transparent;");
            subLbl->setAlignment(Qt::AlignCenter);
            vl->addWidget(mainLbl);
            vl->addWidget(subLbl);
            return container;
        };

        auto* allowBtn = mkActionBtn(trMW("허용"), trMW("이 장치 신뢰 등록"), UiTheme::tokens().Primary, UiTheme::tokens().PrimaryHover, 140);
        auto* denyBtn  = mkActionBtn(trMW("차단"), trMW("즉시 연결 종료"),    UiTheme::tokens().DangerAction, UiTheme::tokens().DangerHover, 120);

        class ClickFilter : public QObject {
            QDialog* dlg_;
            std::shared_ptr<UsbItem> device_;
            bool allow_;
            std::function<void()> onPolicyChanged_;
        public:
            ClickFilter(QDialog* d, std::shared_ptr<UsbItem> device, bool allow,
                        std::function<void()> onPolicyChanged, QObject* parent)
                : QObject(parent), dlg_(d), device_(std::move(device)),
                  allow_(allow), onPolicyChanged_(std::move(onPolicyChanged)) {}
            bool eventFilter(QObject*, QEvent* e) override {
                if (e->type() == QEvent::MouseButtonRelease) {
                    if (device_ && device_->fingerprint != 0) {
                        const QString deviceName = fromWString(
                            device_->name.empty() ? L"Unknown USB Device" : device_->name);
                        if (!confirmPolicyChange(dlg_, deviceName, allow_))
                            return true;

                        device_->allow = allow_;
                        if (allow_) {
                            Policy::AddWhitelist(*device_);
                            reenableAllowedDevice(*device_, L"Buffer monitor allow");
                            Logger::Add(L"[Qt] Buffer monitor allow: " + device_->name);
                        } else {
                            Policy::AddBlacklist(*device_);
                            blockCurrentDevice(*device_, L"Buffer monitor block");
                            Logger::Add(L"[Qt] Buffer monitor block: " + device_->name);
                        }
                        if (onPolicyChanged_) onPolicyChanged_();
                    }
                    dlg_->close();
                    return true;
                }
                return false;
            }
        };
        allowBtn->installEventFilter(new ClickFilter(dlg, sharedDevice, true,  onPolicyChanged, allowBtn));
        denyBtn->installEventFilter(new ClickFilter(dlg, sharedDevice,  false, onPolicyChanged, denyBtn));

        fl->addWidget(allowBtn);
        fl->addSpacing(8);
        fl->addWidget(denyBtn);

        root->addWidget(footer);

        // show() only — no exec(). exec() would create a nested event loop which can
        // delay WH_KEYBOARD_LL callbacks past LowLevelHooksTimeout, causing keystrokes to
        // bypass the hook. Non-blocking show() is consistent with setModal(false) above and
        // keeps the main event loop free to dispatch hook callbacks promptly.
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
        // Center on the primary screen so it's always fully visible regardless of
        // where the main window is positioned (or whether it's hidden to tray).
        {
            const QRect screen = QApplication::primaryScreen()->availableGeometry();
            dlg->move(screen.center() - dlg->rect().center());
        }
    }

} // namespace

// ---- MainWindow ----

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    resize(1440, 900);
    setMinimumSize(1180, 760);
    setWindowTitle("Abang-All");
    applyThemeFromSettings();

    Policy::LoadSettings();
    // 시작 시 로그 보관 기간 적용 — 오래된 엔트리 정리 (0 이면 영구 보관).
    Logger::PruneByDays(Policy::GetSettings().logRetentionDays);

    auto* root = new QWidget;
    root->setObjectName("centralRoot");
    auto* rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    // ---- Sidebar ----
    auto* sidebar = new QWidget;
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(270);
    auto* sidebarLayout = new QVBoxLayout(sidebar);
    sidebarLayout->setContentsMargins(20, 24, 14, 24);
    sidebarLayout->setSpacing(8);

    auto* brand = new QWidget;
    auto* brandLayout = new QHBoxLayout(brand);
    brandLayout->setContentsMargins(12, 4, 10, 4);
    brandLayout->setSpacing(12);
    brandLayout->addWidget(iconLabel(":/icons/brand-mark.png", 40));
    brandLayout->addWidget(label("Abang-All", "brandText"));
    brandLayout->addStretch();
    sidebarLayout->addWidget(brand);
    sidebarLayout->addSpacing(28);

    sidebarLayout->addWidget(addNavButton(tr("대시보드"), 0, ":/icons/nav-dashboard.svg"));
    sidebarLayout->addWidget(addNavButton(tr("USB 목록"), 1, ":/icons/nav-usb.svg"));
    sidebarLayout->addWidget(addNavButton(tr("로그"), 2, ":/icons/nav-log.svg"));
    sidebarLayout->addWidget(addNavButton(tr("행위 분석"), 3, ":/icons/nav-analytics.svg"));
    sidebarLayout->addWidget(addNavButton(tr("설정"), 4, ":/icons/nav-settings.svg"));
    sidebarLayout->addStretch();
    sidebarLayout->addWidget(addNavButton(tr("도움말"), 5, ":/icons/nav-help.svg"));
    auto* collapseButton = new QPushButton(tr("접기"));
    collapseButton->setObjectName("navButton");
    collapseButton->setFont(pretendardFont(FontRole::SemiBold));
    collapseButton->setCursor(Qt::PointingHandCursor);
    collapseButton->setIcon(QIcon(":/icons/nav-collapse.svg"));
    collapseButton->setIconSize(QSize(20, 20));
    connect(collapseButton, &QPushButton::clicked, this, [this] {
        refreshTrayWidget();
        hide();
        if (trayWidget_) {
            trayWidget_->positionNearTray();
            trayWidget_->show();
            trayWidget_->raise();
            trayWidget_->activateWindow();
        }
    });
    sidebarLayout->addWidget(collapseButton);

    // ---- Content area ----
    auto* content = new QWidget;
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);

    // Top bar
    auto* topbar = new QWidget;
    topbar->setObjectName("topbar");
    topbar->setFixedHeight(64);
    auto* topbarLayout = new QHBoxLayout(topbar);
    topbarLayout->setContentsMargins(24, 0, 24, 0);
    topbarLayout->setSpacing(28);
    topbarLayout->addWidget(addTopNavButton(tr("대시보드"), 0));
    topbarLayout->addWidget(addTopNavButton(tr("USB 목록"), 1));
    topbarLayout->addWidget(addTopNavButton(tr("로그"), 2));
    topbarLayout->addWidget(addTopNavButton(tr("행위 분석"), 3));
    topbarLayout->addWidget(addTopNavButton(tr("설정"), 4));
    topbarLayout->addStretch();


    auto* userLayout = new QHBoxLayout;
    userLayout->setContentsMargins(10, 0, 14, 0);
    userLayout->setSpacing(8);
    userLayout->addWidget(iconLabel(":/icons/ui-user.svg", 16));
    userLayout->addWidget(label(tr("관리자")));
    topbarLayout->addLayout(userLayout);

    pages_ = new QStackedWidget;
    pages_->addWidget(createDashboardPage());      // 0
    pages_->addWidget(createUsbListPage());         // 1
    pages_->addWidget(createLogPage());             // 2
    pages_->addWidget(createAnalyticsPage());       // 3
    pages_->addWidget(createSettingsPage());        // 4
    pages_->addWidget(createHelpPage());            // 5

    contentLayout->addWidget(topbar);
    contentLayout->addWidget(pages_, 1);

    rootLayout->addWidget(sidebar);
    rootLayout->addWidget(content, 1);
    setCentralWidget(root);

    registerHidNotifications();
    enforceBlockedDeviceHook();
    refreshNavState();

    auto* helpShortcut = new QShortcut(QKeySequence(Qt::Key_F1), this);
    helpShortcut->setContext(Qt::ApplicationShortcut);
    connect(helpShortcut, &QShortcut::activated, this, [this] {
        showPageHelpPopover(this, currentPage_);
    });

#ifdef Q_OS_WIN
    Logger::SetNotifyWindow(reinterpret_cast<HWND>(winId()), WM_APP_NEWLOG);
#endif

    setupTray();
}

void MainWindow::applyThemeFromSettings()
{
    QSettings settings(QStringLiteral("Abang-All"), QStringLiteral("Abang-All"));
    const QString key = settings.value(QStringLiteral("ui/themeMode"), QStringLiteral("system")).toString();
    applyTheme(UiTheme::themeModeFromKey(key));
}

void MainWindow::applyTheme(UiTheme::ThemeMode mode)
{
    themeMode_ = mode;
    const UiTheme::ThemeMode resolved = UiTheme::resolvedMode(mode);
    UiTheme::setActiveMode(resolved);

    if (auto* app = qobject_cast<QApplication*>(QApplication::instance())) {
        app->setPalette(UiTheme::appPalette(resolved));
        app->setStyleSheet(UiTheme::appStyle(resolved));
    }

    setStyleSheet(UiTheme::appStyle(resolved));

    const auto repolish = [](QWidget* w) {
        if (!w) return;
        w->style()->unpolish(w);
        w->style()->polish(w);
        w->update();
        const auto children = w->findChildren<QWidget*>();
        for (QWidget* child : children) {
            child->style()->unpolish(child);
            child->style()->polish(child);
            child->update();
        }
    };

    repolish(this);
    refreshNavState();
    if (trayWidget_) {
        trayWidget_->applyTheme();
        refreshTrayWidget();
    }
}

// ---------------------------------------------------------------------------
// System tray + TrayWidget setup
// ---------------------------------------------------------------------------
void MainWindow::setupTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable())
        return;

    trayWidget_ = new TrayWidget(this);

    // Connect expand button → show main window
    connect(trayWidget_, &TrayWidget::expandRequested, this, [this] {
        trayWidget_->hide();
        showNormal();
        raise();
        activateWindow();
    });

    // Tray icon
    trayIcon_ = new QSystemTrayIcon(QIcon(":/icons/brand-mark.png"), this);
    trayIcon_->setToolTip(tr("Abang-All 보안 위젯"));

    // Context menu
    auto* menu = new QMenu(this);
    menu->addAction(tr("대시보드 열기"), this, [this] {
        trayWidget_->hide();
        showNormal();
        raise();
        activateWindow();
    });
    menu->addAction(tr("위젯 보기"), this, [this] {
        trayWidget_->positionNearTray();
        trayWidget_->show();
        trayWidget_->raise();
    });
    menu->addSeparator();
    menu->addAction(tr("종료"), qApp, &QApplication::quit);
    trayIcon_->setContextMenu(menu);

    // Left-click: toggle mini widget
    connect(trayIcon_, &QSystemTrayIcon::activated, this,
        [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::Trigger) {
                if (trayWidget_->isVisible()) {
                    trayWidget_->hide();
                } else {
                    trayWidget_->positionNearTray();
                    trayWidget_->show();
                    trayWidget_->raise();
                }
            }
        });

    trayIcon_->show();
    refreshTrayWidget();
}

// ---------------------------------------------------------------------------
// Refresh tray widget with real backend data
// ---------------------------------------------------------------------------
void MainWindow::refreshTrayWidget()
{
    if (!trayWidget_) return;

    // ---- USB count (dedup by display name, same logic as USB list page) ----
    Policy::Load();
    const auto paths = HidScan::EnumeratePresentHidDevicePaths();
    QSet<QString> seenKeys;
    int blacklistConnected = 0;
    int pendingCount = 0;
    for (const auto& path : paths) {
        const UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);
        const QString name = fromWString(item.name.empty() ? L"Unknown HID Device" : item.name);
        QString key = name.trimmed().toLower();
        if (key.isEmpty() || key == "unknown hid device")
            key = fromWString(item.name) + QString::number(item.vid) + QString::number(item.pid);
        if (seenKeys.contains(key)) continue;
        seenKeys.insert(key);

        const Policy::PolicyStatus policyStatus = Policy::Evaluate(item);
        if (policyStatus == Policy::PolicyStatus::Blacklisted)      ++blacklistConnected;
        else if (policyStatus != Policy::PolicyStatus::Whitelisted) ++pendingCount;
    }
    trayWidget_->setUsbCount(static_cast<int>(seenKeys.size()));

    // ---- Threat level ----
    // Danger:  blacklisted device connected
    // Warning: unknown (pending) device connected, or recent anomaly in log
    // Normal:  all clear
    TrayWidget::ThreatLevel level = TrayWidget::ThreatLevel::Normal;
    if (blacklistConnected > 0) {
        level = TrayWidget::ThreatLevel::Danger;
    } else if (pendingCount > 0) {
        level = TrayWidget::ThreatLevel::Warning;
    } else {
        // Also check log for anomaly/warning keywords
        const auto allLogs = Logger::GetAll();
        const auto lines = QString::fromStdWString(allLogs).split('\n', Qt::SkipEmptyParts);
        for (int i = lines.size() - 1; i >= qMax(0, lines.size() - 10); --i) {
            const QString& l = lines[i];
            if (l.contains("anomaly", Qt::CaseInsensitive) ||
                l.contains("block",   Qt::CaseInsensitive) ||
                l.contains("차단",    Qt::CaseInsensitive) ||
                l.contains("warning", Qt::CaseInsensitive)) {
                level = TrayWidget::ThreatLevel::Warning;
                break;
            }
        }
    }
    trayWidget_->setThreatLevel(level);

    // ---- Most recent log event ----
    const auto allLogs = Logger::GetAll();
    const auto lines = QString::fromStdWString(allLogs).split('\n', Qt::SkipEmptyParts);
    if (!lines.isEmpty()) {
        const QString& last = lines.last();
        // Try to extract [HH:mm:ss] or [YYYY-MM-DD HH:mm:ss]
        static const QRegularExpression tsRe(R"(\[(\d{4}-\d{2}-\d{2} )?(\d{2}:\d{2}:\d{2})\])");
        const auto m = tsRe.match(last);
        const QString timeStr = m.hasMatch() ? m.captured(2) : nowTime();
        // Strip leading timestamp bracket for display
        QString msg = last;
        if (m.hasMatch()) msg = last.mid(m.capturedEnd()).trimmed();
        // Strip "[Qt]" prefix
        if (msg.startsWith("[Qt]")) msg = msg.mid(4).trimmed();
        trayWidget_->setRecentEvent(timeStr, msg);
    } else {
        trayWidget_->setRecentEvent(nowTime(), tr("이벤트 없음"));
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // X 버튼으로 닫아도 HWND를 살려야 WM_DEVICECHANGE + 키보드 훅이 유지됨.
    // 트레이로 숨기고 이벤트를 무시해 실제 파괴를 막는다.
    event->ignore();
    hide();
    if (trayIcon_)
        trayIcon_->showMessage(QStringLiteral("Abang-All"),
                               tr("백그라운드에서 계속 모니터링 중입니다."),
                               QSystemTrayIcon::Information, notificationDurationMs());
}

MainWindow::~MainWindow()
{
#ifdef Q_OS_WIN
    if (blockedDeviceHookActive_ && KeyboardHook::IsActive()) {
        KeyboardHook::Uninstall();
        blockedDeviceHookActive_ = false;
    }
    if (hidNotifyHandle_) {
        UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(hidNotifyHandle_));
        hidNotifyHandle_ = nullptr;
    }
#endif
}

QPushButton* MainWindow::addNavButton(const QString& text, int pageIndex, const QString& iconResource)
{
    auto* button = new QPushButton(text);
    button->setObjectName("navButton");
    button->setFont(pretendardFont(FontRole::SemiBold));
    button->setCursor(Qt::PointingHandCursor);
    button->setIcon(QIcon(iconResource));
    button->setIconSize(QSize(20, 20));
    navButtons_.push_back(button);
    connect(button, &QPushButton::clicked, this, [this, pageIndex] { switchPage(pageIndex); });
    return button;
}

QPushButton* MainWindow::addTopNavButton(const QString& text, int pageIndex)
{
    auto* button = new QPushButton(text);
    button->setObjectName("topNavButton");
    button->setFont(pretendardFont(FontRole::SemiBold));
    button->setCursor(Qt::PointingHandCursor);
    topNavButtons_.push_back(button);
    connect(button, &QPushButton::clicked, this, [this, pageIndex] { switchPage(pageIndex); });
    return button;
}

// ===========================================================================
// Dashboard
// ===========================================================================
QWidget* MainWindow::createDashboardPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(100, 24, 100, 32);
    layout->setSpacing(16);

    // Header
    {
        auto* header = new QHBoxLayout;
        auto* titleBox = new QVBoxLayout;
        titleBox->setSpacing(4);
        titleBox->addWidget(label(tr("대시보드"), "pageTitle"));
        titleBox->addWidget(label(tr("시스템 보안 상태 및 주요 이벤트를 모니터링합니다."), "pageSubtitle"));
        header->addLayout(titleBox);
        header->addStretch();
        header->addWidget(label(tr("마지막 업데이트: ") + nowTimestamp(), "mutedText"));
        header->addSpacing(16);
        auto* bufferButton = new QPushButton(tr("장치 위험도 모니터링 열기"));
        bufferButton->setObjectName("bufferButton");
        bufferButton->setFont(pretendardFont(FontRole::SemiBold));
        bufferButton->setIcon(QIcon(":/icons/ui-lock-white.svg"));
        bufferButton->setIconSize(QSize(16, 16));
        connect(bufferButton, &QPushButton::clicked, page, [this] {
            // nullptr parent: the dialog is a fully independent OS window,
            // not constrained to the main window bounds.
            showBufferMonitorDialog(nullptr, UsbItem{}, {}, [this] {
                showNormal();
                raise();
                activateWindow();
                switchPage(3);
            });
        });
        header->addWidget(bufferButton);
        layout->addLayout(header);
    }

    // Single HID scan shared by stat cards + dashboard USB list
    struct DashDevRow { QString time, type, mfr, name, policy; };
    QList<DashDevRow> dashDevRows;

    Policy::Load();
    const int allowedCount = static_cast<int>(Policy::GetWhitelist().size());
    const int blockedCount = static_cast<int>(Policy::GetBlacklist().size());
    int pendingCount = 0;
    {
        const auto paths = HidScan::EnumeratePresentHidDevicePaths();
        QMap<QString, bool> seenName;
        for (const auto& p : paths) {
            const UsbItem item = HidFingerprint::BuildUsbItemFromPath(p);
            if (item.fingerprint == 0) continue;
            const QString displayName = fromWString(item.name);
            if (seenName.contains(displayName)) continue;
            seenName.insert(displayName, true);

            const Policy::PolicyStatus policyStatus = Policy::Evaluate(item);
            if (policyStatus == Policy::PolicyStatus::Unknown) ++pendingCount;
            const QString pol = policyStatus == Policy::PolicyStatus::Whitelisted ? tr("허용")
                : policyStatus == Policy::PolicyStatus::Blacklisted ? tr("차단")
                : tr("승인 대기");
            const QString mfr = item.vid == 0 ? "-" : "HID Device";
            dashDevRows.push_back({nowTimestamp().mid(11, 5), tr("입력 장치"), mfr, displayName, pol});
        }
    }

    // Count security events from log
    int eventCount = 0;
    {
        const std::wstring allLog = Logger::GetAll();
        std::wistringstream ss(allLog);
        std::wstring line;
        while (std::getline(ss, line)) {
            const QString qline = fromWString(line).toLower();
            if (qline.contains("block") || qline.contains("blacklist") || qline.contains("차단")
                || qline.contains("warning") || qline.contains("anomaly"))
                ++eventCount;
        }
    }

    // Stat cards
    {
        auto* stats = new QHBoxLayout;
        stats->setSpacing(16);
        stats->addWidget(createStatCard(tr("허용 장치"),   QString::number(allowedCount), tr("현재 허용된 장치"),      ":/icons/status-allowed.svg"));
        stats->addWidget(createStatCard(tr("차단 장치"),   QString::number(blockedCount), tr("현재 차단된 장치"),      ":/icons/status-blocked.svg"));
        stats->addWidget(createStatCard(tr("승인 대기"),   QString::number(pendingCount), tr("승인 대기 중인 장치"),   ":/icons/status-clock.svg"));
        stats->addWidget(createStatCard(tr("보안 이벤트"), QString::number(eventCount),   tr("로그 기록 기준"),        ":/icons/status-warning.svg"));
        layout->addLayout(stats);
    }

    // Middle row: Protection + Event log
    {
        auto* bodyRow = new QHBoxLayout;
        bodyRow->setSpacing(16);

        // Protection status card
        {
            auto* body = new QWidget;
            auto* bl = new QVBoxLayout(body);
            bl->setContentsMargins(16, 12, 16, 16);
            bl->setSpacing(0);

            // Status panel
            auto* statusPanel = new QFrame;
            statusPanel->setObjectName("statusPanel");
            statusPanel->setMinimumHeight(72);
            statusPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            auto* spL = new QHBoxLayout(statusPanel);
            spL->setContentsMargins(14, 10, 14, 10);
            spL->setSpacing(12);
            spL->addWidget(iconLabel(":/icons/status-shield-blue.svg", 24));
            auto* spText = new QVBoxLayout;
            spText->setSpacing(2);
            auto* spTitle = label(tr("시스템 보호 정상"), "cardTitle");
            spTitle->setObjectName("protectionStatusTitle");
            spText->addWidget(spTitle);
            spText->addWidget(label(tr("모든 보안 모듈이 정상적으로 동작 중입니다."), "pageSubtitle"));
            spL->addLayout(spText);
            spL->addStretch();
            bl->addWidget(statusPanel);
            bl->addSpacing(12);

            const QStringList protRows = {tr("실시간 USB 모니터링"), tr("장치 제어 정책"), tr("비정상 입력 탐지"), tr("행위 분석"), tr("감사 로그")};

            auto* protList = new QWidget;
            protList->setObjectName("protectionList");
            auto* protLayout = new QVBoxLayout(protList);
            protLayout->setContentsMargins(0, 0, 0, 0);
            protLayout->setSpacing(4);
            for (int ri = 0; ri < protRows.size(); ++ri) {
                auto* lineW = new QWidget;
                lineW->setObjectName("protectionRow");
                lineW->setProperty("last", ri == protRows.size() - 1);
                lineW->setFixedHeight(46);
                auto* lineL = new QHBoxLayout(lineW);
                lineL->setContentsMargins(0, 3, 10, 3);
                lineL->setSpacing(10);
                lineL->addWidget(iconLabel(":/icons/status-shield-blue.svg", 16));
                lineL->addWidget(label(protRows[ri]));
                lineL->addStretch();
                auto* onBadge = label(tr("ON"), "protectionOnBadge");
                onBadge->setAlignment(Qt::AlignCenter);
                lineL->addWidget(onBadge);
                protLayout->addWidget(lineW);
            }
            auto* scrollArea = new QScrollArea;
            scrollArea->setObjectName("protectionScrollArea");
            scrollArea->setWidget(protList);
            scrollArea->setWidgetResizable(true);
            scrollArea->setFrameShape(QFrame::NoFrame);
            scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
            scrollArea->setFixedHeight(4 * 50);
            bl->addWidget(scrollArea, 0);
            bodyRow->addWidget(createCard(tr("보호 상태 개요"), body), 1);
        }

        // Recent events card
        {
            auto* body = new QWidget;
            auto* bl = new QVBoxLayout(body);
            bl->setContentsMargins(16, 14, 16, 16);
            bl->setSpacing(18);

            auto* viewAllBtn = new QPushButton(tr("전체 보기 >"));
            viewAllBtn->setCursor(Qt::PointingHandCursor);
            viewAllBtn->setStyleSheet(QString("color: %1; background: transparent; border: none; font-size: 12px;").arg(UiTheme::tokens().TextMuted));
            connect(viewAllBtn, &QPushButton::clicked, this, [this] { switchPage(2); });

            // Build recent events from real log (last 4 non-empty lines)
            struct EventEntry { QString time, title, detail, accent; };
            QList<EventEntry> events;
            {
                const std::wstring allLog = Logger::GetAll();
                QStringList logLines = fromWString(allLog).split('\n', Qt::SkipEmptyParts);
                // Walk from end to collect up to 4
                for (int li = logLines.size() - 1; li >= 0 && events.size() < 4; --li) {
                    const QString& raw = logLines[li].trimmed();
                    if (raw.isEmpty()) continue;
                    // Try to extract timestamp at start (format: [YYYY-MM-DD HH:mm:ss] or bare)
                    QString timeStr, msgStr;
                    static const QRegularExpression tsRx(R"(\[(\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2})\])");
                    const auto match = tsRx.match(raw);
                    if (match.hasMatch()) {
                        timeStr = match.captured(1).mid(11, 5); // HH:mm
                        msgStr  = raw.mid(match.capturedEnd()).trimmed();
                    } else {
                        timeStr = "--:--";
                        msgStr  = raw;
                    }
                    const QString lower = msgStr.toLower();
                    QString accent = "mutedAccent";
                    if (lower.contains("block") || lower.contains("blacklist") || lower.contains("차단"))
                        accent = "dangerAccent";
                    else if (lower.contains("allow") || lower.contains("whitelist") || lower.contains("허용"))
                        accent = "successAccent";
                    else if (lower.contains("warning") || lower.contains("anomaly") || lower.contains("unknown"))
                        accent = "warningAccent";
                    events.prepend({timeStr, msgStr, "", accent});
                }
            }
            if (events.isEmpty())
                events.append({"--:--", tr("기록된 이벤트 없음"), "", "mutedAccent"});

            for (const auto& ev : events) {
                auto* row = new QHBoxLayout;
                row->setSpacing(12);
                auto* timeLabel = label(ev.time, "mutedText");
                timeLabel->setFixedWidth(56);
                row->addWidget(timeLabel);
                auto* accentBar = new QFrame;
                accentBar->setObjectName(ev.accent);
                accentBar->setFixedWidth(2);
                accentBar->setMinimumHeight(38);
                row->addWidget(accentBar);
                auto* evText = new QVBoxLayout;
                evText->setSpacing(2);
                auto* evTitle = label(ev.title, "cardTitle");
                evTitle->setWordWrap(true);
                evText->addWidget(evTitle);
                if (!ev.detail.isEmpty())
                    evText->addWidget(label(ev.detail, "pageSubtitle"));
                row->addLayout(evText);
                row->addStretch();
                bl->addLayout(row);
            }
            bl->addStretch();
            bodyRow->addWidget(createCard(tr("최근 이벤트 로그"), body, viewAllBtn), 1);
        }

        layout->addLayout(bodyRow, 1);
    }

    // Bottom row: device list + policy summary
    {
        auto* bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(16);
        auto* viewAllBtn = new QPushButton(tr("전체 보기 >"));
        viewAllBtn->setCursor(Qt::PointingHandCursor);
        viewAllBtn->setStyleSheet(QString("color: %1; background: transparent; border: none; font-size: 12px;").arg(UiTheme::tokens().TextMuted));
        // Build mini device table from the scan already done above (no second scan)
        auto* devPage = new QWidget;
        {
            auto* devL = new QVBoxLayout(devPage);
            devL->setContentsMargins(16, 8, 16, 16);
            devL->setSpacing(12);
            const int displayCount = qMin(dashDevRows.size(), 4);
            auto* table = new QTableWidget(displayCount == 0 ? 1 : displayCount, 5);
            table->setHorizontalHeaderLabels({tr("시간"), tr("유형"), tr("제조사"), tr("장치 이름"), tr("상태")});
            configureTable(table);
            table->verticalHeader()->setDefaultSectionSize(40);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->setFocusPolicy(Qt::NoFocus);
            if (dashDevRows.isEmpty()) {
                table->setItem(0, 3, new QTableWidgetItem(tr("연결된 HID 장치 없음")));
            } else {
                for (int r = 0; r < displayCount; ++r) {
                    const auto& row = dashDevRows[r];
                    table->setItem(r, 0, new QTableWidgetItem(row.time));
                    table->setItem(r, 1, new QTableWidgetItem(row.type));
                    table->setItem(r, 2, new QTableWidgetItem(row.mfr));
                    table->setItem(r, 3, new QTableWidgetItem(row.name));
                    const char* badge = row.policy == tr("허용") ? "successBadge"
                                      : row.policy == tr("차단") ? "dangerBadge"
                                      : "warningBadge";
                    table->setCellWidget(r, 4, makeBadgeCell(row.policy, badge));
                }
            }
            table->setMinimumHeight(170);
            table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
            devL->addWidget(table, 1);
            auto* fl = new QHBoxLayout;
            fl->addWidget(label(
                dashDevRows.isEmpty()
                    ? tr("연결된 장치 없음")
                    : tr("총 %1개 중 %2개 표시").arg(dashDevRows.size()).arg(displayCount),
                "mutedText"));
            fl->addStretch();
            devL->addLayout(fl);
        }
        bottomRow->addWidget(createCard(tr("현재 연결된 장치"), devPage, viewAllBtn), 1);
        bottomRow->addWidget(createCard(tr("운영 정책 요약"), createPolicySummaryPage()), 1);
        layout->addLayout(bottomRow, 1);
    }

    return page;
}

// ===========================================================================
// USB List Page
// ===========================================================================
QWidget* MainWindow::createUsbListPage(bool /*showTitle*/)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // Header row
    {
        auto* headerRow = new QHBoxLayout;
        auto* titleLbl = new QLabel(tr("USB 목록"));
        titleLbl->setFont(pretendardFont(FontRole::Bold));
        titleLbl->setObjectName("pageTitle");
        headerRow->addWidget(titleLbl);
        headerRow->addStretch();

        auto* searchBox = makeSearchBox(tr("장치명 또는 VID/PID 검색"), 256);
        if (auto* input = searchBox->findChild<QLineEdit*>("searchInput")) {
            input->setText(usbSearchText_);
            connect(input, &QLineEdit::textChanged, this, [this](const QString& t) {
                usbSearchText_ = t;
                applyUsbListFilter();
            });
        }
        headerRow->addWidget(searchBox);
        headerRow->addSpacing(8);

        auto* filterBtn = new QPushButton("  " + tr("필터"));
        filterBtn->setObjectName("secondaryButton");
        filterBtn->setFont(pretendardFont(FontRole::Medium));
        filterBtn->setIcon(QIcon(":/icons/action-filter.svg"));
        filterBtn->setIconSize(QSize(16, 16));
        {
            auto* menu = new QMenu(filterBtn);
            struct Opt { const char* key; QString label; };
            const QList<Opt> opts = {
                {"",        tr("전체")},
                {"허용",     tr("허용")},
                {"차단",     tr("차단")},
                {"승인 대기", tr("승인 대기")},
            };
            for (const auto& o : opts) {
                QAction* act = menu->addAction(o.label);
                const QString key = QString::fromUtf8(o.key);
                connect(act, &QAction::triggered, this, [this, key] {
                    usbPolicyFilter_ = key;
                    applyUsbListFilter();
                });
            }
            filterBtn->setMenu(menu);
        }
        headerRow->addWidget(filterBtn);

        auto* refreshBtn = new QPushButton;
        refreshBtn->setObjectName("secondaryButton");
        refreshBtn->setIcon(QIcon(":/icons/action-refresh.svg"));
        refreshBtn->setIconSize(QSize(16, 16));
        refreshBtn->setFixedWidth(50);
        connect(refreshBtn, &QPushButton::clicked, this, &MainWindow::refreshUsbTable);
        headerRow->addWidget(refreshBtn);

        layout->addLayout(headerRow);
    }

    // Table — rows are filled by refreshUsbTable(), not here
    usbTable_ = new QTableWidget(0, 6);
    usbTable_->setHorizontalHeaderLabels({tr("장치명"), tr("유형"), tr("VID/PID"), tr("상태"), tr("정책"), tr("마지막 확인")});
    configureTable(usbTable_);
    usbTable_->setSelectionMode(QAbstractItemView::NoSelection);
    usbTable_->setFocusPolicy(Qt::NoFocus);
    usbTable_->setColumnWidth(0, 220);
    usbTable_->setColumnWidth(1, 90);
    usbTable_->setColumnWidth(2, 110);
    usbTable_->setColumnWidth(3, 100);
    usbTable_->setColumnWidth(4, 110);
    usbTable_->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);

    // Right: detail card (340px) — widget pointers saved to members
    auto* detailCard = cardFrame();
    detailCard->setFixedWidth(340);
    auto* dcL = new QVBoxLayout(detailCard);
    dcL->setContentsMargins(0, 0, 0, 0);
    dcL->setSpacing(0);

    auto* dcHeader = new QWidget;
    dcHeader->setObjectName("cardHeader");
    dcHeader->setFixedHeight(45);
    auto* dcHL = new QHBoxLayout(dcHeader);
    dcHL->setContentsMargins(16, 0, 16, 0);
    dcHL->addWidget(label(tr("장치 상세 정보"), "cardTitle"));
    dcL->addWidget(dcHeader);

    auto* dcBody = new QWidget;
    auto* dcBL = new QVBoxLayout(dcBody);
    dcBL->setContentsMargins(16, 16, 16, 16);
    dcBL->setSpacing(0);

    // Device icon + name + manufacturer
    {
        auto* devRow = new QHBoxLayout;
        devRow->setSpacing(12);
        auto* iconBox = new QFrame;
        iconBox->setObjectName("usbDetailIconBox");
        iconBox->setFixedSize(42, 42);
        auto* iconBoxL = new QHBoxLayout(iconBox);
        iconBoxL->setContentsMargins(0, 0, 0, 0);
        auto* devIconLbl = new QLabel;
        devIconLbl->setFixedSize(22, 22);
        devIconLbl->setAlignment(Qt::AlignCenter);
        devIconLbl->setPixmap(QIcon(":/icons/device-keyboard.svg").pixmap(18, 18));
        iconBoxL->addWidget(devIconLbl, 0, Qt::AlignCenter);

        auto* nameBox = new QVBoxLayout;
        nameBox->setSpacing(2);
        usbDetName_ = new QLabel("—");
        usbDetName_->setObjectName("usbDetailName");
        usbDetName_->setFont(pretendardFont(FontRole::Bold));
        usbDetMfr_ = new QLabel("—");
        usbDetMfr_->setObjectName("usbDetailMfr");
        usbDetMfr_->setFont(pretendardFont(FontRole::Regular));
        nameBox->addWidget(usbDetName_);
        nameBox->addWidget(usbDetMfr_);

        devRow->addWidget(iconBox);
        devRow->addLayout(nameBox);
        devRow->addStretch();
        dcBL->addLayout(devRow);
        dcBL->addSpacing(24);
    }

    auto addInfoRow = [&](const QString& labelStr, QWidget* valueW) {
        auto* row = new QWidget;
        row->setObjectName("usbDetailInfoRow");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 10, 0, 10);
        rl->setSpacing(0);
        auto* lbl = new QLabel(labelStr);
        lbl->setObjectName("usbDetailInfoLabel");
        lbl->setFont(pretendardFont(FontRole::Medium));
        lbl->setFixedWidth(100);
        rl->addWidget(lbl);
        rl->addWidget(valueW);
        rl->addStretch();
        dcBL->addWidget(row);
    };

    // 상태
    {
        auto* sw = new QWidget;
        sw->setObjectName("usbDetailStatusWrap");
        auto* sl = new QHBoxLayout(sw);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->setSpacing(6);
        usbDetStatusDot_ = new QFrame;
        usbDetStatusDot_->setObjectName("usbDetailStatusDot");
        usbDetStatusDot_->setProperty("connected", false);
        usbDetStatusDot_->setFixedSize(8, 8);
        usbDetStatusLbl_ = new QLabel("—");
        usbDetStatusLbl_->setObjectName("usbDetailStatusText");
        usbDetStatusLbl_->setFont(pretendardFont(FontRole::Regular));
        sl->addWidget(usbDetStatusDot_);
        sl->addWidget(usbDetStatusLbl_);
        addInfoRow(tr("상태"), sw);
    }

    // 적용 정책
    {
        usbDetPolicyLbl_ = new QLabel(tr("승인 대기"));
        usbDetPolicyLbl_->setFont(pretendardFont(FontRole::SemiBold));
        applyCompactPolicyBadge(usbDetPolicyLbl_, "승인 대기");
        addInfoRow(tr("적용 정책"), usbDetPolicyLbl_);
    }

    // 유형
    {
        usbDetTypeV_ = new QLabel("—");
        usbDetTypeV_->setObjectName("usbDetailValue");
        usbDetTypeV_->setFont(pretendardFont(FontRole::Regular));
        addInfoRow(tr("유형"), usbDetTypeV_);
    }

    // VID / PID 행은 사용자 요청으로 제거 (실제 사용성 떨어짐)
    // usbDetVidV_는 다른 곳에서 참조되므로 안전한 더미로 유지
    usbDetVidV_ = new QLabel("—");
    usbDetVidV_->hide();

    // 마지막 연결
    {
        usbDetLastV_ = new QLabel("—");
        usbDetLastV_->setObjectName("usbDetailValue");
        usbDetLastV_->setFont(pretendardFont(FontRole::Regular));
        addInfoRow(tr("마지막 연결"), usbDetLastV_);
    }

    dcBL->addStretch();
    dcBL->addWidget(makeDivider());
    dcBL->addSpacing(16);

    auto makeActionBtn = [](const QString& text, const QString& objectName) {
        auto* btn = new QPushButton(text);
        btn->setObjectName(objectName);
        btn->setFont(pretendardFont(FontRole::Medium));
        btn->setFixedHeight(36);
        return btn;
    };
    usbAllowBtn_      = makeActionBtn(tr("이 장치 허용"), "usbAllowButton");
    usbBlockBtn_      = makeActionBtn(tr("이 장치 차단"), "usbBlockButton");
    usbRemoveRuleBtn_ = makeActionBtn(tr("승인 대기로"), "usbPendingButton");
    dcBL->addWidget(usbAllowBtn_);
    dcBL->addSpacing(8);
    dcBL->addWidget(usbBlockBtn_);
    dcBL->addSpacing(8);
    dcBL->addWidget(usbRemoveRuleBtn_);
    dcBL->addSpacing(10);
    usbActionFeedback_ = new QLabel;
    usbActionFeedback_->setObjectName("usbActionFeedback");
    usbActionFeedback_->setFont(pretendardFont(FontRole::Medium));
    usbActionFeedback_->setWordWrap(true);
    usbActionFeedback_->setMinimumHeight(18);
    dcBL->addWidget(usbActionFeedback_);

    dcL->addWidget(dcBody, 1);

    // Wrap table in card frame
    auto* tableCard = cardFrame();
    auto* tcL = new QVBoxLayout(tableCard);
    tcL->setContentsMargins(0, 0, 0, 0);
    tcL->setSpacing(0);
    tcL->addWidget(usbTable_);

    // Assemble two-panel layout
    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);
    contentRow->addWidget(tableCard, 1);
    contentRow->addWidget(detailCard);
    layout->addLayout(contentRow, 1);

    // Signal connections — use member data, no shared_ptr captures needed
    connect(usbTable_, &QTableWidget::cellClicked, this, [this](int row, int) {
        usbSelectedRow_ = row;
        applyUsbDetailPanel(row);
    });

    connect(usbAllowBtn_, &QPushButton::clicked, this, [this] {
        if (usbSelectedRow_ < 0 || usbSelectedRow_ >= usbRows_.size()) return;
        auto& r = usbRows_[usbSelectedRow_];
        if (r.fingerprint == 0) return;
        // Use stored fingerprint — re-fingerprinting from paths fails for disconnected devices
        UsbItem item;
        item.fingerprint = r.fingerprint;
        item.name        = toWString(r.name);
        item.vid         = r.vid;
        item.pid         = r.pid;
        item.devicePath  = r.devicePaths.empty() ? L"" : r.devicePaths.front();
        item.allow       = true;
        if (!confirmPolicyChange(this, r.name, true)) return;
        Policy::AddWhitelist(item);
        const QString feedback = reenableAllowedDevice(item, L"Policy allow");
        enforceBlockedDeviceHook();
        Logger::Add(L"[Qt] Policy allow: " + toWString(r.name));
        r.policy = "허용";
        usbTable_->setCellWidget(usbSelectedRow_, 4, makeBadgeCell(tr("허용"), "successBadge"));
        applyUsbDetailPanel(usbSelectedRow_);
        showUsbActionFeedback(feedback, UiTheme::tokens().PrimaryText);
    });

    connect(usbBlockBtn_, &QPushButton::clicked, this, [this] {
        if (usbSelectedRow_ < 0 || usbSelectedRow_ >= usbRows_.size()) return;
        auto& r = usbRows_[usbSelectedRow_];
        if (r.fingerprint == 0) return;
        // Use stored fingerprint — re-fingerprinting from paths fails for disconnected devices
        UsbItem item;
        item.fingerprint = r.fingerprint;
        item.name        = toWString(r.name);
        item.vid         = r.vid;
        item.pid         = r.pid;
        item.devicePath  = r.devicePaths.empty() ? L"" : r.devicePaths.front();
        item.allow       = false;
        if (!confirmPolicyChange(this, r.name, false)) return;
        Policy::AddBlacklist(item);
        const QString feedback = blockCurrentDevice(item, L"Policy block");
        enforceBlockedDeviceHook();
        Logger::Add(L"[Qt] Policy block: " + toWString(r.name));
        r.policy = "차단";
        usbTable_->setCellWidget(usbSelectedRow_, 4, makeBadgeCell(tr("차단"), "dangerBadge"));
        applyUsbDetailPanel(usbSelectedRow_);
        showUsbActionFeedback(feedback, UiTheme::tokens().DangerAction);
    });

    connect(usbRemoveRuleBtn_, &QPushButton::clicked, this, [this] {
        if (usbSelectedRow_ < 0 || usbSelectedRow_ >= usbRows_.size()) return;
        auto& r = usbRows_[usbSelectedRow_];
        if (r.fingerprint == 0) return;

        // Use the stored fingerprint directly — re-fingerprinting from paths is unreliable
        // when the device may already be disconnected.
        Policy::Remove(r.fingerprint);
        Logger::Add(L"[Qt] Policy removed: " + toWString(r.name));

        // CRITICAL: if the device is still physically connected it becomes "unknown" right now,
        // but no WM_DEVICECHANGE arrives to trigger a new hook install + approval dialog.
        // We must handle this case inline: lock the keyboard and re-prompt.
        if (r.status == "연결됨" && r.fingerprint != 0) {
            Logger::Add(L"[Qt] Rule deleted while device connected — installing hook for safety.");
            if (!KeyboardHook::IsActive()) {
                KeyboardHook::ClearBuffer();
                KeyboardHook::Install();
            }
            // Reset so the dialog will appear even if this fingerprint was already seen
            lastPromptFingerprint_ = 0;
            // Build a minimal UsbItem from cached row data for the dialog
            UsbItem hookItem;
            hookItem.fingerprint = r.fingerprint;
            hookItem.name        = toWString(r.name);
            hookItem.vid         = r.vid;
            hookItem.pid         = r.pid;
            hookItem.devicePath  = r.devicePaths.empty() ? L"" : r.devicePaths.front();
            QTimer::singleShot(0, this, [this, hookItem] {
                showBufferMonitorDialog(nullptr, hookItem, [this] {
                    enforceBlockedDeviceHook();
                    refreshLiveDataPages();
                }, [this] {
                    showNormal();
                    raise();
                    activateWindow();
                    switchPage(3);
                });
            });
        }

        r.policy = "승인 대기";
        usbTable_->setCellWidget(usbSelectedRow_, 4, makeBadgeCell(tr("승인 대기"), "warningBadge"));
        applyUsbDetailPanel(usbSelectedRow_);
        showUsbActionFeedback(tr("승인 대기 상태로 변경했습니다. 연결 중인 장치는 다시 검토합니다."), UiTheme::tokens().PrimaryText);
    });

    // Initial data fill
    refreshUsbTable();

    return page;
}

// ---------------------------------------------------------------------------
// USB list data refresh — HID scan + table cell update, no widget recreation
// ---------------------------------------------------------------------------
void MainWindow::refreshUsbTable()
{
    if (!usbTable_) return;

    Policy::Load();
    usbRows_.clear();
    QHash<QString, int> rowIndexByDisplayKey;
    const auto devicePaths = HidScan::EnumeratePresentHidDevicePaths();
    for (const auto& path : devicePaths) {
        const UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);
        const QString displayName = fromWString(item.name.empty() ? L"Unknown HID Device" : item.name);
        QString displayKey = displayName.trimmed().toLower();
        if (displayKey.isEmpty() || displayKey == "unknown hid device")
            displayKey = vidPidText(item.vid, item.pid).toLower();

        const QString itemPolicy = policyTextForItem(item);
        if (rowIndexByDisplayKey.contains(displayKey)) {
            auto& existing = usbRows_[rowIndexByDisplayKey.value(displayKey)];
            existing.devicePaths.push_back(item.devicePath);
            if (existing.policy == "승인 대기" && itemPolicy != "승인 대기") {
                existing.policy = itemPolicy;
                existing.fingerprint = item.fingerprint;
            }
            continue;
        }

        UsbRowData row;
        row.name = displayName;
        row.type = "입력 장치";
        row.vidpid = vidPidText(item.vid, item.pid);
        row.status = "연결됨";
        row.policy = itemPolicy;
        row.lastSeen = "방금 전";
        row.iconRes = ":/icons/device-keyboard.svg";
        row.manufacturer = item.vid == 0 ? "Unknown" : "HID Device";
        row.vid = item.vid;
        row.pid = item.pid;
        row.fingerprint = item.fingerprint;
        row.devicePaths.push_back(item.devicePath);
        usbRows_.push_back(row);
        rowIndexByDisplayKey.insert(displayKey, usbRows_.size() - 1);
    }

    if (usbRows_.isEmpty()) {
        UsbRowData empty;
        empty.name = "연결된 HID 장치 없음";
        empty.type = "입력 장치";
        empty.vidpid = "0000:0000";
        empty.status = "해제됨";
        empty.policy = "승인 대기";
        empty.lastSeen = "-";
        empty.iconRes = ":/icons/device-unknown.svg";
        empty.manufacturer = "Unknown";
        usbRows_.push_back(empty);
        Logger::Add(L"[Qt] No connected HID devices found.");
    } else {
        Logger::Add(L"[Qt] HID scan loaded into USB list: " + std::to_wstring(usbRows_.size()));
    }

    auto policyBadge = [](const QString& p) -> const char* {
        if (p == "허용") return "successBadge";
        if (p == "차단") return "dangerBadge";
        return "warningBadge";
    };

    usbTable_->setRowCount(usbRows_.size());
    for (int r = 0; r < usbRows_.size(); ++r) {
        const auto& row = usbRows_[r];

        auto* nameW = new QWidget;
        nameW->setAttribute(Qt::WA_TransparentForMouseEvents);
        setCellWidgetBackground(nameW, cellBackground());
        auto* nl = new QHBoxLayout(nameW);
        nl->setContentsMargins(10, 0, 10, 0);
        nl->setSpacing(8);
        nl->addWidget(iconLabel(row.iconRes, 16));
        auto* nameLbl = new QLabel(row.name);
        nameLbl->setObjectName("usbDeviceName");
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        nameLbl->setFont(pretendardFont(FontRole::Medium));
        nl->addWidget(nameLbl);
        nl->addStretch();
        usbTable_->setCellWidget(r, 0, nameW);

        auto translateType = [this](const QString& t) -> QString {
            if (t == "입력 장치") return tr("입력 장치");
            if (t == "키보드")   return tr("키보드");
            if (t == "저장 장치") return tr("저장 장치");
            if (t == "네트워크") return tr("네트워크");
            if (t == "기타")     return tr("기타");
            return tr("알 수 없음");
        };
        auto translatePolicy = [this](const QString& p) -> QString {
            if (p == "허용")     return tr("허용");
            if (p == "차단")     return tr("차단");
            if (p == "승인 대기") return tr("승인 대기");
            return p;
        };
        usbTable_->setCellWidget(r, 1, makeUsbTextCell(translateType(row.type),    UiTheme::tokens().SubtextMid, pretendardFont(FontRole::Regular), 10));
        usbTable_->setCellWidget(r, 2, makeUsbTextCell(row.vidpid,  UiTheme::tokens().Subtext, QFont("Consolas", 12), 10));
        // row.status는 내부 한국어 키("연결됨"/"해제됨")로 유지 — makeUsbStatusCell이 tr()로 번역
        usbTable_->setCellWidget(r, 3, makeUsbStatusCell(row.status));
        usbTable_->setCellWidget(r, 4, makeBadgeCell(translatePolicy(row.policy), policyBadge(row.policy)));
        const QString lastSeenDisplay = (row.lastSeen == "방금 전") ? tr("방금 전") : row.lastSeen;
        usbTable_->setCellWidget(r, 5, makeUsbTextCell(lastSeenDisplay, UiTheme::tokens().Subtext, pretendardFont(FontRole::Regular), 10));
    }

    if (usbSelectedRow_ >= usbRows_.size()) usbSelectedRow_ = 0;
    applyUsbDetailPanel(usbSelectedRow_);
    applyUsbListFilter();
    usbListStale_ = false;
}

void MainWindow::applyUsbListFilter()
{
    if (!usbTable_) return;
    const QString needle = usbSearchText_.trimmed().toLower();
    for (int r = 0; r < usbRows_.size(); ++r) {
        const auto& row = usbRows_[r];
        bool match = true;
        if (!needle.isEmpty()) {
            match = row.name.toLower().contains(needle)
                 || row.vidpid.toLower().contains(needle);
        }
        if (match && !usbPolicyFilter_.isEmpty())
            match = (row.policy == usbPolicyFilter_);
        usbTable_->setRowHidden(r, !match);
    }
}

// ---------------------------------------------------------------------------
// Update detail panel + row highlight for given row index
// ---------------------------------------------------------------------------
void MainWindow::applyUsbDetailPanel(int row)
{
    if (!usbDetName_ || row < 0 || row >= usbRows_.size()) return;
    const auto& r = usbRows_[row];

    usbDetName_->setText(r.name);
    usbDetMfr_->setText(r.manufacturer);

    const bool connected = (r.status == "연결됨");
    usbDetStatusDot_->setProperty("connected", connected);
    usbDetStatusDot_->style()->unpolish(usbDetStatusDot_);
    usbDetStatusDot_->style()->polish(usbDetStatusDot_);
    usbDetStatusDot_->update();
    usbDetStatusLbl_->setText(connected ? tr("연결됨") : tr("연결 안됨"));

    applyCompactPolicyBadge(usbDetPolicyLbl_, r.policy);

    const QString typeDisplay = (r.type == "입력 장치") ? tr("입력 장치")
                              : (r.type == "키보드") ? tr("키보드")
                              : (r.type == "저장 장치") ? tr("저장 장치")
                              : (r.type == "네트워크") ? tr("네트워크")
                              : (r.type == "기타") ? tr("기타")
                              : r.type;
    usbDetTypeV_->setText(typeDisplay);
    usbDetVidV_->setText(r.vidpid.left(4) + " : " + r.vidpid.mid(5));
    const QString lastDisplay = (r.lastSeen == "방금 전") ? tr("방금 전") : r.lastSeen;
    usbDetLastV_->setText(lastDisplay);

    const bool hasDevice = r.fingerprint != 0;
    if (usbAllowBtn_) {
        usbAllowBtn_->setEnabled(hasDevice && r.policy != "허용");
        usbAllowBtn_->setToolTip(r.policy == "허용" ? tr("이미 허용된 장치입니다.") : tr("이 장치를 허용 목록에 등록합니다."));
    }
    if (usbBlockBtn_) {
        usbBlockBtn_->setEnabled(hasDevice && r.policy != "차단");
        usbBlockBtn_->setToolTip(r.policy == "차단" ? tr("이미 차단된 장치입니다.") : tr("이 장치를 차단 목록에 등록합니다."));
    }
    if (usbRemoveRuleBtn_) {
        usbRemoveRuleBtn_->setEnabled(hasDevice && r.policy != "승인 대기");
        usbRemoveRuleBtn_->setToolTip(r.policy == "승인 대기" ? tr("이미 승인 대기 상태입니다.") : tr("이 장치를 승인 대기 상태로 되돌립니다."));
    }

    // Row highlight
    for (int rr = 0; rr < usbRows_.size(); ++rr) {
        const QColor bg = (rr == row) ? selectedCellBackground() : cellBackground();
        for (int c = 0; c < usbTable_->columnCount(); ++c) {
            if (auto* w = usbTable_->cellWidget(rr, c))
                setCellWidgetBackground(w, bg);
        }
    }
}

void MainWindow::showUsbActionFeedback(const QString& message, const QString& color)
{
    if (!usbActionFeedback_) return;
    usbActionFeedback_->setText(message);
    usbActionFeedback_->setProperty("tone", color == UiTheme::tokens().DangerAction ? "danger" : "primary");
    usbActionFeedback_->style()->unpolish(usbActionFeedback_);
    usbActionFeedback_->style()->polish(usbActionFeedback_);
    usbActionFeedback_->update();

    QTimer::singleShot(3500, this, [this, message] {
        if (usbActionFeedback_ && usbActionFeedback_->text() == message)
            usbActionFeedback_->clear();
    });
}


// Dashboard mini USB list
QWidget* MainWindow::createDashboardUsbListPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 8, 16, 16);
    layout->setSpacing(12);

    // Build rows from real HID scan (same dedup logic as USB list page)
    struct DashRow { QString time, type, mfr, name, policy; };
    QList<DashRow> rows;
    {
        Policy::Load();
        const auto paths = HidScan::EnumeratePresentHidDevicePaths();
        QMap<QString, int> rowIndexByKey;
        for (const auto& p : paths) {
            const UsbItem item = HidFingerprint::BuildUsbItemFromPath(p);
            if (item.fingerprint == 0) continue;
            const QString displayName = fromWString(item.name);
            if (rowIndexByKey.contains(displayName)) continue;
            rowIndexByKey.insert(displayName, rows.size());

            QString policy = "승인 대기";
            const Policy::PolicyStatus policyStatus = Policy::Evaluate(item);
            if (policyStatus == Policy::PolicyStatus::Whitelisted)      policy = "허용";
            else if (policyStatus == Policy::PolicyStatus::Blacklisted) policy = "차단";

            const QString mfr = item.vid == 0 ? "-" : "HID Device";
            const QString type = "입력 장치";

            rows.push_back({nowTimestamp().mid(11, 5), type, mfr, displayName, policy});
        }
    }

    const int displayCount = qMin(rows.size(), 4);
    auto* table = new QTableWidget(displayCount == 0 ? 1 : displayCount, 5);
    table->setHorizontalHeaderLabels({tr("시간"), tr("유형"), tr("제조사"), tr("장치 이름"), tr("상태")});
    configureTable(table);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);

    if (rows.isEmpty()) {
        table->setItem(0, 0, new QTableWidgetItem(""));
        table->setItem(0, 1, new QTableWidgetItem(""));
        table->setItem(0, 2, new QTableWidgetItem(""));
        table->setItem(0, 3, new QTableWidgetItem(tr("연결된 HID 장치 없음")));
        table->setItem(0, 4, new QTableWidgetItem(""));
    } else {
        for (int r = 0; r < displayCount; ++r) {
            const auto& row = rows[r];
            table->setItem(r, 0, new QTableWidgetItem(row.time));
            table->setItem(r, 1, new QTableWidgetItem(tr(row.type.toUtf8().constData())));
            table->setItem(r, 2, new QTableWidgetItem(row.mfr));
            table->setItem(r, 3, new QTableWidgetItem(row.name));
            const char* badge = row.policy == "허용" ? "successBadge"
                              : row.policy == "차단" ? "dangerBadge"
                              : "warningBadge";
            const QString displayPolicy = row.policy == "허용" ? tr("허용")
                                        : row.policy == "차단" ? tr("차단")
                                        : tr("승인 대기");
            table->setCellWidget(r, 4, makeBadgeCell(displayPolicy, badge));
        }
    }
    table->setMinimumHeight(170);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    layout->addWidget(table, 1);
    auto* fl = new QHBoxLayout;
    fl->addWidget(label(
        rows.isEmpty()
            ? tr("연결된 장치 없음")
            : tr("총 %1개 중 %2개 표시").arg(rows.size()).arg(displayCount),
        "mutedText"));
    fl->addStretch();
    layout->addLayout(fl);
    return page;
}

// ===========================================================================
// Log Page
// ===========================================================================
QWidget* MainWindow::createLogPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // ── Header ───────────────────────────────────────────────────────────────
    {
        auto* headerRow = new QHBoxLayout;
        headerRow->addWidget(label(tr("로그"), "pageTitle"));
        headerRow->addStretch();

        auto* searchBox = makeSearchBox(tr("이벤트, 장치명 등 검색"), 256);
        if (auto* input = searchBox->findChild<QLineEdit*>("searchInput")) {
            input->setText(logSearchText_);
            connect(input, &QLineEdit::textChanged, this, [this](const QString& t) {
                logSearchText_ = t;
                applyLogFilter();
            });
        }
        headerRow->addWidget(searchBox);
        headerRow->addSpacing(8);

        auto mkBtn = [&](const QString& text, const QString& icon) {
            auto* btn = new QPushButton("  " + text);
            btn->setObjectName("secondaryButton");
            btn->setFont(pretendardFont(FontRole::Medium));
            btn->setIcon(QIcon(icon));
            btn->setIconSize(QSize(16, 16));
            return btn;
        };

        auto* evtFilterBtn = mkBtn(tr("이벤트 필터"), ":/icons/action-filter.svg");
        {
            auto* menu = new QMenu(evtFilterBtn);
            struct Opt { const char* key; QString label; };
            const QList<Opt> opts = {
                {"",        tr("전체")},
                {"block",   tr("차단")},
                {"allow",   tr("허용")},
                {"warning", tr("확인 필요")},
                {"info",    tr("기록")},
            };
            for (const auto& o : opts) {
                QAction* act = menu->addAction(o.label);
                const QString key = QString::fromUtf8(o.key);
                connect(act, &QAction::triggered, this, [this, key] {
                    logEventFilter_ = key;
                    applyLogFilter();
                });
            }
            evtFilterBtn->setMenu(menu);
        }
        headerRow->addWidget(evtFilterBtn);

        auto* exportBtn = new QPushButton(tr("내보내기 (CSV)"));
        exportBtn->setObjectName("primaryButton");
        exportBtn->setFont(pretendardFont(FontRole::Medium));
        connect(exportBtn, &QPushButton::clicked, this, &MainWindow::exportLogToCsv);
        headerRow->addWidget(exportBtn);
        layout->addLayout(headerRow);
    }

    // ── Per-row helpers shared by initial fill and appendNewLogRows() ───────────
    // Classify a log line → (icon resource, result label, badge objectName)
    auto classifyLine = [this](const QString& line)
        -> std::tuple<QString,QString,QString>
    {
        if (line.contains("block", Qt::CaseInsensitive) ||
            line.contains("blacklist", Qt::CaseInsensitive) || line.contains("차단"))
            return {":/icons/status-blocked.svg", tr("차단"), "blockBadge"};
        if (line.contains("allow", Qt::CaseInsensitive) ||
            line.contains("whitelist", Qt::CaseInsensitive) || line.contains("허용"))
            return {":/icons/status-allowed.svg", tr("허용"), "successBadge"};
        if (line.contains("warning", Qt::CaseInsensitive) ||
            line.contains("anomaly", Qt::CaseInsensitive) ||
            line.contains("unknown", Qt::CaseInsensitive))
            return {":/icons/status-warning.svg", tr("확인 필요"), "warningBadge"};
        return {":/icons/status-info.svg", tr("기록"), "neutralBadge"};
    };

    // Extract a device name from the log line (text after known ": " prefixes)
    auto extractDevice = [](const QString& line) -> QString {
        static const QStringList kPrefixes = {
            "approval: ", "hook active: ", "hook state: ",
            "block: ", "allow: ",
            "Policy removed: ", "Policy allow: ", "Policy block: ",
            "Auto-blocked (설정): ", "Auto-allowed (설정): ",
            "Buffer monitor allow: ", "Buffer monitor block: ",
        };
        for (const auto& p : kPrefixes) {
            const int idx = line.indexOf(p, 0, Qt::CaseInsensitive);
            if (idx != -1) {
                const QString candidate = line.mid(idx + p.length()).trimmed();
                if (!candidate.isEmpty()) return candidate;
            }
        }
        return "-";
    };

    struct LogRow { QString time, event, eventIcon, device, result, badge; };

    QList<LogRow> logRows;
    const QString allLogs = fromWString(Logger::GetAll());
    for (QString line : allLogs.split('\n', Qt::SkipEmptyParts)) {
        line = line.trimmed();
        if (line.isEmpty()) continue;
        auto [icon, result, badge] = classifyLine(line);
        logRows.push_back({nowTimestamp(), line, icon, extractDevice(line), result, badge});
    }
    if (logRows.isEmpty())
        logRows.push_back({nowTimestamp(), tr("아직 기록된 로그가 없습니다."),
                           ":/icons/status-info.svg", "-", tr("대기"), "neutralBadge"});

    QTableWidget* logTable = nullptr;

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    // ── Left: log table (no card title header) ───────────────────────────────
    {
        auto* tableCard = cardFrame();
        auto* tcl = new QVBoxLayout(tableCard);
        tcl->setContentsMargins(0, 0, 0, 0);
        tcl->setSpacing(0);

        auto* table = new QTableWidget(static_cast<int>(logRows.size()), 4);
        logTable = table;
        logTable_ = table;
        lastLogLineCount_ = logRows.size();
        table->setHorizontalHeaderLabels({tr("시간"), tr("이벤트"), tr("장치"), tr("처리 결과")});
        configureTable(table);
        // Same as USB table: NoSelection + WA_TransparentForMouseEvents on all
        // cell widgets → clicks pass through to the viewport → cellClicked fires.
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        auto mkTextCell = [&](const QString& text, const QString& color, bool hi) -> QWidget* {
            auto* w = new QWidget;
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
            setCellWidgetBackground(w, hi ? selectedCellBackground() : cellBackground());
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            l->setSpacing(0);
            auto* txt = new QLabel(text);
            txt->setAttribute(Qt::WA_TransparentForMouseEvents);
            txt->setObjectName(color == QLatin1String(UiTheme::tokens().Subtext) ? "logTextMuted" : "logTextMid");
            txt->setFont(pretendardFont(FontRole::Regular));
            l->addWidget(txt);
            l->addStretch();
            return w;
        };

        // Event cell: icon (16×16) + medium text
        auto mkEventCell = [&](const QString& iconSvg, const QString& text, bool hi) -> QWidget* {
            auto* w = new QWidget;
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
            setCellWidgetBackground(w, hi ? selectedCellBackground() : cellBackground());
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            l->setSpacing(8);
            auto* ico = new QLabel;
            ico->setAttribute(Qt::WA_TransparentForMouseEvents);
            ico->setObjectName("logCellIcon");
            ico->setFixedSize(16, 16);
            ico->setPixmap(QIcon(iconSvg).pixmap(16, 16));
            l->addWidget(ico);
            auto* txt = new QLabel(text);
            txt->setAttribute(Qt::WA_TransparentForMouseEvents);
            txt->setObjectName("logEventText");
            txt->setFont(pretendardFont(FontRole::Medium));
            l->addWidget(txt);
            l->addStretch();
            return w;
        };

        for (int r = 0; r < logRows.size(); ++r) {
            const auto& row = logRows[r];
            const bool hi = (r == 0);

            // Store metadata as QObject properties on the column-0 cell widget.
            // No setItem() needed — identical to how USB table stores data in usbRows_[row].
            // cellClicked reads properties via logTable->cellWidget(row, 0)->property(...).
            auto* timeW = mkTextCell(row.time, UiTheme::tokens().Subtext, hi);
            timeW->setProperty("log_icon",   row.eventIcon);
            timeW->setProperty("log_event",  row.event);
            timeW->setProperty("log_time",   row.time);
            timeW->setProperty("log_device", row.device);
            timeW->setProperty("log_result", row.result);
            timeW->setProperty("log_badge",  row.badge);
            table->setCellWidget(r, 0, timeW);
            table->setCellWidget(r, 1, mkEventCell(row.eventIcon, row.event, hi));
            table->setCellWidget(r, 2, mkTextCell(row.device, UiTheme::tokens().SubtextMid, hi));
            auto* badgeW = makeBadgeCell(row.result, row.badge.toUtf8().constData());
            if (hi) setCellWidgetBackground(badgeW, selectedCellBackground());
            table->setCellWidget(r, 3, badgeW);
        }

        table->setColumnWidth(0, 190);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->setColumnWidth(2, 230);
        table->setColumnWidth(3, 120);
        table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);

        tcl->addWidget(table, 1);
        contentRow->addWidget(tableCard, 1);
    }

    // ── Right: event detail panel (340 px, dynamic) ─────────────────────────
    {
        auto* detailCard = cardFrame();
        detailCard->setFixedWidth(340);
        auto* dcl = new QVBoxLayout(detailCard);
        dcl->setContentsMargins(0, 0, 0, 0);
        dcl->setSpacing(0);

        auto* panelHdr = new QWidget;
        panelHdr->setObjectName("detailPanelHeader");
        panelHdr->setFixedHeight(45);
        auto* phl = new QHBoxLayout(panelHdr);
        phl->setContentsMargins(16, 0, 16, 0);
        phl->addWidget(label(tr("이벤트 상세 정보"), "cardTitle"));
        phl->addStretch();
        dcl->addWidget(panelHdr);

        auto* bodyW = new QWidget;
        auto* bodyL = new QVBoxLayout(bodyW);
        bodyL->setContentsMargins(16, 16, 16, 0);
        bodyL->setSpacing(0);

        // Mutable widgets for dynamic update
        auto* detIconLabel = new QLabel;
        detIconLabel->setFixedSize(22, 22);
        detIconLabel->setAlignment(Qt::AlignCenter);
        detIconLabel->setPixmap(QIcon(logRows[0].eventIcon).pixmap(18, 18));

        auto* detIconBox = new QFrame;
        detIconBox->setFixedSize(38, 38);
        detIconBox->setStyleSheet("QFrame { background: transparent; border: none; }");
        {
            auto* ibl = new QHBoxLayout(detIconBox);
            ibl->setContentsMargins(0, 0, 0, 0);
            ibl->addWidget(detIconLabel, 0, Qt::AlignCenter);
        }

        const auto& t = UiTheme::tokens();

        auto* detEvTitle = new QLabel(logRows[0].event);
        detEvTitle->setObjectName("detailEventTitle");
        detEvTitle->setFont(pretendardFont(FontRole::Bold));
        detEvTitle->setWordWrap(true);

        auto* detEvTime = new QLabel(logRows[0].time);
        detEvTime->setObjectName("detailEventTime");
        detEvTime->setFont(pretendardFont(FontRole::Regular));

        {
            auto* evRow = new QHBoxLayout;
            evRow->setSpacing(12);
            evRow->addWidget(detIconBox);
            auto* evInfo = new QVBoxLayout;
            evInfo->setSpacing(4);
            evInfo->addWidget(detEvTitle);
            evInfo->addWidget(detEvTime);
            evRow->addLayout(evInfo, 1);
            bodyL->addLayout(evRow);
        }
        bodyL->addSpacing(24);

        auto addDetailSection = [&](const QString& labelText, QWidget* valueW) {
            auto* lbl = new QLabel(labelText);
            lbl->setObjectName("detailSectionLabel");
            lbl->setFont(pretendardFont(FontRole::Medium));
            bodyL->addWidget(lbl);
            bodyL->addSpacing(6);
            bodyL->addWidget(valueW);
            bodyL->addSpacing(16);
            bodyL->addWidget(makeDivider());
            bodyL->addSpacing(16);
        };

        auto* detDevBadge = new QLabel(logRows[0].device);
        detDevBadge->setObjectName("detailDeviceBadge");
        detDevBadge->setFont(pretendardFont(FontRole::Medium));
        detDevBadge->setWordWrap(true);
        addDetailSection(tr("장치 정보"), detDevBadge);

        auto* detDesc = new QLabel(tr("등록되지 않은 저장 장치가 연결되었습니다."));
        detDesc->setObjectName("detailDescription");
        detDesc->setFont(pretendardFont(FontRole::Regular));
        detDesc->setWordWrap(true);
        addDetailSection(tr("상세 설명"), detDesc);

        auto* detResultBadge = new QLabel(logRows[0].result);
        detResultBadge->setObjectName(logRows[0].badge.toUtf8().constData());
        detResultBadge->setFont(pretendardFont(FontRole::SemiBold));
        detResultBadge->setFixedSize(policyBadgeSize(logRows[0].result));
        detResultBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        {
            auto* row2 = new QHBoxLayout;
            row2->setSpacing(0);
            auto* resultCol = new QVBoxLayout;
            resultCol->setSpacing(6);
            auto* rLbl = new QLabel(tr("처리 결과"));
            rLbl->setObjectName("detailSectionLabel");
            rLbl->setFont(pretendardFont(FontRole::Medium));
            resultCol->addWidget(rLbl);
            auto* rBadgeRow = new QHBoxLayout;
            rBadgeRow->setContentsMargins(0, 0, 0, 0);
            rBadgeRow->addWidget(detResultBadge);
            rBadgeRow->addStretch();
            resultCol->addLayout(rBadgeRow);
            row2->addLayout(resultCol);
            row2->addStretch();
            auto* userCol = new QVBoxLayout;
            userCol->setSpacing(6);
            auto* uLbl = new QLabel(tr("관련 사용자"));
            uLbl->setObjectName("detailSectionLabel");
            uLbl->setFont(pretendardFont(FontRole::Medium));
            userCol->addWidget(uLbl);
            auto* uVal = new QLabel("SYSTEM");
            uVal->setObjectName("detailUserValue");
            uVal->setFont(pretendardFont(FontRole::Medium));
            userCol->addWidget(uVal);
            row2->addLayout(userCol);
            bodyL->addLayout(row2);
            bodyL->addSpacing(16);
            bodyL->addWidget(makeDivider());
            bodyL->addSpacing(16);
        }

        auto* actionCard = new QFrame;
        actionCard->setObjectName("detailActionCard");
        {
            auto* al = new QVBoxLayout(actionCard);
            al->setContentsMargins(0, 0, 0, 0);
            al->setSpacing(6);
            auto* aHdr = new QHBoxLayout;
            aHdr->setSpacing(6);
            aHdr->addWidget(iconLabel(":/icons/ui-alert.svg", 14));
            auto* aTitle = new QLabel("ACTION REQUIRED");
            aTitle->setObjectName("detailActionTitle");
            aTitle->setFont(pretendardFont(FontRole::Bold));
            aHdr->addWidget(aTitle);
            aHdr->addStretch();
            al->addLayout(aHdr);
            auto* aDesc = new QLabel(
                tr("이 장치가 인가된 장치인지 확인 후 예외 처리를 진행하거나 차단 정책을 유지하세요.")
            );
            aDesc->setObjectName("detailActionDesc");
            aDesc->setFont(pretendardFont(FontRole::Regular));
            aDesc->setWordWrap(true);
            al->addWidget(aDesc);
        }
        bodyL->addWidget(actionCard);
        actionCard->setVisible(logRows[0].badge == "blockBadge");

        bodyL->addStretch();

        {
            // "장치 정보 조회" 버튼은 사용자 요청으로 제거됨
        }

        dcl->addWidget(bodyW, 1);
        contentRow->addWidget(detailCard);

        // Wire up row click → update detail panel
        const QMap<QString,QString> descMap = {
            {"blockBadge",   tr("등록되지 않은 장치의 연결이 차단되었습니다.")},
            {"warningBadge", tr("비정상적인 행위가 탐지되어 경고 알림이 발송되었습니다.")},
            {"successBadge", tr("허용된 장치가 정상적으로 연결되었습니다.")},
            {"neutralBadge", tr("시스템 이벤트가 기록되었습니다.")},
        };

        // Same pattern as USB table: NoSelection + cellClicked.
        // Metadata is read from QObject properties on the column-0 cell widget.
        connect(logTable, &QTableWidget::cellClicked,
            [logTable, detIconLabel, detIconBox, detEvTitle, detEvTime,
             detDevBadge, detDesc, detResultBadge, actionCard, descMap]
            (int row, int) {
                auto* w = logTable->cellWidget(row, 0);
                if (!w) return;

                const QString icon   = w->property("log_icon").toString();
                const QString event  = w->property("log_event").toString();
                const QString time   = w->property("log_time").toString();
                const QString device = w->property("log_device").toString();
                const QString result = w->property("log_result").toString();
                const QString badge  = w->property("log_badge").toString();

                detIconLabel->setPixmap(QIcon(icon).pixmap(18, 18));

                detIconBox->setStyleSheet("QFrame { background: transparent; border: none; }");

                detEvTitle->setText(event);
                detEvTime->setText(time);
                detDevBadge->setText(device == "-" ? "N/A" : device);
                detDesc->setText(descMap.value(badge, tr("이벤트가 기록되었습니다.")));

                detResultBadge->setText(result);
                detResultBadge->setObjectName(badge.toUtf8().constData());
                detResultBadge->setFixedSize(policyBadgeSize(result));
                detResultBadge->style()->unpolish(detResultBadge);
                detResultBadge->style()->polish(detResultBadge);

                actionCard->setVisible(badge == "blockBadge");

                // Re-apply row highlight tint across ALL rows
                for (int rr = 0; rr < logTable->rowCount(); ++rr) {
                    const QColor bg = (rr == row) ? selectedCellBackground() : cellBackground();
                    setCellWidgetBackground(logTable->cellWidget(rr, 0), bg);
                    setCellWidgetBackground(logTable->cellWidget(rr, 1), bg);
                    setCellWidgetBackground(logTable->cellWidget(rr, 2), bg);
                    setCellWidgetBackground(logTable->cellWidget(rr, 3), bg);
                }
            }
        );
    }

    layout->addLayout(contentRow, 1);
    applyLogFilter();
    return page;
}

// ===========================================================================
// Analytics Page
// ===========================================================================
QWidget* MainWindow::createAnalyticsPage()
{
    const auto& t = UiTheme::tokens();
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // Header
    {
        auto* headerRow = new QHBoxLayout;
        auto* titleBox = new QVBoxLayout;
        titleBox->setSpacing(4);
        titleBox->addWidget(label(tr("행위 분석"), "pageTitle"));
        titleBox->addWidget(label(tr("키보드 입력 행위를 분석하여 비정상적 또는 악성 활동을 탐지합니다."), "pageSubtitle"));
        headerRow->addLayout(titleBox);
        headerRow->addStretch();
        headerRow->addWidget(label(tr("마지막 업데이트: ") + nowTimestamp(), "mutedText"));
        layout->addLayout(headerRow);
    }

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    // ---- Left panel (two stacked charts) ----
    auto* leftPanel = new QWidget;
    auto* leftL = new QVBoxLayout(leftPanel);
    leftL->setContentsMargins(0, 0, 0, 0);
    leftL->setSpacing(16);
    BarChartWidget* inputSpeedChart = nullptr;
    RiskBreakdownWidget* riskBreakdownChart = nullptr;
    QLabel* riskAverageValue = nullptr;
    QLabel* sessionStartValue = nullptr;
    QLabel* durationValue = nullptr;
    QLabel* totalInputValue = nullptr;
    QLabel* avgSpeedValue = nullptr;
    QLabel* commandCountValue = nullptr;
    QLabel* hotkeyCountValue = nullptr;
    QLabel* commandScoreValue = nullptr;
    QLabel* urlScoreValue = nullptr;
    QLabel* typingScoreValue = nullptr;
    QLabel* hotkeyScoreValue = nullptr;
    QLabel* threatScoreValue = nullptr;
    QLabel* levelValue = nullptr;
    QFrame* warningBanner = nullptr;
    QLabel* warningTitle = nullptr;
    QLabel* warningDesc = nullptr;
    const QDateTime analyticsSessionStart = QDateTime::currentDateTime();

    // Legend helper
    auto mkLegendItem = [](const QString& color, const QString& text, bool dashed = false) {
        auto* w = new QWidget;
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(0, 0, 0, 0);
        l->setSpacing(6);
        auto* icon = new QLabel(dashed ? "- - -" : "——");
        icon->setStyleSheet(QString("color: %1; font-size: 10px;").arg(color));
        l->addWidget(icon);
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(QString("font-size: 11px; color: %1;").arg(UiTheme::tokens().Subtext));
        l->addWidget(lbl);
        return w;
    };

    // Bar chart card "입력 속도 (keys/sec)"
    {
        auto* rangeLabel = new QLabel(tr("실시간"));
        rangeLabel->setFont(pretendardFont(FontRole::Medium));
        rangeLabel->setStyleSheet(QString("color: %1; font-size: 13px;").arg(t.Subtext));

        auto* chartBody = new QWidget;
        auto* cbL = new QVBoxLayout(chartBody);
        cbL->setContentsMargins(16, 8, 16, 16);
        cbL->setSpacing(8);

        auto* metricLabel = new QLabel(tr("초당 키 입력"));
        metricLabel->setFont(pretendardFont(FontRole::Regular));
        metricLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(t.Subtext));
        auto* subHeader = new QHBoxLayout;
        subHeader->addWidget(metricLabel);
        subHeader->addStretch();
        cbL->addLayout(subHeader);

        auto* barChart = new BarChartWidget;
        inputSpeedChart = barChart;
        barChart->setMinimumHeight(200);
        cbL->addWidget(barChart, 1);

        leftL->addWidget(createCard(tr("입력 속도 (keys/sec)"), chartBody, rangeLabel), 1);
    }

    // Risk breakdown card "탐지 기준별 위험도"
    {
        auto* rangeLabel2 = new QLabel(tr("실시간"));
        rangeLabel2->setFont(pretendardFont(FontRole::Medium));
        rangeLabel2->setStyleSheet(QString("color: %1; font-size: 13px;").arg(t.Subtext));

        auto* breakdownBody = new QWidget;
        auto* lbL = new QVBoxLayout(breakdownBody);
        lbL->setContentsMargins(16, 8, 16, 16);
        lbL->setSpacing(8);

        auto* cumulLabel = new QLabel(tr("알고리즘별 위험도 (0-10)"));
        cumulLabel->setFont(pretendardFont(FontRole::Regular));
        cumulLabel->setStyleSheet(QString("color: %1; font-size: 12px;").arg(t.Subtext));
        auto* subHeader2 = new QHBoxLayout;
        subHeader2->addWidget(cumulLabel);
        subHeader2->addStretch();
        lbL->addLayout(subHeader2);

        auto* breakdownChart = new RiskBreakdownWidget;
        riskBreakdownChart = breakdownChart;
        breakdownChart->setMinimumHeight(180);
        lbL->addWidget(breakdownChart, 1);

        auto* legendRow2 = new QHBoxLayout;
        legendRow2->addStretch();
        legendRow2->addWidget(mkLegendItem("#2563C7", tr("명령어")));
        legendRow2->addSpacing(12);
        legendRow2->addWidget(mkLegendItem("#7C3AED", "URL"));
        legendRow2->addSpacing(12);
        legendRow2->addWidget(mkLegendItem("#0891B2", tr("입력 속도")));
        legendRow2->addSpacing(12);
        legendRow2->addWidget(mkLegendItem("#DB2777", tr("핫키")));
        lbL->addLayout(legendRow2);

        leftL->addWidget(createCard(tr("탐지 기준별 위험도"), breakdownBody, rangeLabel2), 1);
    }

    contentRow->addWidget(leftPanel, 1);

    // ---- Right panel: 현재 세션 요약 (340px) ----
    {
        auto* sessionCard = cardFrame();
        sessionCard->setFixedWidth(340);
        auto* scL = new QVBoxLayout(sessionCard);
        scL->setContentsMargins(0, 0, 0, 0);
        scL->setSpacing(0);

        // Card header
        auto* hdr = new QWidget;
        hdr->setObjectName("cardHeader");
        hdr->setFixedHeight(45);
        auto* hdL = new QHBoxLayout(hdr);
        hdL->setContentsMargins(16, 0, 16, 0);
        hdL->addWidget(label(tr("현재 세션 요약"), "cardTitle"));
        scL->addWidget(hdr);

        // Body
        auto* body = new QWidget;
        auto* bL = new QVBoxLayout(body);
        bL->setContentsMargins(16, 14, 16, 16);
        bL->setSpacing(0);

        // WARNING banner
        {
            auto* warn = new QFrame;
            warningBanner = warn;
            warn->setObjectName("sessionWarningBanner");
            auto* wl = new QHBoxLayout(warn);
            wl->setContentsMargins(0, 0, 0, 0);
            wl->setSpacing(8);
            auto* wIcon = new QLabel("⚠");
            wIcon->setObjectName("sessionWarningIcon");
            auto* wText = new QVBoxLayout;
            wText->setSpacing(2);
            auto* wTitle = new QLabel("WARNING");
            warningTitle = wTitle;
            wTitle->setObjectName("sessionWarningTitle");
            wTitle->setFont(pretendardFont(FontRole::Bold));
            auto* wDesc = new QLabel(tr("불필요한 명령 시퀀스가 감지되었습니다."));
            warningDesc = wDesc;
            wDesc->setObjectName("sessionWarningDesc");
            wDesc->setFont(pretendardFont(FontRole::Regular));
            wText->addWidget(wTitle);
            wText->addWidget(wDesc);
            wl->addWidget(wIcon);
            wl->addLayout(wText);
            wl->addStretch();
            bL->addWidget(warn);
            bL->addSpacing(16);
            warn->setVisible(false);
        }

        // Info rows helper
        auto addSection = [&](const QString& title) {
            auto* lbl = new QLabel(title);
            lbl->setObjectName("sessionSectionLabel");
            lbl->setFont(pretendardFont(FontRole::SemiBold));
            bL->addWidget(lbl);
            bL->addSpacing(6);
        };

        auto addRow = [&](const QString& key, const QString& val, bool valRed = false) -> QLabel* {
            auto* row = new QHBoxLayout;
            auto* k = new QLabel(key);
            k->setObjectName("sessionKeyLabel");
            k->setFont(pretendardFont(FontRole::Regular));
            auto* v = new QLabel(val);
            v->setObjectName(valRed ? "sessionDangerValue" : "sessionValueLabel");
            v->setFont(pretendardFont(FontRole::Medium));
            row->addWidget(k);
            row->addStretch();
            row->addWidget(v);
            bL->addLayout(row);
            bL->addSpacing(8);
            return v;
        };

        // 세션 정보
        addSection(tr("세션 정보"));
        addRow(tr("장치 유형"),  tr("키보드 (HID)"));
        sessionStartValue = addRow(tr("시작 시각"), analyticsSessionStart.toString("yyyy-MM-dd HH:mm:ss"));
        durationValue = addRow(tr("지속 시간"), "00:00:00");
        bL->addWidget(makeDivider());
        bL->addSpacing(12);

        // 입력 지표
        addSection(tr("입력 지표"));
        totalInputValue = addRow(tr("총 키 입력 수"), "0");
        avgSpeedValue = addRow(tr("평균 입력 간격"), "-");
        commandCountValue = addRow(tr("감지 명령"), "0");
        hotkeyCountValue = addRow(tr("핫키 이벤트"), "0");
        bL->addWidget(makeDivider());
        bL->addSpacing(12);

        // 위험 평가
        addSection(tr("위험 평가"));
        commandScoreValue = addRow(tr("명령어 위험도"), "0.0 / 10.0");
        urlScoreValue = addRow(tr("URL 위험도"), "0.0 / 10.0");
        typingScoreValue = addRow(tr("입력 속도 위험도"), "0.0 / 10.0");
        hotkeyScoreValue = addRow(tr("핫키 위험도"), "0.0 / 10.0");
        threatScoreValue = addRow(tr("현재 위협 점수"), "0.0 / 10.0", true);
        levelValue = addRow(tr("판정 등급"),  "SAFE");
        addRow(tr("설정된 임계값"),  "8.0");

        bL->addStretch();
        scL->addWidget(body, 1);
        contentRow->addWidget(sessionCard);
    }

    auto* analyticsTimer = new QTimer(page);
    auto updateThreatTrend = [inputSpeedChart, riskBreakdownChart, analyticsSessionStart, durationValue,
                              totalInputValue, avgSpeedValue, commandCountValue,
                              hotkeyCountValue, commandScoreValue, urlScoreValue,
                              typingScoreValue, hotkeyScoreValue, threatScoreValue, levelValue,
                              warningBanner, warningTitle, warningDesc,
                              lastSnapshot = LiveAnalysisSnapshot{},
                              lastKeyDownCount = -1]() mutable {
        const int bufferedKeyDownCount = currentBufferedKeyDownCount();
        const bool inputChanged = bufferedKeyDownCount != lastKeyDownCount;
        if (inputChanged) {
            lastKeyDownCount = bufferedKeyDownCount;
            lastSnapshot = buildLiveAnalysisSnapshot();
            if (lastSnapshot.keyDownCount > 0) {
                appendInputSpeedTrendSample(
                    lastSnapshot.keysPerSecond,
                    lastSnapshot.typing.classification == TypingAnalyzer::TYPING_AUTOMATED
                        || lastSnapshot.keysPerSecond >= 8.0
                );
                if (inputSpeedChart)
                    inputSpeedChart->setData(g_inputSpeedTrend);
            }
        }
        const auto& snapshot = lastSnapshot;
        if (riskBreakdownChart) {
            riskBreakdownChart->setScores(
                snapshot.threat.session_score,
                snapshot.urlScore,
                snapshot.typingScore,
                snapshot.hotkey.final_score);
        }
        const qint64 elapsed = analyticsSessionStart.secsTo(QDateTime::currentDateTime());
        if (durationValue) {
            durationValue->setText(QString("%1:%2:%3")
                .arg(elapsed / 3600, 2, 10, QChar('0'))
                .arg((elapsed / 60) % 60, 2, 10, QChar('0'))
                .arg(elapsed % 60, 2, 10, QChar('0')));
        }
        if (totalInputValue)
            totalInputValue->setText(QString::number(snapshot.keyDownCount));
        if (avgSpeedValue) {
            avgSpeedValue->setText(snapshot.averageIntervalMs > 0.0
                ? QString("%1 ms/key").arg(snapshot.averageIntervalMs, 0, 'f', 1)
                : QStringLiteral("-"));
        }
        if (commandCountValue)
            commandCountValue->setText(QString::number(static_cast<int>(snapshot.threat.commands.size())));
        if (hotkeyCountValue)
            hotkeyCountValue->setText(QString::number(static_cast<int>(snapshot.hotkey.matches.size())));
        if (commandScoreValue)
            commandScoreValue->setText(scoreText(snapshot.threat.session_score));
        if (urlScoreValue)
            urlScoreValue->setText(scoreText(snapshot.urlScore));
        if (typingScoreValue)
            typingScoreValue->setText(scoreText(snapshot.typingScore));
        if (hotkeyScoreValue)
            hotkeyScoreValue->setText(scoreText(snapshot.hotkey.final_score));
        if (threatScoreValue)
            threatScoreValue->setText(scoreText(snapshot.verdict.final_score));
        if (levelValue)
            levelValue->setText(levelText(snapshot.verdict.level));
        if (warningBanner) {
            const bool showWarning = snapshot.verdict.level == CompoundScorer::WARN
                || snapshot.verdict.level == CompoundScorer::DANGER;
            warningBanner->setVisible(showWarning);
            if (showWarning && warningTitle && warningDesc) {
                if (snapshot.verdict.level == CompoundScorer::DANGER) {
                    warningTitle->setText("DANGER");
                    warningDesc->setText(tr("고위험 명령 시퀀스가 감지되었습니다."));
                } else {
                    warningTitle->setText("WARNING");
                    warningDesc->setText(tr("불필요한 명령 시퀀스가 감지되었습니다."));
                }
            }
        }
    };
    updateThreatTrend();
    QObject::connect(analyticsTimer, &QTimer::timeout, page, updateThreatTrend);
    analyticsTimer->start(1000);

    layout->addLayout(contentRow, 1);
    return page;
}

// ===========================================================================
// Settings Page
// ===========================================================================
QWidget* MainWindow::createSettingsPage()
{
    const auto& t = UiTheme::tokens();
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    QPushButton* saveBtn = nullptr;
    QPushButton* resetBtn = nullptr;
    QCheckBox* cbAutostart = nullptr;
    QCheckBox* cbShowMain = nullptr;
    QButtonGroup* themeGroup = nullptr;
    QCheckBox* cbNotif = nullptr;
    QCheckBox* cbSound = nullptr;
    QComboBox* comboNotifDuration = nullptr;
    QComboBox* comboLogRetention = nullptr;

    {
        auto* headerRow = new QHBoxLayout;
        headerRow->addWidget(label(tr("설정"), "pageTitle"));
        headerRow->addStretch();
        saveBtn = new QPushButton(tr(" 변경 사항 저장"));
        saveBtn->setObjectName("primaryButton");
        saveBtn->setFont(pretendardFont(FontRole::Medium));
        saveBtn->setIcon(QIcon(":/icons/ui-save.svg"));
        saveBtn->setIconSize(QSize(16, 16));
        resetBtn = new QPushButton(tr("되돌리기"));
        resetBtn->setObjectName("secondaryButton");
        resetBtn->setFont(pretendardFont(FontRole::SemiBold));
        layout->addLayout(headerRow);
    }

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    auto addSection = [](QVBoxLayout* parent, const QString& title) {
        auto* row = new QWidget;
        row->setObjectName("settingsSectionDivider");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);
        rowLayout->addWidget(label(title, "sectionHeading"));
        rowLayout->addWidget(makeDivider(), 1);
        parent->addWidget(row);
        parent->addSpacing(12);
    };

    auto addControlRow = [](QVBoxLayout* parent, const QString& title, QWidget* control) {
        auto* row = new QWidget;
        row->setObjectName("settingsFormRow");
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        rowLayout->setSpacing(12);
        auto* rowLabel = label(title);
        rowLabel->setObjectName("settingsFormLabel");
        rowLabel->setFixedWidth(160);
        rowLayout->addWidget(rowLabel);
        rowLayout->addWidget(control);
        rowLayout->addStretch();
        parent->addWidget(row);
    };

    auto* settingsCard = cardFrame();
    auto* scL = new QVBoxLayout(settingsCard);
    scL->setContentsMargins(0, 0, 0, 0);
    scL->setSpacing(0);

    auto* tabBar = new QWidget;
    tabBar->setObjectName("settingsTabBar");
    tabBar->setFixedHeight(51);
    auto* tabBarL = new QHBoxLayout(tabBar);
    tabBarL->setContentsMargins(32, 16, 16, 0);
    tabBarL->setSpacing(24);

    auto* tabStack = new QStackedWidget;
    QVector<QPushButton*> tabBtns;
    struct TabInfo { QString name; QString icon; };
    const QList<TabInfo> tabInfos = {
        {tr("일반"), ":/icons/action-filter.svg"},
        {tr("알림 및 로그"), ":/icons/ui-bell.svg"},
    };

    for (int i = 0; i < tabInfos.size(); ++i) {
        auto* btn = new QPushButton(" " + tabInfos[i].name);
        btn->setObjectName("settingsTab");
        btn->setFont(pretendardFont(FontRole::Medium));
        btn->setCursor(Qt::PointingHandCursor);
        btn->setIcon(QIcon(tabInfos[i].icon));
        btn->setIconSize(QSize(16, 16));
        btn->setProperty("active", i == 0);
        tabBtns.push_back(btn);
        tabBarL->addWidget(btn);
    }
    tabBarL->addStretch();

    for (int i = 0; i < tabBtns.size(); ++i) {
        QPushButton* btn = tabBtns[i];
        connect(btn, &QPushButton::clicked, this, [tabBtns, tabStack, btn, i]() {
            tabStack->setCurrentIndex(i);
            for (QPushButton* b : tabBtns) {
                b->setProperty("active", b == btn);
                b->style()->unpolish(b);
                b->style()->polish(b);
            }
        });
    }

    {
        auto* w = new QWidget;
        auto* wl = new QVBoxLayout(w);
        wl->setContentsMargins(24, 24, 24, 24);
        wl->setSpacing(12);
        addSection(wl, tr("일반"));

        cbAutostart = new QCheckBox(tr("Windows 시작 시 자동 실행"));
        cbAutostart->setObjectName("settingsCheck");
        cbAutostart->setFont(pretendardFont(FontRole::Medium));
        wl->addWidget(cbAutostart);

        cbShowMain = new QCheckBox(tr("시작 시 메인 창 표시"));
        cbShowMain->setObjectName("settingsCheck");
        cbShowMain->setFont(pretendardFont(FontRole::Medium));
        wl->addWidget(cbShowMain);

        auto* themeToggle = new QFrame;
        themeToggle->setObjectName("themeSegmentGroup");
        themeToggle->setFixedSize(300, 46);
        auto* themeToggleL = new QHBoxLayout(themeToggle);
        themeToggleL->setContentsMargins(4, 4, 4, 4);
        themeToggleL->setSpacing(4);
        themeGroup = new QButtonGroup(themeToggle);
        themeGroup->setExclusive(true);
        const QList<QPair<QString, QString>> themeOptions = {
            { QStringLiteral("system"), tr("시스템") },
            { QStringLiteral("light"), tr("라이트") },
            { QStringLiteral("dark"), tr("다크") },
        };
        for (const auto& option : themeOptions) {
            auto* button = new QPushButton(option.second);
            button->setObjectName("themeSegment");
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            button->setFont(pretendardFont(FontRole::Medium));
            button->setProperty("themeKey", option.first);
            themeGroup->addButton(button);
            themeToggleL->addWidget(button, 1);
        }
        addControlRow(wl, tr("테마"), themeToggle);

        auto* langToggle = new QWidget;
        langToggle->setObjectName("themeToggle");
        langToggle->setFixedSize(300, 46);
        auto* langToggleL = new QHBoxLayout(langToggle);
        langToggleL->setContentsMargins(4, 4, 4, 4);
        langToggleL->setSpacing(4);
        langGroup = new QButtonGroup(langToggle);
        langGroup->setExclusive(true);
        const QList<QPair<QString, QString>> langOptions = {
            { QStringLiteral("ko"), tr("한국어") },
            { QStringLiteral("en"), QStringLiteral("English") },
        };
        for (const auto& option : langOptions) {
            auto* button = new QPushButton(option.second);
            button->setObjectName("themeSegment");
            button->setCheckable(true);
            button->setCursor(Qt::PointingHandCursor);
            button->setFont(pretendardFont(FontRole::Medium));
            button->setProperty("langKey", option.first);
            langGroup->addButton(button);
            langToggleL->addWidget(button, 1);
        }
        addControlRow(wl, tr("언어"), langToggle);
        wl->addStretch();
        tabStack->addWidget(w);
    }

    {
        auto* w = new QWidget;
        auto* wl = new QVBoxLayout(w);
        wl->setContentsMargins(24, 24, 24, 24);
        wl->setSpacing(12);
        addSection(wl, tr("알림"));

        cbNotif = new QCheckBox(tr("위험 감지 시 알림 표시"));
        cbNotif->setObjectName("settingsCheck");
        cbNotif->setFont(pretendardFont(FontRole::Medium));
        wl->addWidget(cbNotif);

        cbSound = new QCheckBox(tr("알림 소리"));
        cbSound->setObjectName("settingsCheck");
        cbSound->setFont(pretendardFont(FontRole::Medium));
        wl->addWidget(cbSound);

        comboNotifDuration = new QComboBox;
        comboNotifDuration->addItems({ tr("3초"), tr("5초"), tr("10초") });
        comboNotifDuration->setFixedWidth(180);
        addControlRow(wl, tr("알림 표시 시간"), comboNotifDuration);

        addSection(wl, tr("로그"));
        comboLogRetention = new QComboBox;
        comboLogRetention->addItems({ tr("7일"), tr("30일"), tr("90일"), tr("영구 보관") });
        comboLogRetention->setFixedWidth(180);
        addControlRow(wl, tr("로그 보관 기간"), comboLogRetention);

        auto* openLogDirBtn = new QPushButton(tr("로그 폴더 열기"));
        openLogDirBtn->setObjectName("secondaryButton");
        openLogDirBtn->setFont(pretendardFont(FontRole::SemiBold));
        openLogDirBtn->setFixedWidth(160);
        connect(openLogDirBtn, &QPushButton::clicked, this, [] {
            QDesktopServices::openUrl(QUrl::fromLocalFile(fromWString(Storage::GetAbangAllDir())));
        });
        wl->addWidget(openLogDirBtn);
        wl->addStretch();
        tabStack->addWidget(w);
    }

    scL->addWidget(tabBar);
    scL->addWidget(tabStack, 1);
    contentRow->addWidget(settingsCard, 1);

    layout->addLayout(contentRow, 1);

    {
        auto* footerRow = new QHBoxLayout;
        footerRow->addStretch();
        footerRow->addWidget(saveBtn);
        footerRow->addWidget(resetBtn);
        layout->addLayout(footerRow);
    }

    auto loadSettingsToUi = [=] {
        Policy::LoadSettings();
        const Policy::Settings settings = Policy::GetSettings();
        // 테마/언어만 QSettings(레지스트리) 사용. 나머지는 settings.json.
        QSettings uiSettings(QStringLiteral("Abang-All"), QStringLiteral("Abang-All"));
        cbAutostart->setChecked(settings.autoStart);
        cbShowMain->setChecked(settings.showMainOnStartup);
        cbNotif->setChecked(settings.notifyOnRisk);
        cbSound->setChecked(settings.notificationSound);
        const QString currentLang = uiSettings.value(QStringLiteral("ui/language"), QStringLiteral("ko")).toString();
        for (QAbstractButton* button : langGroup->buttons()) {
            button->setChecked(button->property("langKey").toString() == currentLang);
        }
        if (!langGroup->checkedButton() && !langGroup->buttons().isEmpty())
            langGroup->buttons().front()->setChecked(true);

        const QString themeKey = uiSettings.value(QStringLiteral("ui/themeMode"), QStringLiteral("system")).toString();
        for (QAbstractButton* button : themeGroup->buttons()) {
            button->setChecked(button->property("themeKey").toString() == themeKey);
        }
        if (!themeGroup->checkedButton() && !themeGroup->buttons().isEmpty())
            themeGroup->buttons().front()->setChecked(true);

        {
            const QList<int> durationValues = {3, 5, 10};
            const int dIdx = durationValues.indexOf(settings.notificationDurationSec);
            comboNotifDuration->setCurrentIndex(dIdx >= 0 ? dIdx : 0); // 기본 3초
        }
        {
            const QList<int> retentionValues = {7, 30, 90, 0};  // 0 = 영구 보관
            const int retIdx = retentionValues.indexOf(settings.logRetentionDays);
            comboLogRetention->setCurrentIndex(retIdx >= 0 ? retIdx : 1); // 기본 30일
        }
    };

    connect(themeGroup, &QButtonGroup::buttonClicked, this, [this](QAbstractButton* button) {
        if (!button)
            return;
        const QString key = button->property("themeKey").toString();
        QSettings uiSettings(QStringLiteral("Abang-All"), QStringLiteral("Abang-All"));
        uiSettings.setValue(QStringLiteral("ui/themeMode"), key.isEmpty() ? QStringLiteral("system") : key);
        applyTheme(UiTheme::themeModeFromKey(key));
    });

    connect(saveBtn, &QPushButton::clicked, this, [=] {
        Policy::LoadSettings();
        Policy::Settings settings = Policy::GetSettings();
        settings.autoStart = cbAutostart->isChecked();
        settings.showMainOnStartup = cbShowMain->isChecked();
        settings.notifyOnRisk = cbNotif->isChecked();
        settings.notificationSound = cbSound->isChecked();
        {
            const QList<int> durationValues = {3, 5, 10};
            const int durIdx = comboNotifDuration->currentIndex();
            settings.notificationDurationSec =
                (durIdx >= 0 && durIdx < durationValues.size()) ? durationValues[durIdx] : 3;
        }
        int newRetention = 30;
        {
            const QList<int> retentionValues = {7, 30, 90, 0};
            const int retIdx = comboLogRetention->currentIndex();
            newRetention = (retIdx >= 0 && retIdx < retentionValues.size()) ? retentionValues[retIdx] : 30;
            settings.logRetentionDays = newRetention;
        }
        Policy::SetSettings(settings);
#ifdef Q_OS_WIN
        setWindowsAutoStart(settings.autoStart);
#endif
        // 보관 기간 변경을 즉시 반영 — 짧게 줄였으면 곧바로 오래된 엔트리 제거.
        Logger::PruneByDays(newRetention);

        // 테마/언어만 QSettings(레지스트리) 사용.
        QSettings uiSettings(QStringLiteral("Abang-All"), QStringLiteral("Abang-All"));
        const QString prevLang = uiSettings.value(QStringLiteral("ui/language"), QStringLiteral("ko")).toString();
        QString newLang = QStringLiteral("ko");
        if (langGroup->checkedButton())
            newLang = langGroup->checkedButton()->property("langKey").toString();
        uiSettings.setValue(QStringLiteral("ui/language"), newLang);
        if (newLang != prevLang) {
            const bool toEnglish = (newLang == QStringLiteral("en"));
            const QString title = toEnglish
                ? QStringLiteral("Settings saved.")
                : QStringLiteral("설정이 저장되었습니다.");
            const QString body = toEnglish
                ? QStringLiteral("This program continues running in the background.\nTo apply the language change, exit via the tray icon (\"Quit\") and restart the app.")
                : QStringLiteral("이 프로그램은 백그라운드에서 계속 실행됩니다.\n트레이 아이콘에서 \"종료\"를 선택해 완전히 종료한 뒤 다시 실행하면 언어가 변경됩니다.");
            QMessageBox::information(this, title, body);
        }
        // Dashboard policy summary reads logRetentionDays at construction time.
        // Mark stale so the next switch to dashboard rebuilds it with the new value.
        dashboardStale_ = true;
        if (themeGroup->checkedButton()) {
            const QString key = themeGroup->checkedButton()->property("themeKey").toString();
            uiSettings.setValue(QStringLiteral("ui/themeMode"), key.isEmpty() ? QStringLiteral("system") : key);
            applyTheme(UiTheme::themeModeFromKey(key));
        }
    });

    connect(resetBtn, &QPushButton::clicked, this, loadSettingsToUi);
    loadSettingsToUi();

    return page;
}

// ===========================================================================
// Help Page
// ===========================================================================
QWidget* MainWindow::createHelpPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    {
        auto* headerRow = new QHBoxLayout;
        headerRow->addWidget(label(tr("도움말"), "pageTitle"));
        headerRow->addStretch();
        headerRow->addWidget(makeSearchBox(tr("도움말 검색..."), 288));
        layout->addLayout(headerRow);
    }

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    // ── Right: stacked content (built first so nav can reference it) ──────────
    auto* contentStack = new QStackedWidget;

    // Helper: bullet row
    auto makeBullet = [&](const QString& text, QVBoxLayout* parent) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        row->setContentsMargins(8, 0, 0, 0);
        auto* dot = new QLabel("•");
        dot->setObjectName("helpBulletDot");
        dot->setFixedWidth(12);
        row->addWidget(dot, 0, Qt::AlignTop);
        auto* bText = new QLabel(text);
        bText->setObjectName("helpBulletText");
        bText->setFont(pretendardFont(FontRole::Regular));
        bText->setWordWrap(true);
        row->addWidget(bText, 1);
        parent->addLayout(row);
        parent->addSpacing(8);
    };

    auto makeH2 = [&](const QString& text) {
        auto* h = new QLabel(text);
        h->setObjectName("helpH2");
        h->setFont(pretendardFont(FontRole::Bold));
        return h;
    };
    auto makeH3 = [&](const QString& text) {
        auto* h = new QLabel(text);
        h->setObjectName("helpH3");
        h->setFont(pretendardFont(FontRole::SemiBold));
        return h;
    };
    auto makePara = [&](const QString& text) {
        auto* p = new QLabel(text);
        p->setObjectName("helpPara");
        p->setFont(pretendardFont(FontRole::Regular));
        p->setWordWrap(true);
        return p;
    };

    // Content page 0: 에이전트 설치 및 초기 설정
    {
        auto* card = cardFrame();
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(32, 32, 80, 32);
        cl->setSpacing(0);
        cl->addWidget(makeH2(tr("Abang-All 에이전트 설치 및 초기 설정")));
        cl->addSpacing(16);
        cl->addWidget(makePara(tr("Abang-All 에이전트는 기업의 엔드포인트 보안을 위해 USB 장치 연결을 제어하고 위험한 행위를 차단합니다.")));
        cl->addSpacing(28);
        cl->addWidget(makeH3(tr("1. 설치 과정")));
        cl->addSpacing(16);
        for (const auto& b : {
            tr("제공된 설치 파일(AbangAll_Setup.exe)을 실행합니다."),
            tr("관리자 권한을 승인합니다."),
            tr("화면의 지시에 따라 설치를 완료하고 시스템을 재부팅합니다."),
        }) makeBullet(b, cl);
        cl->addSpacing(8);
        auto* note = new QFrame;
        note->setObjectName("helpNote");
        auto* nl = new QHBoxLayout(note);
        nl->setContentsMargins(17, 14, 17, 14);
        auto* noteLbl = new QLabel(tr("참고: 설치 후 트레이 아이콘이 나타나면 정상적으로 보호가 시작된 것입니다."));
        noteLbl->setObjectName("helpNoteText");
        noteLbl->setFont(pretendardFont(FontRole::Medium));
        noteLbl->setWordWrap(true);
        nl->addWidget(noteLbl);
        cl->addWidget(note);
        cl->addStretch();
        contentStack->addWidget(card);
    }

    // Content page 1: 승인 대기 장치 허용 방법
    {
        auto* card = cardFrame();
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(32, 32, 80, 32);
        cl->setSpacing(0);
        cl->addWidget(makeH2(tr("승인 대기 장치 허용 방법")));
        cl->addSpacing(16);
        cl->addWidget(makePara(tr("새로운 USB 장치가 연결되면 기본적으로 차단 및 \"승인 대기\" 상태가 됩니다.")));
        cl->addSpacing(20);
        for (const auto& b : {
            tr("좌측 메뉴에서 USB 목록으로 이동합니다."),
            tr("상태가 [승인 대기]인 장치를 찾아 클릭합니다."),
            tr("우측 장치 상세 정보 패널에서 하단의 [이 장치 허용] 버튼을 클릭합니다."),
            tr("확인 후 해당 장치(VID/PID 기준)가 영구 허용 목록에 추가됩니다."),
        }) makeBullet(b, cl);
        cl->addStretch();
        contentStack->addWidget(card);
    }

    // Content page 2: 행위 분석(BadUSB 차단) 원리
    {
        auto* card = cardFrame();
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(32, 32, 80, 32);
        cl->setSpacing(0);
        cl->addWidget(makeH2(tr("행위 분석(BadUSB 차단) 원리")));
        cl->addSpacing(16);
        cl->addWidget(makePara(tr("BadUSB는 일반 USB 저장 장치로 위장하여 시스템에 연결된 후, 키보드처럼 동작하여 악성 스크립트를 초고속으로 자동 입력하는 해킹 기법입니다.")));
        cl->addSpacing(28);
        cl->addWidget(makeH3(tr("탐지 및 차단 방식")));
        cl->addSpacing(16);
        for (const auto& b : {
            tr("명령어, URL, 입력 속도, 핫키 패턴을 함께 분석해 종합 위험 점수를 계산합니다."),
            tr("위험 점수가 설정된 임계치(기본 8.0 / 10.0)에 도달하면 BadUSB 또는 자동화 공격으로 간주합니다."),
            tr("정책에 따라 해당 장치를 차단하고 입력을 무효화합니다."),
        }) makeBullet(b, cl);
        cl->addStretch();
        contentStack->addWidget(card);
    }

    contentStack->setCurrentIndex(2); // default: 행위 분석

    // ── Left: nav card (300px) ────────────────────────────────────────────────
    {
        auto* navCard = cardFrame();
        navCard->setFixedWidth(300);
        auto* ncL = new QVBoxLayout(navCard);
        ncL->setContentsMargins(0, 0, 0, 0);
        ncL->setSpacing(0);

        auto* navHeader = new QWidget;
        navHeader->setObjectName("cardHeader");
        navHeader->setFixedHeight(57);
        auto* nhL = new QHBoxLayout(navHeader);
        nhL->setContentsMargins(16, 0, 16, 0);
        nhL->setSpacing(8);
        nhL->addWidget(iconLabel(":/icons/nav-help.svg", 16));
        nhL->addWidget(label(tr("도움말 항목"), "cardTitle"));
        nhL->addStretch();
        ncL->addWidget(navHeader);

        auto* navContent = new QWidget;
        auto* navL = new QVBoxLayout(navContent);
        navL->setContentsMargins(8, 8, 8, 8);
        navL->setSpacing(4);

        struct HelpItem { QString category, title; int index; };
        const QList<HelpItem> items = {
            {tr("시작하기"),   tr("Abang-All 에이전트 설치 및 초기 설정"), 0},
            {tr("정책 관리"),  tr("승인 대기 장치 허용 방법"),             1},
            {tr("기능 가이드"),tr("행위 분석(BadUSB 차단) 원리"),          2},
        };

        auto navItems = std::make_shared<QVector<QWidget*>>();

        for (const auto& item : items) {
            auto* itemW = new QWidget;
            itemW->setObjectName("helpNavItem");
            itemW->setCursor(Qt::PointingHandCursor);
            const bool active = (item.index == 2);
            itemW->setProperty("active", active);
            auto* iL = new QHBoxLayout(itemW);
            iL->setContentsMargins(12, 10, 12, 10);
            iL->setSpacing(0);

            auto* textBox = new QVBoxLayout;
            textBox->setSpacing(2);
            auto* catLbl = new QLabel(item.category.toUpper());
            catLbl->setObjectName("helpNavCategory");
            catLbl->setProperty("active", active);
            catLbl->setFont(pretendardFont(FontRole::SemiBold));
            textBox->addWidget(catLbl);

            auto* titleLbl = new QLabel(item.title);
            titleLbl->setObjectName("helpNavTitle");
            titleLbl->setProperty("active", active);
            titleLbl->setFont(pretendardFont(FontRole::Medium));
            titleLbl->setWordWrap(true);
            textBox->addWidget(titleLbl);
            iL->addLayout(textBox, 1);

            // Chevron for active item
            auto* chevron = new QLabel("›");
            chevron->setObjectName("helpNavChevron");
            chevron->setProperty("active", active);
            iL->addWidget(chevron);

            navL->addWidget(itemW);
            navItems->push_back(itemW);

            const int idx = item.index;
            auto* mouseFilter = new QObject(itemW);
            connect(mouseFilter, &QObject::destroyed, []{}); // keep alive

            // Use event filter via a helper object
            class NavFilter : public QObject {
                int idx_;
                QStackedWidget* stack_;
                std::shared_ptr<QVector<QWidget*>> navItems_;
            public:
                NavFilter(int i, QStackedWidget* s,
                          std::shared_ptr<QVector<QWidget*>> ni,
                          QObject* parent)
                    : QObject(parent), idx_(i), stack_(s),
                      navItems_(std::move(ni)) {}
                bool eventFilter(QObject*, QEvent* e) override {
                    if (e->type() == QEvent::MouseButtonRelease) {
                        stack_->setCurrentIndex(idx_);
                        for (int i = 0; i < navItems_->size(); ++i) {
                            const bool sel = (i == idx_);
                            auto* navItem = navItems_->at(i);
                            navItem->setProperty("active", sel);
                            navItem->style()->unpolish(navItem);
                            navItem->style()->polish(navItem);
                            for (auto* label : navItem->findChildren<QLabel*>()) {
                                label->setProperty("active", sel);
                                label->style()->unpolish(label);
                                label->style()->polish(label);
                                label->update();
                            }
                            navItem->update();
                        }
                        return true;
                    }
                    return false;
                }
            };
            itemW->installEventFilter(new NavFilter(idx, contentStack, navItems, itemW));
        }

        navL->addStretch();
        ncL->addWidget(navContent, 1);

        auto* navFooter = new QWidget;
        navFooter->setObjectName("helpNavFooter");
        auto* nfL = new QVBoxLayout(navFooter);
        nfL->setContentsMargins(16, 14, 16, 14);
        auto* supportBtn = new QPushButton(tr("고객 지원 문의하기"));
        supportBtn->setObjectName("secondaryButton");
        supportBtn->setFont(pretendardFont(FontRole::Medium));
        connect(supportBtn, &QPushButton::clicked, this, [this]() {
            showContactSupportDialog();
        });
        nfL->addWidget(supportBtn);
        ncL->addWidget(navFooter);

        contentRow->addWidget(navCard);
    }

    contentRow->addWidget(contentStack, 1);
    layout->addLayout(contentRow, 1);
    return page;
}

void MainWindow::showContactSupportDialog()
{
    QDialog dlg(this);
    dlg.setWindowTitle(tr("고객 지원 문의하기"));
    dlg.setModal(true);
    dlg.setMinimumWidth(460);

    auto* root = new QVBoxLayout(&dlg);
    root->setContentsMargins(20, 20, 20, 20);
    root->setSpacing(12);

    auto* title = new QLabel(tr("문의 / 오탐 신고"));
    title->setFont(pretendardFont(FontRole::Bold));
    title->setStyleSheet("font-size: 16px;");
    root->addWidget(title);

    auto* note = new QLabel(tr("아래 양식을 작성하면 기본 메일 앱이 열리며 내용이 자동으로 채워집니다."));
    note->setWordWrap(true);
    note->setStyleSheet(QString("color: %1; font-size: 12px;").arg(UiTheme::tokens().SubtextMid));
    root->addWidget(note);

    // 발신자 이메일
    auto* emailLbl = new QLabel(tr("회신 받을 이메일"));
    emailLbl->setFont(pretendardFont(FontRole::Medium));
    root->addWidget(emailLbl);
    auto* emailEdit = new QLineEdit;
    emailEdit->setPlaceholderText(QStringLiteral("example@domain.com"));
    root->addWidget(emailEdit);

    // 신고 유형
    auto* typeLbl = new QLabel(tr("신고 유형"));
    typeLbl->setFont(pretendardFont(FontRole::Medium));
    root->addWidget(typeLbl);
    auto* typeCombo = new QComboBox;
    typeCombo->addItem(tr("오탐 신고 (안전한 장치가 차단됨)"), QStringLiteral("false_positive"));
    typeCombo->addItem(tr("일반 문의"), QStringLiteral("inquiry"));
    typeCombo->addItem(tr("버그 신고"), QStringLiteral("bug"));
    root->addWidget(typeCombo);

    // 본문
    auto* msgLbl = new QLabel(tr("내용"));
    msgLbl->setFont(pretendardFont(FontRole::Medium));
    root->addWidget(msgLbl);
    auto* msgEdit = new QTextEdit;
    msgEdit->setPlaceholderText(tr("상황을 자세히 적어주세요. 발생 시각, 장치명, 재현 방법 등이 도움이 됩니다."));
    msgEdit->setMinimumHeight(140);
    root->addWidget(msgEdit);

    // 버튼
    auto* btnRow = new QHBoxLayout;
    btnRow->addStretch();
    auto* cancelBtn = new QPushButton(tr("취소"));
    auto* sendBtn = new QPushButton(tr("메일 앱으로 열기"));
    sendBtn->setObjectName("primaryButton");
    sendBtn->setDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(sendBtn);
    root->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(sendBtn, &QPushButton::clicked, &dlg, [&]() {
        const QString senderEmail = emailEdit->text().trimmed();
        const QString reportType = typeCombo->currentText();
        const QString reportKey = typeCombo->currentData().toString();
        const QString body = msgEdit->toPlainText().trimmed();

        if (senderEmail.isEmpty() || body.isEmpty()) {
            QMessageBox::warning(&dlg, tr("입력 누락"),
                tr("이메일과 내용을 모두 입력해주세요."));
            return;
        }

        // mailto URL 구성
        const QString subjectPrefix = QStringLiteral("[Abang-All]");
        const QString subject = QString("%1 %2").arg(subjectPrefix, reportType);

        QString bodyText;
        bodyText += tr("──── 사용자 입력 ────") + "\n";
        bodyText += tr("신고 유형: ") + reportType + "\n";
        bodyText += tr("회신 이메일: ") + senderEmail + "\n\n";
        bodyText += tr("내용:") + "\n";
        bodyText += body + "\n\n";
        bodyText += tr("──── 시스템 정보 (자동 첨부) ────") + "\n";
        bodyText += QString("Report Type Key: %1\n").arg(reportKey);
        bodyText += QString("App: Abang-All\n");
        bodyText += QString("Timestamp: %1\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate));

        QUrl mailto(QStringLiteral("mailto:tember8003@mju.ac.kr"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("subject"), subject);
        query.addQueryItem(QStringLiteral("body"), bodyText);
        mailto.setQuery(query);

        if (!QDesktopServices::openUrl(mailto)) {
            QMessageBox::warning(&dlg, tr("메일 앱 열기 실패"),
                tr("기본 메일 앱을 열지 못했습니다. 시스템에 메일 클라이언트가 설정되어 있는지 확인해주세요."));
            return;
        }
        dlg.accept();
    });

    dlg.exec();
}

// ===========================================================================
// Helpers
// ===========================================================================
QWidget* MainWindow::createPolicySummaryPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 8, 16, 14);
    layout->setSpacing(8);

    auto* content = new QWidget;
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(4, 0, 4, 0);
    contentLayout->setSpacing(0);

    // settings.json 에서 로그 보관 기간 읽기
    Policy::LoadSettings();
    const int _retDays = Policy::GetSettings().logRetentionDays;
    const QString _retLabel = _retDays <= 0 ? tr("영구 보관") : tr("%1일").arg(_retDays);

    const QList<QStringList> rows = {
        {tr("미등록 장치 동작"), tr("분석 후 승인"),   "policyValueText"},
        {tr("행위 분석 임계값"), "8.0 / 10",           "policyValueText"},
        {tr("로그 보관 기간"),   _retLabel,               "policyValueText"},
        {tr("실시간 모니터링"),  tr("사용 중"),         "policySuccessText"},
    };
    for (int i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        auto* lineWidget = new QWidget;
        lineWidget->setObjectName("policySummaryLine");
        lineWidget->setFixedHeight(44);
        auto* line = new QHBoxLayout(lineWidget);
        line->setContentsMargins(0, 0, 0, 0);
        line->addWidget(label(row[0]));
        line->addStretch();
        line->addWidget(label(row[1], row[2].toUtf8().constData()));
        contentLayout->addWidget(lineWidget);
        if (i < rows.size() - 1) contentLayout->addWidget(makeDivider());
    }

    auto* scrollArea = new QScrollArea;
    scrollArea->setObjectName("policySummaryScrollArea");
    scrollArea->setWidget(content);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    layout->addWidget(scrollArea, 1);

    auto* editButton = new QPushButton(tr("정책 편집"));
    editButton->setObjectName("secondaryButton");
    editButton->setFont(pretendardFont(FontRole::SemiBold));
    editButton->setIcon(QIcon(":/icons/ui-lock.svg"));
    editButton->setIconSize(QSize(16, 16));
    editButton->setCursor(Qt::PointingHandCursor);
    connect(editButton, &QPushButton::clicked, this, [this] { switchPage(4); });
    layout->addWidget(editButton);
    return page;
}

QWidget* MainWindow::createPlaceholderPage(const QString& title, const QString& subtitle)
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->addWidget(label(title, "pageTitle"));
    layout->addWidget(label(subtitle, "pageSubtitle"));
    layout->addStretch();
    return page;
}

QWidget* MainWindow::createStatCard(const QString& title, const QString& number,
                                     const QString& description, const QString& iconResource)
{
    auto* frame = cardFrame();
    frame->setMinimumHeight(88);
    auto* layout = new QHBoxLayout(frame);
    layout->setContentsMargins(20, 20, 20, 20);
    layout->setSpacing(16);

    auto* iconCircle = new QFrame;
    iconCircle->setObjectName("statIconCircle");
    iconCircle->setFixedSize(46, 46);
    auto* iconLayout = new QHBoxLayout(iconCircle);
    iconLayout->setContentsMargins(0, 0, 0, 0);
    iconLayout->addWidget(iconLabel(iconResource, 24), 0, Qt::AlignCenter);
    layout->addWidget(iconCircle);

    auto* textBox = new QVBoxLayout;
    textBox->setSpacing(2);
    textBox->addWidget(label(title, "cardTitle"));
    auto* numberRow = new QHBoxLayout;
    numberRow->setSpacing(8);
    numberRow->addWidget(label(number, "statNumber"));
    numberRow->addWidget(label(description, "mutedText"));
    numberRow->addStretch();
    textBox->addLayout(numberRow);
    layout->addLayout(textBox);
    layout->addStretch();
    return frame;
}

QWidget* MainWindow::createCard(const QString& title, QWidget* body, QWidget* headerRight)
{
    auto* frame = cardFrame();
    auto* layout = new QVBoxLayout(frame);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QWidget;
    header->setObjectName("cardHeader");
    auto* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(16, 12, 16, 12);
    headerLayout->addWidget(label(title, "cardTitle"));
    headerLayout->addStretch();
    if (headerRight) headerLayout->addWidget(headerRight);
    layout->addWidget(header);
    layout->addWidget(body, 1);
    return frame;
}

void MainWindow::registerHidNotifications()
{
#ifdef Q_OS_WIN
    if (hidNotifyHandle_)
        return;

    GUID hidGuid{};
    HidD_GetHidGuid(&hidGuid);

    DEV_BROADCAST_DEVICEINTERFACE_W filter{};
    filter.dbcc_size = sizeof(filter);
    filter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    filter.dbcc_classguid = hidGuid;

    hidNotifyHandle_ = RegisterDeviceNotificationW(
        reinterpret_cast<HWND>(winId()),
        &filter,
        DEVICE_NOTIFY_WINDOW_HANDLE);

    Logger::Add(hidNotifyHandle_
        ? L"[Qt] HID notifications registered."
        : L"[Qt] HID notification registration failed.");
#endif
}

void MainWindow::enforceBlockedDeviceHook()
{
    Policy::Load();

    bool hasBlockedDevice = false;
    std::wstring blockedName;
    const auto devicePaths = HidScan::EnumeratePresentHidDevicePaths();
    for (const auto& path : devicePaths) {
        if (path.empty())
            continue;

        const UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);
        if (item.fingerprint != 0 && Policy::Evaluate(item) == Policy::PolicyStatus::Blacklisted) {
            hasBlockedDevice = true;
            blockedName = item.name;
            break;
        }
    }

    if (hasBlockedDevice) {
        if (!KeyboardHook::IsActive()) {
            if (KeyboardHook::Install()) {
                Logger::Add(L"[Qt] Keyboard hook installed for blocked HID device: " + blockedName);
            } else {
                Logger::Add(L"[Qt] Keyboard hook install failed for blocked HID device: " + blockedName);
            }
        } else if (!blockedDeviceHookActive_) {
            Logger::Add(L"[Qt] Keyboard hook already active while blocked HID device is present.");
        }
        blockedDeviceHookActive_ = KeyboardHook::IsActive();
        return;
    }

    if (blockedDeviceHookActive_) {
        if (KeyboardHook::IsActive())
            KeyboardHook::Uninstall();
        blockedDeviceHookActive_ = false;
        Logger::Add(L"[Qt] Keyboard hook released; no blocked HID devices present.");
    } else if (!KeyboardHook::IsActive()) {
        // The user may have closed the buffer monitor while a blocked device is
        // still connected. Keep our cached state aligned with the actual hook.
        blockedDeviceHookActive_ = false;
    }
}

void MainWindow::replacePage(int pageIndex, QWidget* newPage)
{
    if (!pages_ || !newPage || pageIndex < 0 || pageIndex >= pages_->count())
        return;

    QWidget* oldPage = pages_->widget(pageIndex);
    pages_->removeWidget(oldPage);
    pages_->insertWidget(pageIndex, newPage);
    if (oldPage)
        oldPage->deleteLater();
}

void MainWindow::refreshLiveDataPages()
{
    if (!pages_)
        return;

    const int visiblePage = currentPage_;

    // Log: always use incremental append — never recreate the full page here.
    appendNewLogRows();

    // Tray widget: always sync with latest data (lightweight, no widget recreation)
    refreshTrayWidget();

    // Dashboard and USB list: only rebuild if currently visible.
    // If not visible, mark stale so switchPage() rebuilds on demand.
    if (visiblePage == 0) {
        replacePage(0, createDashboardPage());
        dashboardStale_ = false;
    } else {
        dashboardStale_ = true;
    }

    // USB list: refresh data in-place (no page recreation)
    if (visiblePage == 1) {
        refreshUsbTable();
    } else {
        usbListStale_ = true;
    }

    pages_->setCurrentIndex(visiblePage);
    refreshNavState();
}

void MainWindow::appendNewLogRows()
{
    if (!logTable_) return;

    const QString allLogs = fromWString(Logger::GetAll());
    const QStringList allLines = allLogs.split('\n', Qt::SkipEmptyParts);
    const int total = allLines.size();
    if (total <= lastLogLineCount_) return;

    auto iconForLine = [](const QString& line) -> QString {
        if (line.contains("block", Qt::CaseInsensitive) || line.contains("blacklist", Qt::CaseInsensitive) || line.contains("차단"))
            return ":/icons/status-blocked.svg";
        if (line.contains("allow", Qt::CaseInsensitive) || line.contains("whitelist", Qt::CaseInsensitive) || line.contains("허용"))
            return ":/icons/status-allowed.svg";
        if (line.contains("warning", Qt::CaseInsensitive) || line.contains("anomaly", Qt::CaseInsensitive) || line.contains("unknown", Qt::CaseInsensitive))
            return ":/icons/status-warning.svg";
        return ":/icons/status-info.svg";
    };
    auto badgeForLine = [this](const QString& line) -> std::pair<QString,QString> {
        if (line.contains("block", Qt::CaseInsensitive) || line.contains("blacklist", Qt::CaseInsensitive) || line.contains("차단"))
            return {tr("차단"), "blockBadge"};
        if (line.contains("allow", Qt::CaseInsensitive) || line.contains("whitelist", Qt::CaseInsensitive) || line.contains("허용"))
            return {tr("허용"), "successBadge"};
        if (line.contains("warning", Qt::CaseInsensitive) || line.contains("anomaly", Qt::CaseInsensitive) || line.contains("unknown", Qt::CaseInsensitive))
            return {tr("확인 필요"), "warningBadge"};
        return {tr("기록"), "neutralBadge"};
    };

    const int startRow = logTable_->rowCount();
    logTable_->setRowCount(startRow + (total - lastLogLineCount_));

    // Extract device name using same logic as createLogPage()
    auto extractDeviceAppend = [](const QString& line) -> QString {
        static const QStringList kPrefixes = {
            "approval: ", "hook active: ", "hook state: ",
            "block: ", "allow: ",
            "Policy removed: ", "Policy allow: ", "Policy block: ",
            "Auto-blocked (설정): ", "Auto-allowed (설정): ",
            "Buffer monitor allow: ", "Buffer monitor block: ",
        };
        for (const auto& p : kPrefixes) {
            const int idx = line.indexOf(p, 0, Qt::CaseInsensitive);
            if (idx != -1) {
                const QString candidate = line.mid(idx + p.length()).trimmed();
                if (!candidate.isEmpty()) return candidate;
            }
        }
        return "-";
    };

    int insertRow = startRow;
    for (int i = lastLogLineCount_; i < total; ++i) {
        const QString line = allLines[i].trimmed();
        if (line.isEmpty()) continue;
        const QString icon = iconForLine(line);
        const auto [result, badge] = badgeForLine(line);
        const QString ts     = nowTimestamp();
        const QString device = extractDeviceAppend(line);

        // Same contract as createLogPage(): store metadata as QObject properties
        // on the column-0 cell widget. No setItem() needed (matches USB table pattern).
        auto mkText = [&](const QString& text, const QString& color) -> QWidget* {
            auto* w = new QWidget;
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
            setCellWidgetBackground(w, cellBackground());
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            auto* lbl = new QLabel(text);
            lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            lbl->setObjectName(color == QLatin1String(UiTheme::tokens().Subtext) ? "logTextMuted" : "logTextMid");
            lbl->setFont(pretendardFont(FontRole::Regular));
            l->addWidget(lbl);
            l->addStretch();
            return w;
        };
        auto mkEvent = [&](const QString& iconRes, const QString& text) -> QWidget* {
            auto* w = new QWidget;
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
            setCellWidgetBackground(w, cellBackground());
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            l->setSpacing(8);
            auto* ico = new QLabel;
            ico->setAttribute(Qt::WA_TransparentForMouseEvents);
            ico->setObjectName("logCellIcon");
            ico->setFixedSize(16, 16);
            ico->setPixmap(QIcon(iconRes).pixmap(16, 16));
            l->addWidget(ico);
            auto* txt = new QLabel(text);
            txt->setAttribute(Qt::WA_TransparentForMouseEvents);
            txt->setObjectName("logEventText");
            txt->setFont(pretendardFont(FontRole::Medium));
            l->addWidget(txt);
            l->addStretch();
            return w;
        };

        const auto& t = UiTheme::tokens();
        auto* timeW = mkText(ts, t.Subtext);
        timeW->setProperty("log_icon",   icon);
        timeW->setProperty("log_event",  line);
        timeW->setProperty("log_time",   ts);
        timeW->setProperty("log_device", device);
        timeW->setProperty("log_result", result);
        timeW->setProperty("log_badge",  badge);
        logTable_->setCellWidget(insertRow, 0, timeW);
        logTable_->setCellWidget(insertRow, 1, mkEvent(icon, line));
        logTable_->setCellWidget(insertRow, 2, mkText(device, t.SubtextMid));
        logTable_->setCellWidget(insertRow, 3, makeBadgeCell(result, badge.toUtf8().constData()));
        ++insertRow;
    }
    // Trim any unfilled rows
    if (insertRow < logTable_->rowCount())
        logTable_->setRowCount(insertRow);

    lastLogLineCount_ = total;
    applyLogFilter();
    logTable_->scrollToBottom();
}

void MainWindow::applyLogFilter()
{
    if (!logTable_) return;
    const QString needle = logSearchText_.trimmed().toLower();
    const QString evtKey = logEventFilter_;
    for (int r = 0; r < logTable_->rowCount(); ++r) {
        QWidget* w = logTable_->cellWidget(r, 0);
        if (!w) { logTable_->setRowHidden(r, false); continue; }
        const QString event  = w->property("log_event").toString();
        const QString device = w->property("log_device").toString();
        const QString badge  = w->property("log_badge").toString();
        bool match = true;
        if (!needle.isEmpty())
            match = event.toLower().contains(needle) || device.toLower().contains(needle);
        if (match && !evtKey.isEmpty()) {
            if      (evtKey == "block")   match = (badge == "blockBadge");
            else if (evtKey == "allow")   match = (badge == "successBadge");
            else if (evtKey == "warning") match = (badge == "warningBadge");
            else if (evtKey == "info")    match = (badge == "neutralBadge");
        }
        logTable_->setRowHidden(r, !match);
    }
}

void MainWindow::exportLogToCsv()
{
    if (!logTable_) return;
    const QString defaultName = QStringLiteral("abang_all_log_") +
        QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss") + QStringLiteral(".csv");
    const QString path = QFileDialog::getSaveFileName(this,
        tr("내보내기 (CSV)"),
        QDir(QDir::homePath()).filePath(defaultName),
        QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, tr("내보내기 (CSV)"), tr("파일을 저장하지 못했습니다."));
        return;
    }
    auto csvEscape = [](QString s) {
        s.replace('"', "\"\"");
        if (s.contains(',') || s.contains('"') || s.contains('\n'))
            s = '"' + s + '"';
        return s;
    };
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    // UTF-8 BOM for Excel compatibility
    f.write("\xEF\xBB\xBF");
    out << tr("시간") << ',' << tr("이벤트") << ',' << tr("장치") << ',' << tr("처리 결과") << '\n';
    int exported = 0;
    for (int r = 0; r < logTable_->rowCount(); ++r) {
        if (logTable_->isRowHidden(r)) continue;
        QWidget* w = logTable_->cellWidget(r, 0);
        if (!w) continue;
        const QString time   = w->property("log_time").toString();
        const QString event  = w->property("log_event").toString();
        const QString device = w->property("log_device").toString();
        const QString result = w->property("log_result").toString();
        out << csvEscape(time) << ',' << csvEscape(event) << ',' << csvEscape(device) << ',' << csvEscape(result) << '\n';
        ++exported;
    }
    QMessageBox::information(this, tr("내보내기 (CSV)"),
        tr("%1개 항목을 저장했습니다.").arg(exported));
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType);
    auto* msg = static_cast<MSG*>(message);
    if (msg && msg->message == WM_DEVICECHANGE) {
        if (msg->wParam == DBT_DEVICEARRIVAL) {
            Logger::Add(L"[Qt] HID device arrival detected.");
            if (msg->lParam) {
                auto* hdr = reinterpret_cast<PDEV_BROADCAST_HDR>(msg->lParam);
                if (hdr->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
                    auto* di = reinterpret_cast<PDEV_BROADCAST_DEVICEINTERFACE_W>(msg->lParam);
                    const std::wstring path = di->dbcc_name ? di->dbcc_name : L"";
                    if (!path.empty()) {
                        // 1) Install hook immediately — captures keystrokes from the first moment.
                        if (!KeyboardHook::IsActive()) {
                            KeyboardHook::ClearBuffer();
                            KeyboardHook::Install();
                            Logger::Add(L"[Qt] Hook pre-installed; awaiting fingerprint.");
                        }

                        // 2) Show input buffer dialog immediately (before fingerprinting)
                        //    so the user sees captured keystrokes with no delay.
                        //    Device info shows "장치 확인 중..." until fingerprinting completes.
                        if (!g_activeBufferDlg) {
                            showBufferMonitorDialog(nullptr, UsbItem{}, [this] {
                                enforceBlockedDeviceHook();
                                refreshLiveDataPages();
                            }, [this] {
                                showNormal();
                                raise();
                                activateWindow();
                                switchPage(3);
                            });
                        }

                        // 3) Fingerprint in background. 150 ms sleep: DBT_DEVICEARRIVAL fires
                        //    before SetupAPI finishes registering the device; we need a brief
                        //    delay before HID descriptor queries succeed.
                        const int mySerial = removalSerial_;
                        std::thread([this, path, mySerial] {
                            std::this_thread::sleep_for(std::chrono::milliseconds(150));
                            Policy::Load();
                            const UsbItem item = HidFingerprint::BuildUsbItemFromPath(path);
                            QMetaObject::invokeMethod(this, [this, item, mySerial] {
                                if (mySerial != removalSerial_) {
                                    // Device removed while thread slept — close any open dialog.
                                    if (g_activeBufferDlg) g_activeBufferDlg->close();
                                    enforceBlockedDeviceHook();
                                    return;
                                }

                                // Helper: push real device info into the already-open dialog.
                                auto updateDlgInfo = [&] {
                                    if (g_activeBufferDlg) {
                                        const QString name = fromWString(
                                            item.name.empty() ? L"Unknown USB Device" : item.name);
                                        const QString vp = QString("0x%1 / 0x%2")
                                                               .arg(hex4(item.vid), hex4(item.pid));
                                        if (auto* l = g_activeBufferDlg->findChild<QLabel*>("dlg_device_name"))
                                            l->setText(name);
                                        if (auto* l = g_activeBufferDlg->findChild<QLabel*>("dlg_vid_pid"))
                                            l->setText(vp);
                                    }
                                    if (g_dlgSharedDevice) *g_dlgSharedDevice = item;
                                };

                                if (item.fingerprint != 0) {
                                    const Policy::PolicyStatus policyStatus = Policy::Evaluate(item);
                                    if (policyStatus == Policy::PolicyStatus::Whitelisted) {
                                        // Trusted — silently close the pre-opened dialog.
                                        Logger::Add(L"[Qt] Whitelisted; closing immediate dialog: " + item.name);
                                        if (g_activeBufferDlg) g_activeBufferDlg->close();
                                        enforceBlockedDeviceHook();
                                    } else if (policyStatus == Policy::PolicyStatus::Blacklisted) {
                                        Logger::Add(L"[Qt] Blocked HID device; hook active: " + item.name);
                                        updateDlgInfo();
                                        blockCurrentDevice(item, L"Reconnect blacklist");
                                        if (notifyOnRiskEnabled() && trayIcon_) {
                                            showRiskTrayMessage(
                                                trayIcon_,
                                                QStringLiteral("Abang-All"),
                                                tr("차단된 장치가 연결되었습니다.\n%1").arg(fromWString(item.name)),
                                                QSystemTrayIcon::Warning);
                                        }
                                        if (!KeyboardHook::IsActive()) KeyboardHook::Install();
                                        blockedDeviceHookActive_ = true;
                                    } else {
                                        // Unknown device — analyse-then-prompt is the only mode.
                                        // The buffer monitor dialog is already open from step 2;
                                        // just push the real device info in.
                                        Logger::Add(L"[Qt] Unknown HID device: " + item.name);
                                        updateDlgInfo();
                                        if (notifyOnRiskEnabled() && trayIcon_) {
                                            showRiskTrayMessage(
                                                trayIcon_,
                                                QStringLiteral("Abang-All"),
                                                tr("승인 대기 장치가 연결되었습니다.\n%1").arg(fromWString(item.name)),
                                                QSystemTrayIcon::Warning);
                                        }
                                        lastPromptFingerprint_ = item.fingerprint;
                                    }
                                } else {
                                    enforceBlockedDeviceHook();
                                }
                            }, Qt::QueuedConnection);
                        }).detach();
                    }
                }
            }
            // Defer heavy UI rebuild so the Windows message pump is not blocked.
            QTimer::singleShot(200, this, [this] { refreshLiveDataPages(); });
            if (result) *result = 0;
            return false;
        }

        if (msg->wParam == DBT_DEVICEREMOVECOMPLETE) {
            Logger::Add(L"[Qt] HID device removal detected.");
            lastPromptFingerprint_ = 0;
            // Increment serial so any background fingerprint thread that was started
            // before this removal can detect it is now stale and skip showing a dialog.
            ++removalSerial_;
            // Single deferred rebuild — 300ms gives the OS time to fully remove the device
            // before we re-enumerate HID paths and check the hook state.
            QTimer::singleShot(300, this, [this] {
                enforceBlockedDeviceHook();
                refreshLiveDataPages();
            });
            if (result) *result = 0;
            return false;
        }
    }

    if (msg && msg->message == WM_APP_NEWLOG) {
        QTimer::singleShot(0, this, [this] { appendNewLogRows(); });
        if (result) *result = 0;
        return true;
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::switchPage(int pageIndex)
{
    if (!pages_ || pageIndex < 0 || pageIndex >= pages_->count())
        return;

    // Lazy rebuild: if the page was marked stale while the user was elsewhere,
    // rebuild it now before showing.
    if (pageIndex == 0 && dashboardStale_) {
        replacePage(0, createDashboardPage());
        dashboardStale_ = false;
    } else if (pageIndex == 1 && usbListStale_) {
        refreshUsbTable();  // data-only, no page recreation
    } else if (pageIndex == 2) {
        // Log page is always up-to-date via appendNewLogRows(); no rebuild needed.
        appendNewLogRows();
    }

    currentPage_ = pageIndex;
    pages_->setCurrentIndex(pageIndex);
    refreshNavState();
}

void MainWindow::refreshNavState()
{
    for (int i = 0; i < navButtons_.size(); ++i) {
        const bool active = (i == currentPage_);
        navButtons_[i]->setProperty("active", active);
        navButtons_[i]->style()->unpolish(navButtons_[i]);
        navButtons_[i]->style()->polish(navButtons_[i]);
    }
    for (int i = 0; i < topNavButtons_.size(); ++i) {
        const bool active = (i == currentPage_);
        topNavButtons_[i]->setProperty("active", active);
        topNavButtons_[i]->style()->unpolish(topNavButtons_[i]);
        topNavButtons_[i]->style()->polish(topNavButtons_[i]);
    }
}
