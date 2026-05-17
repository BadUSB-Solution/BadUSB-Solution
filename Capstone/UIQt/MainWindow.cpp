// UTF-8
#include "MainWindow.h"
#include "Theme.h"

#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QApplication>
#include <QMenu>
#include <QRegularExpression>
#include <QSettings>
#include <QDateTime>
#include <QDialog>
#include <QFrame>
#include <QFont>
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
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSize>
#include <QSpacerItem>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStringList>
#include <QStyle>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbt.h>
#include <hidsdi.h>
#endif

#include "TrayWidget.h"
#include "hid_fingerprint.h"
#include "hid_scan.h"
#include "compound_scorer.h"
#include "hotkey_scorer.h"
#include "key_decoder.h"
#include "keyboard_hook.h"
#include "logger.h"
#include "policy.h"
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
    enum class FontRole { Regular, Medium, SemiBold, Bold };

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
            ? QStringLiteral("선택한 장치")
            : deviceName.trimmed();

        QDialog dlg(parent);
        dlg.setModal(true);
        dlg.setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
        dlg.setFixedWidth(438);
        dlg.setObjectName("confirmPolicyDialog");
        dlg.setStyleSheet(QString(
            "QDialog#confirmPolicyDialog { background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 12px; }"
            "QLabel#confirmTitle { color: #0F172A; font-size: 17px; font-weight: 700; background: transparent; }"
            "QLabel#confirmSubtitle { color: #475569; font-size: 12px; background: transparent; }"
            "QLabel#confirmDevice { color: #0F172A; font-size: 14px; font-weight: 600; background: #F8FAFC;"
            " border: 1px solid #E2E8F0; border-radius: 8px; padding: 10px 12px; }"
            "QLabel#confirmBody { color: #475569; font-size: 13px; line-height: 150%; background: transparent; }"
            "QPushButton#confirmClose { color: #64748B; background: transparent; border: none; font-size: 18px; }"
            "QPushButton#confirmClose:hover { color: #0F172A; }"
            "QPushButton#confirmCancel { color: #334155; background: #FFFFFF; border: 1px solid #CBD5E1;"
            " border-radius: 8px; padding: 9px 18px; font-size: 13px; font-weight: 600; }"
            "QPushButton#confirmCancel:hover { background: #F8FAFC; border-color: #94A3B8; }"
            "QPushButton#confirmAction { color: #FFFFFF; background: %1; border: none;"
            " border-radius: 8px; padding: 9px 22px; font-size: 13px; font-weight: 700; }"
            "QPushButton#confirmAction:hover { background: %2; }"
        ).arg(allow ? UiTheme::Primary : UiTheme::DangerAction,
              allow ? UiTheme::PrimaryHover : UiTheme::DangerHover));

        auto* root = new QVBoxLayout(&dlg);
        root->setContentsMargins(22, 20, 22, 20);
        root->setSpacing(16);

        auto* header = new QHBoxLayout;
        header->setSpacing(12);

        auto* iconWrap = new QFrame;
        iconWrap->setFixedSize(42, 42);
        iconWrap->setStyleSheet(QString(
            "QFrame { background: %1; border: 1px solid %2; border-radius: 10px; }"
        ).arg(allow ? UiTheme::PrimarySoft : "#FFF1F2",
              allow ? UiTheme::PrimarySoftBorder : "#FECDD3"));
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
        auto* title = new QLabel(allow ? QStringLiteral("장치 허용 확인")
                                       : QStringLiteral("장치 차단 확인"));
        title->setObjectName("confirmTitle");
        auto* subtitle = new QLabel(allow ? QStringLiteral("신뢰 목록에 등록하기 전에 한 번 더 확인합니다.")
                                          : QStringLiteral("차단 정책을 적용하기 전에 한 번 더 확인합니다."));
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
            ? QStringLiteral("허용된 장치는 이후 연결 시 키보드 후킹과 입력 차단이 적용되지 않습니다.\n신뢰할 수 있는 장치인지 확인한 뒤 진행하세요.")
            : QStringLiteral("차단된 장치는 블랙리스트에 등록되며 이후 연결 시 입력 차단 상태가 적용됩니다.\n현재 사용 중인 정상 장치가 아닌지 확인한 뒤 진행하세요."));
        body->setObjectName("confirmBody");
        body->setWordWrap(true);
        root->addWidget(body);

        auto* buttons = new QHBoxLayout;
        buttons->addStretch();
        auto* cancelBtn = new QPushButton(QStringLiteral("취소"));
        cancelBtn->setObjectName("confirmCancel");
        cancelBtn->setCursor(Qt::PointingHandCursor);
        auto* actionBtn = new QPushButton(allow ? QStringLiteral("허용")
                                                : QStringLiteral("차단"));
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
            return QStringLiteral("허용 목록에 저장했습니다. VID/PID 정보가 없어 장치 재활성화는 건너뛰었습니다.");
        }

        std::wstring enableDetail;
        const DeviceBlocker::EnableResult enableResult =
            DeviceBlocker::EnableByVidPid(item.vid, item.pid, &enableDetail);
        Logger::Add(L"[Qt] " + source + L" re-enable result=" +
            std::to_wstring(static_cast<int>(enableResult)) + L", " + enableDetail);

        switch (enableResult) {
        case DeviceBlocker::EnableResult::Enabled:
            return QStringLiteral("허용 목록에 저장하고 장치 재활성화를 요청했습니다.");
        case DeviceBlocker::EnableResult::NotFound:
            return QStringLiteral("허용 목록에 저장했습니다. 현재 재활성화할 장치를 찾지 못했습니다.");
        case DeviceBlocker::EnableResult::Failed:
        default:
            return QStringLiteral("허용 목록에 저장했지만 장치 재활성화 요청은 실패했습니다.");
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

    bool settingEnabled(const QString& key, bool fallback = true)
    {
        QSettings settings("Abang-All", "Abang-All");
        return settings.value(key, fallback).toBool();
    }

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

    double scoreTypingResult(const TypingAnalyzer::Result& typing)
    {
        if (typing.intervalCount < 5)
            return 0.0;

        double score = 0.0;
        if (typing.classification == TypingAnalyzer::TYPING_AUTOMATED) {
            score = 6.0;
        } else if (typing.classification == TypingAnalyzer::TYPING_HUMAN) {
            score = 1.0;
        }

        if (typing.medianMs < 30.0)
            score += 2.0;
        else if (typing.medianMs < 50.0)
            score += 1.0;

        if (typing.madMs < 5.0)
            score += 2.0;
        else if (typing.madMs < 12.0)
            score += 1.0;

        return std::clamp(score, 0.0, 10.0);
    }

    double scoreUrlFlags(uint32_t flags)
    {
        double score = 0.0;
        const auto has = [flags](UrlClassifier::UrlFlag flag) {
            return (flags & static_cast<uint32_t>(flag)) != 0;
        };

        if (has(UrlClassifier::URL_FLAG_EXE_EXTENSION))   score += 3.0;
        if (has(UrlClassifier::URL_FLAG_HIGH_ENTROPY))    score += 2.0;
        if (has(UrlClassifier::URL_FLAG_ABNORMAL_SCHEME)) score += 2.0;
        if (has(UrlClassifier::URL_FLAG_UNUSUAL_PORT))    score += 2.0;
        if (has(UrlClassifier::URL_FLAG_SHORTENER))       score += 2.0;
        if (has(UrlClassifier::URL_FLAG_IP_ADDRESS))      score += 1.5;
        if (has(UrlClassifier::URL_FLAG_FREE_TLD))        score += 2.0;
        if (has(UrlClassifier::URL_FLAG_DYNAMIC_DNS))     score += 2.0;
        if (has(UrlClassifier::URL_FLAG_TUNNELING))       score += 2.0;
        if (has(UrlClassifier::URL_FLAG_FILE_SHARING))    score += 1.5;
        if (has(UrlClassifier::URL_FLAG_FREE_HOSTING))    score += 1.0;

        return std::clamp(score, 0.0, 10.0);
    }

    double scoreUrlResult(const UrlClassifier::Result& urls)
    {
        double maxScore = 0.0;
        for (const auto& item : urls.items)
            maxScore = (std::max)(maxScore, scoreUrlFlags(item.flags));
        return maxScore;
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
        snapshot.typingScore = scoreTypingResult(snapshot.typing);
        if (snapshot.typing.meanMs > 0.0)
            snapshot.keysPerSecond = 1000.0 / snapshot.typing.meanMs;
        else if (snapshot.averageIntervalMs > 0.0)
            snapshot.keysPerSecond = 1000.0 / snapshot.averageIntervalMs;

        snapshot.decoded = KeyDecoder::Decode(keys);
        snapshot.urls = UrlClassifier::Classify(snapshot.decoded);
        snapshot.urlScore = scoreUrlResult(snapshot.urls);
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
            reasons << QStringLiteral("자동 입력으로 의심되는 빠르고 균일한 키 입력이 감지되었습니다.");
        }
        if (!snapshot.threat.commands.empty() || snapshot.threat.session_score >= 3.0) {
            reasons << QStringLiteral("실행/명령 입력 패턴이 감지되었습니다.");
        }
        if (!snapshot.hotkey.matches.empty() || snapshot.hotkey.final_score >= 3.0) {
            reasons << QStringLiteral("위험 단축키 조합이 포함되어 있습니다.");
        }
        if (!snapshot.urls.items.empty() && snapshot.urlScore > 0.0) {
            reasons << QStringLiteral("의심 URL 또는 실행 파일 URL 패턴이 포함되어 있습니다.");
        }
        if (reasons.isEmpty()) {
            reasons << QStringLiteral("여러 입력 신호가 조합되어 위험 점수가 상승했습니다.");
        }
        return reasons;
    }

    QString blockReasonHtml(const QStringList& reasons)
    {
        QString html;
        for (const QString& reason : reasons) {
            html += QStringLiteral(
                "<div style='margin-bottom:4px;'>"
                "<span style='color:#F97316; font-weight:800;'>•</span>"
                "<span style='color:#45556C;'>&nbsp;&nbsp;%1</span>"
                "</div>").arg(reason.toHtmlEscaped());
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

    QString policyTextForFingerprint(uint64_t fp)
    {
        if (Policy::IsWhitelisted(fp)) return "허용";
        if (Policy::IsBlacklisted(fp)) return "차단";
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
            return "입력 대기 중...\n미등록 HID 장치에서 키 입력이 감지되면 이 영역에 실시간으로 표시됩니다.";

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
                lines << QString("[ANOMALY] 입력 속도 임계값 초과: %1ms/key (임계값 50ms)")
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
            return "입력 대기 중...\n16진수 형식으로 표시됩니다.";

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
        QString family = QApplication::font().family();
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
        icon->setPixmap(QIcon(resource).pixmap(size, size));
        icon->setAlignment(Qt::AlignCenter);
        icon->setAttribute(Qt::WA_TransparentForMouseEvents);
        return icon;
    }

    void setCellWidgetBackground(QWidget* widget, const QColor& color)
    {
        if (!widget) return;
        widget->setAutoFillBackground(true);
        QPalette pal = widget->palette();
        pal.setColor(QPalette::Window, color);
        widget->setPalette(pal);
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
        badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        badge->setAttribute(Qt::WA_TransparentForMouseEvents);
        auto* container = new QWidget;
        container->setAttribute(Qt::WA_TransparentForMouseEvents);
        setCellWidgetBackground(container, Qt::white);
        auto* l = new QHBoxLayout(container);
        l->setContentsMargins(10, 0, 10, 0);
        l->addWidget(badge, 0, Qt::AlignLeft | Qt::AlignVCenter);
        l->addStretch();
        return container;
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
        setCellWidgetBackground(w, Qt::white);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(10, 0, 6, 0);
        l->setSpacing(6);
        auto* dot = new QFrame;
        dot->setFixedSize(8, 8);
        dot->setStyleSheet(QString("QFrame { background: %1; border-radius: 4px; border: none; }")
            .arg(status == "연결됨" ? "#00C950" : "#CAD5E2"));
        auto* txt = new QLabel(status);
        txt->setAttribute(Qt::WA_TransparentForMouseEvents);
        txt->setFont(pretendardFont(FontRole::Regular));
        txt->setStyleSheet("QLabel { color: #45556C; font-size: 14px; border: none; background: transparent; }");
        l->addWidget(dot);
        l->addWidget(txt);
        l->addStretch();
        return w;
    }

    QWidget* makeUsbTextCell(const QString& text, const QString& color, const QFont& font, int leftPadding = 10)
    {
        auto* w = new QWidget;
        w->setAttribute(Qt::WA_TransparentForMouseEvents);
        setCellWidgetBackground(w, Qt::white);
        auto* l = new QHBoxLayout(w);
        l->setContentsMargins(leftPadding, 0, 8, 0);
        l->setSpacing(0);
        auto* txt = new QLabel(text);
        txt->setAttribute(Qt::WA_TransparentForMouseEvents);
        txt->setFont(font);
        txt->setStyleSheet(QString("QLabel { color: %1; font-size: 14px; border: none; background: transparent; }").arg(color));
        l->addWidget(txt);
        l->addStretch();
        return w;
    }

    // Search box with leading icon
    QWidget* makeSearchBox(const QString& placeholder, int w = 256)
    {
        auto* box = new QWidget;
        box->setFixedHeight(38);
        if (w > 0) box->setFixedWidth(w);
        box->setStyleSheet("QWidget { background: white; border: 1px solid #CAD5E2; border-radius: 8px; }");
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
        input->setStyleSheet("QLineEdit { background: transparent; border: none; font-size: 14px; color: #1D293D; }");
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
            p.setBrush(on_ ? QColor(UiTheme::Primary) : QColor(UiTheme::BorderInput));
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
            const int lm = 42, rm = 20, tm = 14, bm = 30;
            const int cw = width() - lm - rm;
            const int ch = height() - tm - bm;

            // Grid lines + Y labels
            const QStringList yLabels = {"0", "5", "10", "15", "20"};
            for (int i = 0; i < 5; ++i) {
                const double ratio = i / 4.0;
                const int y = tm + ch - static_cast<int>(ratio * ch);
                p.setPen(QPen(QColor("#E2E8F0"), 1, Qt::DashLine));
                p.drawLine(lm, y, lm + cw, y);
                p.setPen(QColor("#94A3B8"));
                QFont f("Inter", 9); p.setFont(f);
                p.drawText(0, y - 7, lm - 4, 14, Qt::AlignRight | Qt::AlignVCenter, yLabels[i]);
            }

            if (data.isEmpty()) {
                p.setPen(QColor("#94A3B8"));
                QFont emptyFont("Inter", 10);
                p.setFont(emptyFont);
                p.drawText(lm, tm, cw, ch, Qt::AlignCenter, "입력 대기중...");
            }

            const int thresholdY = tm + ch - static_cast<int>((8.0 / maxV) * ch);
            p.setPen(QPen(QColor("#94A3B8"), 1.3, Qt::DashLine));
            p.drawLine(lm, thresholdY, lm + cw, thresholdY);

            // Input speed line
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
                p.setPen(QPen(QColor(UiTheme::ChartRisk), 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
                p.drawPath(linePath);

                for (int i = 0; i < pts.size(); ++i) {
                    const bool anom = data[i].second;
                    p.setPen(QPen(QColor("#FFFFFF"), 2));
                    p.setBrush(QColor(anom ? "#7C3AED" : UiTheme::ChartRisk));
                    p.drawEllipse(pts[i], anom ? 4.5 : 3.5, anom ? 4.5 : 3.5);
                }
            }

            // X-axis labels
            const QStringList xLabels = {"0초", "5초", "10초", "15초", "20초", "25초", "30초"};
            p.setPen(QColor("#94A3B8"));
            QFont xf("Inter", 9); p.setFont(xf);
            for (int i = 0; i < xLabels.size(); ++i) {
                const double x = lm + (i / 6.0) * cw;
                p.drawText(static_cast<int>(x) - 20, tm + ch + 6, 40, 16,
                           Qt::AlignCenter, xLabels[i]);
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
                p.setPen(QPen(QColor("#E2E8F0"), 1));
                p.drawLine(lm, y, lm + cw, y);
                p.setPen(QColor("#94A3B8"));
                QFont f("Inter", 9); p.setFont(f);
                p.drawText(0, y - 7, lm - 4, 14, Qt::AlignRight | Qt::AlignVCenter, yLbls[i]);
            }

            // Threshold dashed line at 8.0
            {
                const int ty = tm + ch - static_cast<int>((8.0 / maxV) * ch);
                p.setPen(QPen(QColor(UiTheme::ChartThreshold), 1.5, Qt::DashLine));
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
            p.setPen(QPen(QColor(UiTheme::ChartRisk), 2));
            for (int i = 1; i < pts.size(); ++i)
                p.drawLine(pts[i-1], pts[i]);

            // Crossing dot near index 15 (14:05)
            if (!pts.isEmpty() && *std::max_element(data.cbegin(), data.cend()) > 0.05) {
                const int cpIndex = std::distance(data.cbegin(), std::max_element(data.cbegin(), data.cend()));
                const QPointF& cp = pts[std::clamp(cpIndex, 0, static_cast<int>(pts.size()) - 1)];
                p.setPen(QPen(QColor("#FFFFFF"), 2));
                p.setBrush(QColor(UiTheme::ChartThreshold));
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
                {QStringLiteral("명령어"), std::clamp(command, 0.0, 10.0), QColor("#2563C7")},
                {QStringLiteral("URL"), std::clamp(url, 0.0, 10.0), QColor("#7C3AED")},
                {QStringLiteral("입력 속도"), std::clamp(typing, 0.0, 10.0), QColor("#0891B2")},
                {QStringLiteral("핫키"), std::clamp(hotkey, 0.0, 10.0), QColor("#DB2777")},
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
                p.setPen(QColor("#94A3B8"));
                p.setFont(QFont("Inter", 10));
                p.drawText(rect(), Qt::AlignCenter, QStringLiteral("분석 대기 중..."));
                return;
            }

            for (int i = 0; i < scores_.size(); ++i) {
                const auto& item = scores_[i];
                const int y = top + i * (rowH + gap);
                const int barY = y + 12;

                p.setPen(QColor("#45556C"));
                p.setFont(QFont("Inter", 10, QFont::DemiBold));
                p.drawText(0, y, leftLabelW, rowH, Qt::AlignRight | Qt::AlignVCenter, item.label);

                p.setPen(Qt::NoPen);
                p.setBrush(QColor("#EAF2FF"));
                p.drawRoundedRect(QRectF(barX, barY, barW, barH), 6, 6);

                const double filled = (item.value / 10.0) * barW;
                p.setBrush(item.color);
                p.drawRoundedRect(QRectF(barX, barY, filled, barH), 6, 6);

                p.setPen(QColor("#1D293D"));
                p.setFont(QFont("Inter", 10, QFont::DemiBold));
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
            : QStringLiteral("장치 확인 중...");
        const QString vidPid = hasDevice
            ? QString("0x%1 / 0x%2").arg(hex4(device.vid), hex4(device.pid))
            : QStringLiteral("분석 중...");
        const QString detectedAt = nowTimestamp();

        auto* dlg = new QDialog(parent, Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        // Non-modal: WindowStaysOnTopHint keeps it visible.
        // setModal(true) was blocking all input to the main window even when
        // the dialog was opened manually without a suspected device.
        dlg->setModal(false);
        dlg->setFixedSize(1024, 790);
        dlg->setStyleSheet(
            QString(
                "QDialog { background: #FFFFFF; border-radius: 12px; }"
                "QFrame#dlgCard { background: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 8px; }"
                "QFrame#dlgInfoRow { border-bottom: 1px solid #F1F5F9; }"
                "QTextEdit { background: %2; color: #E2E8F0; font-family: Consolas, monospace;"
                "            font-size: 12px; border: none; padding: 12px; }"
                "QCheckBox { color: #45556C; font-size: 13px; spacing: 6px; }"
                "QCheckBox::indicator { width: 15px; height: 15px; border: 1.5px solid #CAD5E2;"
                "                       border-radius: 4px; background: white; }"
                "QCheckBox::indicator:checked { background: %1; border-color: %1;"
                "                               image: url(:/icons/check-white.svg); }"
            ).arg(UiTheme::Primary, UiTheme::TerminalBg)
        );

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
        hdr->setStyleSheet("QWidget { background: #FFFFFF; border-bottom: 1px solid #E2E8F0; }");
        hdr->installEventFilter(new DialogDragFilter(dlg));
        auto* hdrL = new QHBoxLayout(hdr);
        hdrL->setContentsMargins(24, 0, 16, 0);
        hdrL->setSpacing(10);
        hdrL->addWidget(iconLabel(":/icons/brand-mark.png", 20));
        auto* title = new QLabel("장치 승인 및 버퍼 모니터");
        title->setFont(pretendardFont(FontRole::SemiBold));
        title->setStyleSheet("font-size: 16px; color: #1D293D;");
        title->setAttribute(Qt::WA_TransparentForMouseEvents);
        hdrL->addWidget(title);
        hdrL->addStretch();
        auto* closeBtn = new QPushButton("✕");
        closeBtn->setFixedSize(34, 34);
        closeBtn->setStyleSheet(
            "QPushButton { background: #F1F5F9; border: none; border-radius: 17px;"
            "              color: #62748E; font-size: 14px; }"
            "QPushButton:hover { background: #E2E8F0; color: #1D293D; }"
        );
        QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
        hdrL->addWidget(closeBtn);
        root->addWidget(hdr);

        // ── Body scroll area ─────────────────────────────────────────────────
        auto* body = new QWidget;
        auto* bodyL = new QVBoxLayout(body);
        bodyL->setContentsMargins(24, 16, 24, 0);
        bodyL->setSpacing(12);
        QTextEdit* bufText = nullptr;
        auto normalizedMode = std::make_shared<bool>(false);
        std::function<void()> updateBufferText;
        QFrame* blockReasonCard = nullptr;
        QLabel* blockScoreValue = nullptr;
        QLabel* blockThresholdValue = nullptr;
        QLabel* blockReasonList = nullptr;
        std::function<void()> updateBlockReasonCard;

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
                ch->setStyleSheet("background: #FFFFFF; border-bottom: 1px solid #E2E8F0;"
                                  "border-top-left-radius: 8px; border-top-right-radius: 8px;");
                auto* chl = new QHBoxLayout(ch);
                chl->setContentsMargins(16, 0, 16, 0);
                auto* chTitle = new QLabel("연결된 장치 정보");
                chTitle->setFont(pretendardFont(FontRole::SemiBold));
                chTitle->setStyleSheet("font-size: 13px; color: #314158; background: transparent; border: none;");
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
                    k->setStyleSheet("color: #62748E; font-size: 13px;");
                    k->setFixedWidth(100);
                    auto* v = new QLabel(val);
                    v->setFont(pretendardFont(FontRole::Medium));
                    v->setStyleSheet("color: #1D293D; font-size: 13px;");
                    if (!objName.isEmpty()) v->setObjectName(objName);
                    rl->addWidget(k);
                    rl->addWidget(v, 1);
                    cbl->addWidget(row);
                    return v;
                };

                addDevRow("장치 이름",  deviceName, "dlg_device_name");
                addDevRow("장치 유형",  "HID (Human Interface Device)");
                addDevRow("VID / PID",  vidPid, "dlg_vid_pid");
                addDevRow("연결 포트",  "HID Interface");
                addDevRow("첫 감지 시간", detectedAt);

                // 상태 badge row
                auto* stRow = new QFrame;
                stRow->setStyleSheet("QFrame { border: none; }");
                auto* stL = new QHBoxLayout(stRow);
                stL->setContentsMargins(0, 8, 0, 0);
                stL->setSpacing(0);
                auto* stKey = new QLabel("상태");
                stKey->setFont(pretendardFont(FontRole::Regular));
                stKey->setStyleSheet("color: #62748E; font-size: 13px;");
                stKey->setFixedWidth(100);
                auto* stBadge = new QLabel("자동 차단됨");
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
                ch->setStyleSheet("background: #FFFFFF; border-bottom: 1px solid #E2E8F0;"
                                  "border-top-left-radius: 8px; border-top-right-radius: 8px;");
                auto* chl = new QHBoxLayout(ch);
                chl->setContentsMargins(16, 0, 16, 0);
                auto* chTitle = new QLabel("승인/차단 안내");
                chTitle->setFont(pretendardFont(FontRole::SemiBold));
                chTitle->setStyleSheet("font-size: 13px; color: #314158; background: transparent; border: none;");
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
                    d->setStyleSheet("color: #62748E; font-size: 12px;");
                    d->setWordWrap(true);
                    texts->addWidget(t);
                    texts->addWidget(d);
                    row->addLayout(texts, 1);
                    cbl->addLayout(row);
                };

                addGuideRow(":/icons/status-allowed.svg", "#2563C7", "허용",
                            "이 장치를 신뢰할 수 있는 장치로 등록하고 연결을 허용합니다.");
                addGuideRow(":/icons/status-blocked.svg", "#E93729", "차단",
                            "이 장치를 차단하고 연결을 즉시 종료합니다.");

                cbl->addStretch();

                cl->addWidget(cbody, 1);
                topRow->addWidget(card, 1);
            }

            bodyL->addLayout(topRow);
        }

        // Block recommendation card
        {
            auto* card = new QFrame;
            blockReasonCard = card;
            card->setStyleSheet(
                "QFrame { background: #FFFFFF; border: 1px solid #FED7AA; border-radius: 8px; }"
            );
            auto* cl = new QVBoxLayout(card);
            cl->setContentsMargins(0, 0, 0, 0);
            cl->setSpacing(0);

            auto* ch = new QWidget;
            ch->setFixedHeight(38);
            ch->setStyleSheet(
                "background: #FFFFFF; border-bottom: 1px solid #FED7AA;"
                "border-top-left-radius: 8px; border-top-right-radius: 8px;"
            );
            auto* chl = new QHBoxLayout(ch);
            chl->setContentsMargins(16, 0, 16, 0);
            chl->setSpacing(10);
            auto* title = new QLabel(QStringLiteral("차단 권장 사유"));
            title->setFont(pretendardFont(FontRole::SemiBold));
            title->setStyleSheet("font-size: 14px; color: #1D293D; background: transparent; border: none;");
            chl->addWidget(title);
            chl->addStretch();
            auto* badge = new QLabel(QStringLiteral("차단 권장"));
            badge->setFont(pretendardFont(FontRole::SemiBold));
            badge->setFixedHeight(24);
            badge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
            badge->setStyleSheet(
                "QLabel { color: #EA580C; background: #FFF7ED; border: 1px solid #FDBA74;"
                " border-radius: 7px; padding: 0 10px; font-size: 12px; }"
            );
            chl->addWidget(badge);
            cl->addWidget(ch);

            auto* content = new QWidget;
            content->setStyleSheet("background: transparent; border: none;");
            auto* contentL = new QHBoxLayout(content);
            contentL->setContentsMargins(16, 10, 16, 10);
            contentL->setSpacing(18);

            auto* left = new QVBoxLayout;
            left->setSpacing(8);
            auto* summary = new QLabel(QStringLiteral("종합 위험 점수가 임계값을 초과했습니다."));
            summary->setFont(pretendardFont(FontRole::SemiBold));
            summary->setStyleSheet("color: #1D293D; font-size: 13px; background: transparent; border: none;");
            left->addWidget(summary);

            auto* scoreBox = new QFrame;
            scoreBox->setStyleSheet(
                "QFrame { background: #FFFDFB; border: 1px solid #E2E8F0; border-radius: 8px; }"
            );
            auto* scoreL = new QHBoxLayout(scoreBox);
            scoreL->setContentsMargins(18, 7, 18, 7);
            scoreL->setSpacing(18);

            auto makeScoreCol = [&](const QString& key, QLabel** out, bool accent) {
                auto* col = new QVBoxLayout;
                col->setSpacing(4);
                auto* k = new QLabel(key);
                k->setFont(pretendardFont(FontRole::Regular));
                k->setStyleSheet("color: #45556C; font-size: 12px; background: transparent; border: none;");
                auto* v = new QLabel(QStringLiteral("-"));
                v->setFont(pretendardFont(FontRole::Bold));
                v->setStyleSheet(QString("color: %1; font-size: 18px; background: transparent; border: none;")
                    .arg(accent ? "#F97316" : "#1D293D"));
                col->addWidget(k, 0, Qt::AlignCenter);
                col->addWidget(v, 0, Qt::AlignCenter);
                scoreL->addLayout(col, 1);
                *out = v;
            };
            makeScoreCol(QStringLiteral("현재 점수:"), &blockScoreValue, true);
            auto* divider = new QFrame;
            divider->setFixedWidth(1);
            divider->setStyleSheet("QFrame { background: #CBD5E1; border: none; }");
            scoreL->addWidget(divider);
            makeScoreCol(QStringLiteral("임계값:"), &blockThresholdValue, true);
            left->addWidget(scoreBox);
            contentL->addLayout(left, 1);

            auto* midDivider = new QFrame;
            midDivider->setFixedWidth(1);
            midDivider->setStyleSheet("QFrame { background: #E2E8F0; border: none; }");
            contentL->addWidget(midDivider);

            auto* right = new QVBoxLayout;
            right->setSpacing(6);
            auto* reasonTitle = new QLabel(QStringLiteral("주요 근거"));
            reasonTitle->setFont(pretendardFont(FontRole::SemiBold));
            reasonTitle->setStyleSheet("color: #1D293D; font-size: 13px; background: transparent; border: none;");
            right->addWidget(reasonTitle);
            blockReasonList = new QLabel;
            blockReasonList->setFont(pretendardFont(FontRole::Regular));
            blockReasonList->setTextFormat(Qt::RichText);
            blockReasonList->setWordWrap(true);
            blockReasonList->setStyleSheet("font-size: 11px; background: transparent; border: none;");
            right->addWidget(blockReasonList);
            right->addStretch();
            contentL->addLayout(right, 1);

            cl->addWidget(content);
            card->setVisible(false);
            bodyL->addWidget(card);
        }

        // Buffer card
        {
            auto* card = new QFrame;
            card->setStyleSheet("QFrame { background: #FFFFFF; border: 1px solid #E2E8F0; border-radius: 8px; }");
            auto* cl = new QVBoxLayout(card);
            cl->setContentsMargins(0, 0, 0, 0);
            cl->setSpacing(0);

            // Card header
            auto* ch = new QWidget;
            ch->setFixedHeight(44);
            ch->setStyleSheet("background: #F8FAFC; border-bottom: 1px solid #E2E8F0;"
                              "border-top-left-radius: 8px; border-top-right-radius: 8px;");
            auto* chl = new QHBoxLayout(ch);
            chl->setContentsMargins(16, 0, 12, 0);
            auto* chTitle = new QLabel("입력 버퍼 (읽기 전용)");
            chTitle->setFont(pretendardFont(FontRole::SemiBold));
            chTitle->setStyleSheet("font-size: 13px; color: #314158; background: transparent; border: none;");
            chl->addWidget(chTitle);
            chl->addStretch();
            auto* clearBtn = new QPushButton("지우기");
            clearBtn->setObjectName("secondaryButton");
            clearBtn->setFont(pretendardFont(FontRole::Medium));
            clearBtn->setFixedHeight(30);
            clearBtn->setFixedWidth(72);
            clearBtn->setStyleSheet("QPushButton { background: #FFFFFF; color: #62748E; border: 1px solid #E2E8F0;"
                                    "border-radius: 6px; font-size: 12px; } QPushButton:hover { background: #F1F5F9; }");
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

            updateBlockReasonCard = [blockReasonCard, blockScoreValue, blockThresholdValue, blockReasonList] {
                if (!blockReasonCard || !blockScoreValue || !blockThresholdValue || !blockReasonList)
                    return;

                const LiveAnalysisSnapshot snapshot = buildLiveAnalysisSnapshot();
                constexpr double blockThreshold = 8.0;
                const bool shouldShow = snapshot.hasInput
                    && snapshot.verdict.final_score >= blockThreshold;

                blockReasonCard->setVisible(shouldShow);
                if (!shouldShow)
                    return;

                blockScoreValue->setText(QString("%1 / 10.0")
                    .arg(snapshot.verdict.final_score, 0, 'f', 1));
                blockThresholdValue->setText(QString("%1").arg(blockThreshold, 0, 'f', 1));
                blockReasonList->setText(blockReasonHtml(blockReasonLines(snapshot)));
            };
            updateBlockReasonCard();

            auto* bufferTimer = new QTimer(dlg);
            bufferTimer->setInterval(250);
            QObject::connect(bufferTimer, &QTimer::timeout, dlg, [updateBufferText, updateBlockReasonCard] {
                updateBufferText();
                if (updateBlockReasonCard)
                    updateBlockReasonCard();
            });
            bufferTimer->start();

            QObject::connect(clearBtn, &QPushButton::clicked, dlg, [updateBufferText, normalizedMode] {
                *normalizedMode = false;
                KeyboardHook::ClearBuffer();
                updateBufferText();
            });
            cl->addWidget(bufText, 1);

            bodyL->addWidget(card, 1);
        }

        root->addWidget(body, 1);

        // ── Footer ───────────────────────────────────────────────────────────
        auto* footer = new QWidget;
        footer->setFixedHeight(68);
        footer->setStyleSheet("QWidget { background: #FFFFFF; border-top: 1px solid #E2E8F0; }");
        auto* fl = new QHBoxLayout(footer);
        fl->setContentsMargins(24, 0, 24, 0);
        fl->setSpacing(8);

        auto mkFooterBtn = [](const QString& text) {
            auto* btn = new QPushButton(text);
            btn->setFont(pretendardFont(FontRole::Medium));
            btn->setFixedHeight(36);
            btn->setStyleSheet("QPushButton { background: #FFFFFF; color: #314158; border: 1px solid #E2E8F0;"
                               "border-radius: 8px; padding: 0 14px; font-size: 13px; }"
                               "QPushButton:hover { background: #F8FAFC; }");
            return btn;
        };

        auto* copyBtn = mkFooterBtn("복사");
        auto* normalizeBtn = mkFooterBtn("정규화");
        auto* analyticsBtn = mkFooterBtn("행위 분석 보기");
        fl->addWidget(copyBtn);
        fl->addWidget(normalizeBtn);
        fl->addWidget(analyticsBtn);
        fl->addStretch();

        QObject::connect(copyBtn, &QPushButton::clicked, dlg, [bufText] {
            if (bufText)
                QApplication::clipboard()->setText(bufText->toPlainText());
        });

        QObject::connect(normalizeBtn, &QPushButton::clicked, dlg,
            [normalizedMode, normalizeBtn, updateBufferText] {
                *normalizedMode = !*normalizedMode;
                normalizeBtn->setText(*normalizedMode ? "원본 보기" : "정규화");
                updateBufferText();
            });

        QObject::connect(analyticsBtn, &QPushButton::clicked, dlg, [dlg, onOpenAnalytics] {
            if (onOpenAnalytics)
                onOpenAnalytics();
            dlg->showMinimized();
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

        auto* allowBtn = mkActionBtn("허용", "이 장치 신뢰 등록", "#2563C7", "#1E4F9E", 140);
        auto* denyBtn  = mkActionBtn("차단", "즉시 연결 종료",    UiTheme::DangerAction, UiTheme::DangerHover, 120);

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
    setStyleSheet(UiTheme::appStyle());

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

    sidebarLayout->addWidget(addNavButton("대시보드", 0, ":/icons/nav-dashboard.svg"));
    sidebarLayout->addWidget(addNavButton("USB 제어", 1, ":/icons/nav-usb.svg"));
    sidebarLayout->addWidget(addNavButton("로그", 2, ":/icons/nav-log.svg"));
    sidebarLayout->addWidget(addNavButton("행위 분석", 3, ":/icons/nav-analytics.svg"));
    sidebarLayout->addWidget(addNavButton("설정", 4, ":/icons/nav-settings.svg"));
    sidebarLayout->addStretch();
    sidebarLayout->addWidget(addNavButton("도움말", 5, ":/icons/nav-help.svg"));
    auto* collapseButton = new QPushButton("접기");
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
    topbarLayout->addWidget(addTopNavButton("대시보드", 0));
    topbarLayout->addWidget(addTopNavButton("USB 목록", 1));
    topbarLayout->addWidget(addTopNavButton("로그", 2));
    topbarLayout->addWidget(addTopNavButton("행위 분석", 3));
    topbarLayout->addWidget(addTopNavButton("설정", 4));
    topbarLayout->addStretch();

    // Bell notification icon
    auto* bellContainer = new QWidget;
    bellContainer->setFixedSize(38, 38);
    bellContainer->setStyleSheet("background: transparent;");
    auto* bellBtn = new QPushButton(bellContainer);
    bellBtn->setFlat(true);
    bellBtn->setIcon(QIcon(":/icons/ui-alert.svg"));
    bellBtn->setIconSize(QSize(16, 16));
    bellBtn->setGeometry(2, 2, 34, 34);
    bellBtn->setStyleSheet(
        "QPushButton {"
        "  border-radius: 17px;"
        "  border: 1px solid #E2E8F0;"
        "  background: #F8FAFC;"
        "}"
        "QPushButton:hover { background: #F1F5F9; }"
        "QPushButton:pressed { background: #E2E8F0; }"
    );
    auto* bellBadge = new QLabel("2", bellContainer);
    bellBadge->setObjectName("notificationBadge");
    bellBadge->setFixedSize(16, 16);
    bellBadge->setAlignment(Qt::AlignCenter);
    bellBadge->move(22, 0);
    topbarLayout->addWidget(bellContainer);

    auto* userLayout = new QHBoxLayout;
    userLayout->setContentsMargins(10, 0, 14, 0);
    userLayout->setSpacing(8);
    userLayout->addWidget(iconLabel(":/icons/ui-user.svg", 16));
    userLayout->addWidget(label("관리자"));
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

    // Load persisted unknownDeviceAction
    {
        QSettings s("Abang-All", "Abang-All");
        unknownDeviceAction_ = s.value("policy/unknownDeviceAction", 0).toInt();
    }

#ifdef Q_OS_WIN
    Logger::SetNotifyWindow(reinterpret_cast<HWND>(winId()), WM_APP_NEWLOG);
#endif

    setupTray();
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
    trayIcon_->setToolTip("Abang-All 보안 위젯");

    // Context menu
    auto* menu = new QMenu(this);
    menu->addAction("대시보드 열기", this, [this] {
        trayWidget_->hide();
        showNormal();
        raise();
        activateWindow();
    });
    menu->addAction("위젯 보기", this, [this] {
        trayWidget_->positionNearTray();
        trayWidget_->show();
        trayWidget_->raise();
    });
    menu->addSeparator();
    menu->addAction("종료", qApp, &QApplication::quit);
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

        if (Policy::IsBlacklisted(item.fingerprint))      ++blacklistConnected;
        else if (!Policy::IsWhitelisted(item.fingerprint)) ++pendingCount;
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
        trayWidget_->setRecentEvent(nowTime(), "이벤트 없음");
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // X 버튼으로 닫아도 HWND를 살려야 WM_DEVICECHANGE + 키보드 훅이 유지됨.
    // 트레이로 숨기고 이벤트를 무시해 실제 파괴를 막는다.
    event->ignore();
    hide();
    if (trayIcon_)
        trayIcon_->showMessage("Abang-All", "백그라운드에서 계속 모니터링 중입니다.",
                               QSystemTrayIcon::Information, 2000);
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
        titleBox->addWidget(label("대시보드", "pageTitle"));
        titleBox->addWidget(label("시스템 보안 상태 및 주요 이벤트를 모니터링합니다.", "pageSubtitle"));
        header->addLayout(titleBox);
        header->addStretch();
        header->addWidget(label("마지막 업데이트: " + nowTimestamp(), "mutedText"));
        header->addSpacing(16);
        auto* bufferButton = new QPushButton("버퍼 모니터 팝업 열기");
        bufferButton->setObjectName("bufferButton");
        bufferButton->setFont(pretendardFont(FontRole::SemiBold));
        bufferButton->setIcon(QIcon(":/icons/ui-lock.svg"));
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

            const bool wl = Policy::IsWhitelisted(item.fingerprint);
            const bool bl = Policy::IsBlacklisted(item.fingerprint);
            if (!wl && !bl) ++pendingCount;
            const QString pol = wl ? "허용" : bl ? "차단" : "승인 대기";
            const QString mfr = item.vid == 0 ? "-" : "HID Device";
            dashDevRows.push_back({nowTimestamp().mid(11, 5), "입력 장치", mfr, displayName, pol});
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
        stats->addWidget(createStatCard("허용 장치",   QString::number(allowedCount), "현재 허용된 장치",      ":/icons/status-allowed.svg"));
        stats->addWidget(createStatCard("차단 장치",   QString::number(blockedCount), "현재 차단된 장치",      ":/icons/status-blocked.svg"));
        stats->addWidget(createStatCard("승인 대기",   QString::number(pendingCount), "승인 대기 중인 장치",   ":/icons/status-clock.svg"));
        stats->addWidget(createStatCard("보안 이벤트", QString::number(eventCount),   "로그 기록 기준",        ":/icons/status-warning.svg"));
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
            auto* spTitle = label("시스템 보호 정상", "cardTitle");
            spTitle->setStyleSheet("font-size: 16px; color: #45556C;");
            spText->addWidget(spTitle);
            spText->addWidget(label("모든 보안 모듈이 정상적으로 동작 중입니다.", "pageSubtitle"));
            spL->addLayout(spText);
            spL->addStretch();
            bl->addWidget(statusPanel);
            bl->addSpacing(12);

            const QStringList protRows = {"실시간 USB 모니터링", "장치 제어 정책", "비정상 입력 탐지", "행위 분석", "감사 로그"};

            auto* protList = new QWidget;
            protList->setObjectName("protectionList");
            protList->setStyleSheet("QWidget#protectionList { background: #FFFFFF; }");
            auto* protLayout = new QVBoxLayout(protList);
            protLayout->setContentsMargins(0, 0, 0, 0);
            protLayout->setSpacing(4);
            for (int ri = 0; ri < protRows.size(); ++ri) {
                auto* lineW = new QWidget;
                lineW->setObjectName("protectionRow");
                lineW->setFixedHeight(46);
                lineW->setStyleSheet(ri < protRows.size() - 1
                    ? "QWidget#protectionRow { background: #FFFFFF; border-bottom: 1px solid #F1F5F9; }"
                    : "QWidget#protectionRow { background: #FFFFFF; border: none; }");
                auto* lineL = new QHBoxLayout(lineW);
                lineL->setContentsMargins(0, 3, 10, 3);
                lineL->setSpacing(10);
                lineL->addWidget(iconLabel(":/icons/status-shield-blue.svg", 16));
                lineL->addWidget(label(protRows[ri]));
                lineL->addStretch();
                auto* onBadge = label("ON", "protectionOnBadge");
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
            scrollArea->setStyleSheet(
                "QScrollArea#protectionScrollArea { background: #FFFFFF; border: none; }"
                "QScrollArea#protectionScrollArea > QWidget > QWidget { background: #FFFFFF; }"
                "QScrollBar:vertical { background: #FFFFFF; width: 6px; margin: 0; }"
                "QScrollBar::handle:vertical { background: #B7D4FF; border-radius: 3px; min-height: 24px; }"
                "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
                "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: #FFFFFF; }"
            );
            bl->addWidget(scrollArea, 0);
            bodyRow->addWidget(createCard("보호 상태 개요", body), 1);
        }

        // Recent events card
        {
            auto* body = new QWidget;
            auto* bl = new QVBoxLayout(body);
            bl->setContentsMargins(16, 14, 16, 16);
            bl->setSpacing(18);

            auto* viewAllBtn = new QPushButton("전체 보기 >");
            viewAllBtn->setCursor(Qt::PointingHandCursor);
            viewAllBtn->setStyleSheet("color: #90A1B9; background: transparent; border: none; font-size: 12px;");

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
                events.append({"--:--", "기록된 이벤트 없음", "", "mutedAccent"});

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
            bodyRow->addWidget(createCard("최근 이벤트 로그", body, viewAllBtn), 1);
        }

        layout->addLayout(bodyRow, 1);
    }

    // Bottom row: device list + policy summary
    {
        auto* bottomRow = new QHBoxLayout;
        bottomRow->setSpacing(16);
        auto* viewAllBtn = new QPushButton("전체 보기 >");
        viewAllBtn->setCursor(Qt::PointingHandCursor);
        viewAllBtn->setStyleSheet("color: #90A1B9; background: transparent; border: none; font-size: 12px;");
        // Build mini device table from the scan already done above (no second scan)
        auto* devPage = new QWidget;
        {
            auto* devL = new QVBoxLayout(devPage);
            devL->setContentsMargins(16, 8, 16, 16);
            devL->setSpacing(12);
            const int displayCount = qMin(dashDevRows.size(), 4);
            auto* table = new QTableWidget(displayCount == 0 ? 1 : displayCount, 5);
            table->setHorizontalHeaderLabels({"시간", "유형", "제조사", "장치 이름", "상태"});
            configureTable(table);
            table->verticalHeader()->setDefaultSectionSize(40);
            table->setSelectionMode(QAbstractItemView::NoSelection);
            table->setFocusPolicy(Qt::NoFocus);
            if (dashDevRows.isEmpty()) {
                table->setItem(0, 3, new QTableWidgetItem("연결된 HID 장치 없음"));
            } else {
                for (int r = 0; r < displayCount; ++r) {
                    const auto& row = dashDevRows[r];
                    table->setItem(r, 0, new QTableWidgetItem(row.time));
                    table->setItem(r, 1, new QTableWidgetItem(row.type));
                    table->setItem(r, 2, new QTableWidgetItem(row.mfr));
                    table->setItem(r, 3, new QTableWidgetItem(row.name));
                    const char* badge = row.policy == "허용" ? "successBadge"
                                      : row.policy == "차단" ? "dangerBadge"
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
                    ? "연결된 장치 없음"
                    : QString("총 %1개 중 %2개 표시").arg(dashDevRows.size()).arg(displayCount),
                "mutedText"));
            fl->addStretch();
            devL->addLayout(fl);
        }
        bottomRow->addWidget(createCard("현재 연결된 장치", devPage, viewAllBtn), 1);
        bottomRow->addWidget(createCard("운영 정책 요약", createPolicySummaryPage()), 1);
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
        auto* titleLbl = new QLabel("USB 목록");
        titleLbl->setFont(pretendardFont(FontRole::Bold));
        titleLbl->setObjectName("pageTitle");
        headerRow->addWidget(titleLbl);
        headerRow->addStretch();
        headerRow->addWidget(makeSearchBox("장치명 또는 VID/PID 검색", 256));
        headerRow->addSpacing(8);

        auto* filterBtn = new QPushButton("  필터");
        filterBtn->setObjectName("secondaryButton");
        filterBtn->setFont(pretendardFont(FontRole::Medium));
        filterBtn->setIcon(QIcon(":/icons/action-filter.svg"));
        filterBtn->setIconSize(QSize(16, 16));
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
    usbTable_->setHorizontalHeaderLabels({"장치명", "유형", "VID/PID", "상태", "정책", "마지막 확인"});
    configureTable(usbTable_);
    usbTable_->setSelectionMode(QAbstractItemView::NoSelection);
    usbTable_->setFocusPolicy(Qt::NoFocus);
    usbTable_->setColumnWidth(0, 220);
    usbTable_->setColumnWidth(1, 90);
    usbTable_->setColumnWidth(2, 110);
    usbTable_->setColumnWidth(3, 100);
    usbTable_->setColumnWidth(4, 90);
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
    dcHL->addWidget(label("장치 상세 정보", "cardTitle"));
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
        iconBox->setFixedSize(38, 38);
        iconBox->setStyleSheet("QFrame { border: 1px solid #E2E8F0; border-radius: 6px; background: white; }");
        auto* iconBoxL = new QHBoxLayout(iconBox);
        iconBoxL->setContentsMargins(0, 0, 0, 0);
        auto* devIconLbl = new QLabel;
        devIconLbl->setFixedSize(16, 16);
        devIconLbl->setPixmap(QIcon(":/icons/device-keyboard.svg").pixmap(16, 16));
        iconBoxL->addWidget(devIconLbl, 0, Qt::AlignCenter);

        auto* nameBox = new QVBoxLayout;
        nameBox->setSpacing(2);
        usbDetName_ = new QLabel("—");
        usbDetName_->setFont(pretendardFont(FontRole::Bold));
        usbDetName_->setStyleSheet("QLabel { font-size: 16px; color: #1D293D; }");
        usbDetMfr_ = new QLabel("—");
        usbDetMfr_->setFont(pretendardFont(FontRole::Regular));
        usbDetMfr_->setStyleSheet("QLabel { font-size: 12px; color: #62748E; }");
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
        row->setStyleSheet("QWidget { border-bottom: 1px solid #F1F5F9; background: transparent; }");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(0, 10, 0, 10);
        rl->setSpacing(0);
        auto* lbl = new QLabel(labelStr);
        lbl->setFont(pretendardFont(FontRole::Medium));
        lbl->setStyleSheet("QLabel { color: #62748E; font-size: 13px; border: none; }");
        lbl->setFixedWidth(100);
        rl->addWidget(lbl);
        rl->addWidget(valueW);
        rl->addStretch();
        dcBL->addWidget(row);
    };

    // 상태
    {
        auto* sw = new QWidget;
        sw->setStyleSheet("QWidget { background: transparent; }");
        auto* sl = new QHBoxLayout(sw);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->setSpacing(6);
        usbDetStatusDot_ = new QFrame;
        usbDetStatusDot_->setFixedSize(8, 8);
        usbDetStatusDot_->setStyleSheet("QFrame { background: #CAD5E2; border-radius: 4px; border: none; }");
        usbDetStatusLbl_ = new QLabel("—");
        usbDetStatusLbl_->setFont(pretendardFont(FontRole::Regular));
        usbDetStatusLbl_->setStyleSheet("QLabel { color: #45556C; font-size: 14px; border: none; background: transparent; }");
        sl->addWidget(usbDetStatusDot_);
        sl->addWidget(usbDetStatusLbl_);
        addInfoRow("상태", sw);
    }

    // 적용 정책
    {
        usbDetPolicyLbl_ = new QLabel("승인 대기");
        usbDetPolicyLbl_->setObjectName("warningBadge");
        usbDetPolicyLbl_->setFont(pretendardFont(FontRole::SemiBold));
        addInfoRow("적용 정책", usbDetPolicyLbl_);
    }

    // 유형
    {
        usbDetTypeV_ = new QLabel("—");
        usbDetTypeV_->setFont(pretendardFont(FontRole::Regular));
        usbDetTypeV_->setStyleSheet("QLabel { color: #1D293D; font-size: 13px; border: none; background: transparent; }");
        addInfoRow("유형", usbDetTypeV_);
    }

    // VID / PID
    {
        usbDetVidV_ = new QLabel("—");
        usbDetVidV_->setFont(QFont("Consolas", 12));
        usbDetVidV_->setStyleSheet("QLabel { background: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 6px; color: #314158; padding: 2px 6px; }");
        addInfoRow("VID / PID", usbDetVidV_);
    }

    // Serial No.
    {
        auto* v = new QLabel("N/A");
        v->setFont(pretendardFont(FontRole::Regular));
        v->setStyleSheet("QLabel { color: #1D293D; font-size: 13px; border: none; background: transparent; }");
        addInfoRow("Serial No.", v);
    }

    // 마지막 연결
    {
        usbDetLastV_ = new QLabel("—");
        usbDetLastV_->setFont(pretendardFont(FontRole::Regular));
        usbDetLastV_->setStyleSheet("QLabel { color: #1D293D; font-size: 13px; border: none; background: transparent; }");
        addInfoRow("마지막 연결", usbDetLastV_);
    }

    dcBL->addStretch();
    dcBL->addWidget(makeDivider());
    dcBL->addSpacing(16);

    auto makeActionBtn = [](const QString& text, const QString& bg, const QString& hoverBg,
                            const QString& fg, const QString& border) {
        auto* btn = new QPushButton(text);
        btn->setFont(pretendardFont(FontRole::Medium));
        btn->setFixedHeight(36);
        btn->setStyleSheet(QString(
            "QPushButton { background: %1; color: %2; border-radius: 8px; border: %3; font-size: 14px; }"
            "QPushButton:hover { background: %4; }"
            "QPushButton:disabled { background: #F1F5F9; color: #90A1B9; border: 1px solid #E2E8F0; }"
        ).arg(bg, fg, border, hoverBg));
        return btn;
    };
    usbAllowBtn_      = makeActionBtn("이 장치 허용",   "#2563C7", "#1E4F9E", "#FFFFFF", "none");
    usbBlockBtn_      = makeActionBtn("이 장치 차단",   "#D21F4A", "#AD183B", "#FFFFFF", "none");
    usbRemoveRuleBtn_ = makeActionBtn("예외 규칙 삭제", "#FFFFFF", "#F8FAFC", "#62748E", "1px solid #E2E8F0");
    dcBL->addWidget(usbAllowBtn_);
    dcBL->addSpacing(8);
    dcBL->addWidget(usbBlockBtn_);
    dcBL->addSpacing(8);
    dcBL->addWidget(usbRemoveRuleBtn_);
    dcBL->addSpacing(10);
    usbActionFeedback_ = new QLabel;
    usbActionFeedback_->setFont(pretendardFont(FontRole::Medium));
    usbActionFeedback_->setWordWrap(true);
    usbActionFeedback_->setStyleSheet("QLabel { color: #62748E; font-size: 12px; background: transparent; border: none; }");
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
        item.allow       = true;
        if (!confirmPolicyChange(this, r.name, true)) return;
        Policy::AddWhitelist(item);
        const QString feedback = reenableAllowedDevice(item, L"Policy allow");
        enforceBlockedDeviceHook();
        Logger::Add(L"[Qt] Policy allow: " + toWString(r.name));
        r.policy = "허용";
        usbTable_->setCellWidget(usbSelectedRow_, 4, makeBadgeCell("허용", "successBadge"));
        applyUsbDetailPanel(usbSelectedRow_);
        showUsbActionFeedback(feedback, UiTheme::PrimaryText);
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
        item.allow       = false;
        if (!confirmPolicyChange(this, r.name, false)) return;
        Policy::AddBlacklist(item);
        enforceBlockedDeviceHook();
        Logger::Add(L"[Qt] Policy block: " + toWString(r.name));
        r.policy = "차단";
        usbTable_->setCellWidget(usbSelectedRow_, 4, makeBadgeCell("차단", "dangerBadge"));
        applyUsbDetailPanel(usbSelectedRow_);
        showUsbActionFeedback("차단 목록에 저장하고 입력 차단 상태를 갱신했습니다.", "#C10007");
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
        usbTable_->setCellWidget(usbSelectedRow_, 4, makeBadgeCell("승인 대기", "warningBadge"));
        applyUsbDetailPanel(usbSelectedRow_);
        showUsbActionFeedback("예외 규칙을 삭제했습니다. 연결 중인 장치는 다시 검토합니다.", UiTheme::PrimaryText);
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

        const QString itemPolicy = policyTextForFingerprint(item.fingerprint);
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
        setCellWidgetBackground(nameW, Qt::white);
        auto* nl = new QHBoxLayout(nameW);
        nl->setContentsMargins(10, 0, 10, 0);
        nl->setSpacing(8);
        nl->addWidget(iconLabel(row.iconRes, 16));
        auto* nameLbl = new QLabel(row.name);
        nameLbl->setAttribute(Qt::WA_TransparentForMouseEvents);
        nameLbl->setFont(pretendardFont(FontRole::Medium));
        nameLbl->setStyleSheet("QLabel { color: #1D293D; font-size: 14px; border: none; background: transparent; }");
        nl->addWidget(nameLbl);
        nl->addStretch();
        usbTable_->setCellWidget(r, 0, nameW);

        usbTable_->setCellWidget(r, 1, makeUsbTextCell(row.type,    "#45556C", pretendardFont(FontRole::Regular), 10));
        usbTable_->setCellWidget(r, 2, makeUsbTextCell(row.vidpid,  "#62748E", QFont("Consolas", 12), 10));
        usbTable_->setCellWidget(r, 3, makeUsbStatusCell(row.status));
        usbTable_->setCellWidget(r, 4, makeBadgeCell(row.policy, policyBadge(row.policy)));
        usbTable_->setCellWidget(r, 5, makeUsbTextCell(row.lastSeen, "#62748E", pretendardFont(FontRole::Regular), 10));
    }

    if (usbSelectedRow_ >= usbRows_.size()) usbSelectedRow_ = 0;
    applyUsbDetailPanel(usbSelectedRow_);
    usbListStale_ = false;
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
    usbDetStatusDot_->setStyleSheet(QString(
        "QFrame { background: %1; border-radius: 4px; border: none; }")
        .arg(connected ? "#00C950" : "#CAD5E2"));
    usbDetStatusLbl_->setText(r.status);

    const char* badgeName = (r.policy == "허용") ? "successBadge"
                          : (r.policy == "차단") ? "dangerBadge"
                          : "warningBadge";
    usbDetPolicyLbl_->setText(r.policy);
    usbDetPolicyLbl_->setObjectName(badgeName);
    usbDetPolicyLbl_->style()->unpolish(usbDetPolicyLbl_);
    usbDetPolicyLbl_->style()->polish(usbDetPolicyLbl_);

    usbDetTypeV_->setText(r.type);
    usbDetVidV_->setText(r.vidpid.left(4) + " : " + r.vidpid.mid(5));
    usbDetLastV_->setText(r.lastSeen);

    const bool hasDevice = r.fingerprint != 0;
    if (usbAllowBtn_) {
        usbAllowBtn_->setEnabled(hasDevice && r.policy != "허용");
        usbAllowBtn_->setToolTip(r.policy == "허용" ? "이미 허용된 장치입니다." : "이 장치를 허용 목록에 등록합니다.");
    }
    if (usbBlockBtn_) {
        usbBlockBtn_->setEnabled(hasDevice && r.policy != "차단");
        usbBlockBtn_->setToolTip(r.policy == "차단" ? "이미 차단된 장치입니다." : "이 장치를 차단 목록에 등록합니다.");
    }
    if (usbRemoveRuleBtn_) {
        usbRemoveRuleBtn_->setEnabled(hasDevice && r.policy != "승인 대기");
        usbRemoveRuleBtn_->setToolTip(r.policy == "승인 대기" ? "삭제할 허용/차단 규칙이 없습니다." : "이 장치의 허용/차단 규칙을 삭제합니다.");
    }

    // Row highlight
    const QColor hi(255, 247, 237);
    for (int rr = 0; rr < usbRows_.size(); ++rr) {
        const QColor bg = (rr == row) ? hi : Qt::white;
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
    usbActionFeedback_->setStyleSheet(QString(
        "QLabel { color: %1; font-size: 12px; background: transparent; border: none; }"
    ).arg(color));

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
            if (Policy::IsWhitelisted(item.fingerprint))      policy = "허용";
            else if (Policy::IsBlacklisted(item.fingerprint)) policy = "차단";

            const QString mfr = item.vid == 0 ? "-" : "HID Device";
            const QString type = "입력 장치";

            rows.push_back({nowTimestamp().mid(11, 5), type, mfr, displayName, policy});
        }
    }

    const int displayCount = qMin(rows.size(), 4);
    auto* table = new QTableWidget(displayCount == 0 ? 1 : displayCount, 5);
    table->setHorizontalHeaderLabels({"시간", "유형", "제조사", "장치 이름", "상태"});
    configureTable(table);
    table->verticalHeader()->setDefaultSectionSize(40);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setFocusPolicy(Qt::NoFocus);

    if (rows.isEmpty()) {
        table->setItem(0, 0, new QTableWidgetItem(""));
        table->setItem(0, 1, new QTableWidgetItem(""));
        table->setItem(0, 2, new QTableWidgetItem(""));
        table->setItem(0, 3, new QTableWidgetItem("연결된 HID 장치 없음"));
        table->setItem(0, 4, new QTableWidgetItem(""));
    } else {
        for (int r = 0; r < displayCount; ++r) {
            const auto& row = rows[r];
            table->setItem(r, 0, new QTableWidgetItem(row.time));
            table->setItem(r, 1, new QTableWidgetItem(row.type));
            table->setItem(r, 2, new QTableWidgetItem(row.mfr));
            table->setItem(r, 3, new QTableWidgetItem(row.name));
            const char* badge = row.policy == "허용" ? "successBadge"
                              : row.policy == "차단" ? "dangerBadge"
                              : "warningBadge";
            table->setCellWidget(r, 4, makeBadgeCell(row.policy, badge));
        }
    }
    table->setMinimumHeight(170);
    table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);

    layout->addWidget(table, 1);
    auto* fl = new QHBoxLayout;
    fl->addWidget(label(
        rows.isEmpty()
            ? "연결된 장치 없음"
            : QString("총 %1개 중 %2개 표시").arg(rows.size()).arg(displayCount),
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
        headerRow->addWidget(label("로그", "pageTitle"));
        headerRow->addStretch();
        headerRow->addWidget(makeSearchBox("이벤트, 장치명 등 검색", 256));
        headerRow->addSpacing(8);

        auto mkBtn = [&](const QString& text, const QString& icon) {
            auto* btn = new QPushButton("  " + text);
            btn->setObjectName("secondaryButton");
            btn->setFont(pretendardFont(FontRole::Medium));
            btn->setIcon(QIcon(icon));
            btn->setIconSize(QSize(16, 16));
            return btn;
        };
        headerRow->addWidget(mkBtn("기간 선택",   ":/icons/status-clock.svg"));
        headerRow->addWidget(mkBtn("이벤트 필터", ":/icons/action-filter.svg"));

        auto* exportBtn = new QPushButton("내보내기 (CSV)");
        exportBtn->setObjectName("primaryButton");
        exportBtn->setFont(pretendardFont(FontRole::Medium));
        headerRow->addWidget(exportBtn);
        layout->addLayout(headerRow);
    }

    // ── Per-row helpers shared by initial fill and appendNewLogRows() ───────────
    // Classify a log line → (icon resource, result label, badge objectName)
    auto classifyLine = [](const QString& line)
        -> std::tuple<QString,QString,QString>
    {
        if (line.contains("block", Qt::CaseInsensitive) ||
            line.contains("blacklist", Qt::CaseInsensitive) || line.contains("차단"))
            return {":/icons/status-blocked.svg", "차단", "blockBadge"};
        if (line.contains("allow", Qt::CaseInsensitive) ||
            line.contains("whitelist", Qt::CaseInsensitive) || line.contains("허용"))
            return {":/icons/status-allowed.svg", "허용", "successBadge"};
        if (line.contains("warning", Qt::CaseInsensitive) ||
            line.contains("anomaly", Qt::CaseInsensitive) ||
            line.contains("unknown", Qt::CaseInsensitive))
            return {":/icons/status-warning.svg", "확인 필요", "warningBadge"};
        return {":/icons/status-info.svg", "기록", "neutralBadge"};
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
        logRows.push_back({nowTimestamp(), "아직 기록된 로그가 없습니다.",
                           ":/icons/status-info.svg", "-", "대기", "neutralBadge"});

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
        table->setHorizontalHeaderLabels({"시간", "이벤트", "장치", "처리 결과"});
        configureTable(table);
        // Same as USB table: NoSelection + WA_TransparentForMouseEvents on all
        // cell widgets → clicks pass through to the viewport → cellClicked fires.
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setFocusPolicy(Qt::NoFocus);
        table->horizontalHeader()->setStyleSheet(
            "QHeaderView::section {"
            "  background: #F8FAFC;"
            "  border: none;"
            "  border-bottom: 1px solid #1D293D;"
            "  padding-left: 20px;"
            "  font-size: 14px;"
            "  color: #45556C;"
            "}"
        );
        table->setStyleSheet(
            "QTableWidget { background: white; border: none; }"
            "QTableWidget::item { border: none; color: #45556C; padding-left: 20px; }"
        );

        auto mkTextCell = [&](const QString& text, const QString& color, bool hi) -> QWidget* {
            auto* w = new QWidget;
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
            setCellWidgetBackground(w, hi ? QColor(255, 247, 237) : Qt::white);
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            l->setSpacing(0);
            auto* txt = new QLabel(text);
            txt->setAttribute(Qt::WA_TransparentForMouseEvents);
            txt->setFont(pretendardFont(FontRole::Regular));
            txt->setStyleSheet(QString("font-size: 14px; color: %1; background: transparent; border: none;").arg(color));
            l->addWidget(txt);
            l->addStretch();
            return w;
        };

        // Event cell: icon (16×16) + medium text
        auto mkEventCell = [&](const QString& iconSvg, const QString& text, bool hi) -> QWidget* {
            auto* w = new QWidget;
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
            setCellWidgetBackground(w, hi ? QColor(255, 247, 237) : Qt::white);
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            l->setSpacing(8);
            auto* ico = new QLabel;
            ico->setAttribute(Qt::WA_TransparentForMouseEvents);
            ico->setFixedSize(16, 16);
            ico->setPixmap(QIcon(iconSvg).pixmap(16, 16));
            ico->setStyleSheet("background: transparent;");
            l->addWidget(ico);
            auto* txt = new QLabel(text);
            txt->setAttribute(Qt::WA_TransparentForMouseEvents);
            txt->setFont(pretendardFont(FontRole::Medium));
            txt->setStyleSheet("font-size: 14px; color: #1D293D; background: transparent;");
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
            auto* timeW = mkTextCell(row.time, "#62748E", hi);
            timeW->setProperty("log_icon",   row.eventIcon);
            timeW->setProperty("log_event",  row.event);
            timeW->setProperty("log_time",   row.time);
            timeW->setProperty("log_device", row.device);
            timeW->setProperty("log_result", row.result);
            timeW->setProperty("log_badge",  row.badge);
            table->setCellWidget(r, 0, timeW);
            table->setCellWidget(r, 1, mkEventCell(row.eventIcon, row.event, hi));
            table->setCellWidget(r, 2, mkTextCell(row.device, "#45556C", hi));
            auto* badgeW = makeBadgeCell(row.result, row.badge.toUtf8().constData());
            if (hi) setCellWidgetBackground(badgeW, QColor(255, 247, 237));
            table->setCellWidget(r, 3, badgeW);
        }

        table->setColumnWidth(0, 190);
        table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        table->setColumnWidth(2, 230);
        table->setColumnWidth(3, 100);
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
        panelHdr->setFixedHeight(45);
        panelHdr->setStyleSheet(
            "QWidget { background: rgba(248,250,252,128);"
            "          border-bottom: 1px solid #F1F5F9; }"
        );
        auto* phl = new QHBoxLayout(panelHdr);
        phl->setContentsMargins(16, 0, 16, 0);
        phl->addWidget(label("이벤트 상세 정보", "cardTitle"));
        phl->addStretch();
        dcl->addWidget(panelHdr);

        auto* bodyW = new QWidget;
        auto* bodyL = new QVBoxLayout(bodyW);
        bodyL->setContentsMargins(16, 16, 16, 0);
        bodyL->setSpacing(0);

        // Mutable widgets for dynamic update
        auto borderForLogBadge = [](const QString& badge) {
            if (badge == "blockBadge")   return QStringLiteral("border: 1px solid #FFCCD3;");
            if (badge == "warningBadge") return QStringLiteral("border: 1px solid #FDE68A;");
            if (badge == "successBadge") return QStringLiteral("border: 1px solid #ABEFC6;");
            return QString("border: 1px solid %1;").arg(UiTheme::ChartNormal);
        };

        auto* detIconLabel = new QLabel;
        detIconLabel->setFixedSize(16, 16);
        detIconLabel->setPixmap(QIcon(logRows[0].eventIcon).pixmap(16, 16));

        auto* detIconBox = new QFrame;
        detIconBox->setFixedSize(34, 34);
        detIconBox->setStyleSheet(QString(
            "QFrame { background: white; %1 border-radius: 4px; }").arg(borderForLogBadge(logRows[0].badge)));
        {
            auto* ibl = new QHBoxLayout(detIconBox);
            ibl->setContentsMargins(8, 8, 8, 8);
            ibl->addWidget(detIconLabel, 0, Qt::AlignCenter);
        }

        auto* detEvTitle = new QLabel(logRows[0].event);
        detEvTitle->setFont(pretendardFont(FontRole::Bold));
        detEvTitle->setStyleSheet("font-size: 18px; color: #1D293D;");
        detEvTitle->setWordWrap(true);

        auto* detEvTime = new QLabel(logRows[0].time);
        detEvTime->setFont(pretendardFont(FontRole::Regular));
        detEvTime->setStyleSheet("font-size: 12px; color: #62748E;");

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
            lbl->setFont(pretendardFont(FontRole::Medium));
            lbl->setStyleSheet("font-size: 12px; color: #62748E;");
            bodyL->addWidget(lbl);
            bodyL->addSpacing(6);
            bodyL->addWidget(valueW);
            bodyL->addSpacing(16);
            bodyL->addWidget(makeDivider());
            bodyL->addSpacing(16);
        };

        auto* detDevBadge = new QLabel(logRows[0].device);
        detDevBadge->setFont(pretendardFont(FontRole::Medium));
        detDevBadge->setStyleSheet(
            "background: #F8FAFC; border: 1px solid #E2E8F0;"
            "border-radius: 6px; padding: 5px 8px; font-size: 13px; color: #314158;"
        );
        detDevBadge->setWordWrap(true);
        addDetailSection("장치 정보", detDevBadge);

        auto* detDesc = new QLabel("등록되지 않은 저장 장치가 연결되었습니다.");
        detDesc->setFont(pretendardFont(FontRole::Regular));
        detDesc->setStyleSheet("font-size: 14px; color: #1D293D;");
        detDesc->setWordWrap(true);
        addDetailSection("상세 설명", detDesc);

        auto* detResultBadge = new QLabel(logRows[0].result);
        detResultBadge->setObjectName(logRows[0].badge.toUtf8().constData());
        detResultBadge->setFont(pretendardFont(FontRole::SemiBold));
        detResultBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        {
            auto* row2 = new QHBoxLayout;
            row2->setSpacing(0);
            auto* resultCol = new QVBoxLayout;
            resultCol->setSpacing(6);
            auto* rLbl = new QLabel("처리 결과");
            rLbl->setFont(pretendardFont(FontRole::Medium));
            rLbl->setStyleSheet("font-size: 12px; color: #62748E;");
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
            auto* uLbl = new QLabel("관련 사용자");
            uLbl->setFont(pretendardFont(FontRole::Medium));
            uLbl->setStyleSheet("font-size: 12px; color: #62748E;");
            userCol->addWidget(uLbl);
            auto* uVal = new QLabel("SYSTEM");
            uVal->setFont(pretendardFont(FontRole::Medium));
            uVal->setStyleSheet("font-size: 14px; color: #1D293D;");
            userCol->addWidget(uVal);
            row2->addLayout(userCol);
            bodyL->addLayout(row2);
            bodyL->addSpacing(16);
            bodyL->addWidget(makeDivider());
            bodyL->addSpacing(16);
        }

        auto* actionCard = new QFrame;
        actionCard->setStyleSheet(
            "QFrame { background: transparent; border: none; }"
        );
        {
            auto* al = new QVBoxLayout(actionCard);
            al->setContentsMargins(0, 0, 0, 0);
            al->setSpacing(6);
            auto* aHdr = new QHBoxLayout;
            aHdr->setSpacing(6);
            aHdr->addWidget(iconLabel(":/icons/ui-alert.svg", 14));
            auto* aTitle = new QLabel("ACTION REQUIRED");
            aTitle->setFont(pretendardFont(FontRole::Bold));
            aTitle->setStyleSheet("font-size: 13px; color: #E34B3F; letter-spacing: 0.3px; background: transparent; border: none;");
            aHdr->addWidget(aTitle);
            aHdr->addStretch();
            al->addLayout(aHdr);
            auto* aDesc = new QLabel(
                "이 장치가 인가된 장치인지 확인 후 예외 처리를 진행하거나 차단 정책을 유지하세요."
            );
            aDesc->setFont(pretendardFont(FontRole::Regular));
            aDesc->setStyleSheet("font-size: 13px; color: #314158; background: transparent; border: none;");
            aDesc->setWordWrap(true);
            al->addWidget(aDesc);
        }
        bodyL->addWidget(actionCard);
        actionCard->setVisible(logRows[0].badge == "blockBadge");

        bodyL->addStretch();

        {
            auto* btnArea = new QFrame;
            btnArea->setStyleSheet("QFrame { border-top: 1px solid #E2E8F0; }");
            auto* bl = new QHBoxLayout(btnArea);
            bl->setContentsMargins(0, 16, 0, 16);
            bl->setSpacing(8);
            auto* infoBtn = new QPushButton("장치 정보 조회");
            infoBtn->setObjectName("secondaryButton");
            infoBtn->setFont(pretendardFont(FontRole::Medium));
            infoBtn->setFixedHeight(36);
            bl->addWidget(infoBtn, 1);
            auto* regBtn = new QPushButton("예외 규칙 등록");
            regBtn->setObjectName("primaryButton");
            regBtn->setFont(pretendardFont(FontRole::Medium));
            regBtn->setFixedHeight(36);
            bl->addWidget(regBtn, 1);
            bodyL->addWidget(btnArea);
        }

        dcl->addWidget(bodyW, 1);
        contentRow->addWidget(detailCard);

        // Wire up row click → update detail panel
        const QMap<QString,QString> descMap = {
            {"blockBadge",   "등록되지 않은 장치의 연결이 차단되었습니다."},
            {"warningBadge", "비정상적인 행위가 탐지되어 경고 알림이 발송되었습니다."},
            {"successBadge", "허용된 장치가 정상적으로 연결되었습니다."},
            {"neutralBadge", "시스템 이벤트가 기록되었습니다."},
        };

        // Same pattern as USB table: NoSelection + cellClicked.
        // Metadata is read from QObject properties on the column-0 cell widget.
        connect(logTable, &QTableWidget::cellClicked,
            [logTable, detIconLabel, detIconBox, detEvTitle, detEvTime,
             detDevBadge, detDesc, detResultBadge, actionCard, descMap, borderForLogBadge]
            (int row, int) {
                auto* w = logTable->cellWidget(row, 0);
                if (!w) return;

                const QString icon   = w->property("log_icon").toString();
                const QString event  = w->property("log_event").toString();
                const QString time   = w->property("log_time").toString();
                const QString device = w->property("log_device").toString();
                const QString result = w->property("log_result").toString();
                const QString badge  = w->property("log_badge").toString();

                detIconLabel->setPixmap(QIcon(icon).pixmap(16, 16));

                detIconBox->setStyleSheet(QString(
                    "QFrame { background: white; %1 border-radius: 4px; }").arg(borderForLogBadge(badge)));

                detEvTitle->setText(event);
                detEvTime->setText(time);
                detDevBadge->setText(device == "-" ? "N/A" : device);
                detDesc->setText(descMap.value(badge, "이벤트가 기록되었습니다."));

                detResultBadge->setText(result);
                detResultBadge->setObjectName(badge.toUtf8().constData());
                detResultBadge->style()->unpolish(detResultBadge);
                detResultBadge->style()->polish(detResultBadge);

                actionCard->setVisible(badge == "blockBadge");

                // Re-apply row highlight tint across ALL rows
                const QColor hiColor(255, 247, 237);
                for (int rr = 0; rr < logTable->rowCount(); ++rr) {
                    const QColor bg = (rr == row) ? hiColor : Qt::white;
                    setCellWidgetBackground(logTable->cellWidget(rr, 0), bg);
                    setCellWidgetBackground(logTable->cellWidget(rr, 1), bg);
                    setCellWidgetBackground(logTable->cellWidget(rr, 2), bg);
                    setCellWidgetBackground(logTable->cellWidget(rr, 3), bg);
                }
            }
        );
    }

    layout->addLayout(contentRow, 1);
    return page;
}

// ===========================================================================
// Analytics Page
// ===========================================================================
QWidget* MainWindow::createAnalyticsPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // Header
    {
        auto* headerRow = new QHBoxLayout;
        auto* titleBox = new QVBoxLayout;
        titleBox->setSpacing(4);
        titleBox->addWidget(label("행위 분석", "pageTitle"));
        titleBox->addWidget(label("키보드 입력 행위를 분석하여 비정상적 또는 악성 활동을 탐지합니다.", "pageSubtitle"));
        headerRow->addLayout(titleBox);
        headerRow->addStretch();
        headerRow->addWidget(label("마지막 업데이트: 2026-04-23 14:22:33", "mutedText"));
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
        lbl->setStyleSheet("font-size: 11px; color: #62748E;");
        l->addWidget(lbl);
        return w;
    };

    // Bar chart card "입력 속도 (keys/sec)"
    {
        auto* rangeLabel = new QLabel("최근 30분");
        rangeLabel->setFont(pretendardFont(FontRole::Medium));
        rangeLabel->setStyleSheet("color: #62748E; font-size: 13px;");

        auto* chartBody = new QWidget;
        auto* cbL = new QVBoxLayout(chartBody);
        cbL->setContentsMargins(16, 8, 16, 16);
        cbL->setSpacing(8);

        auto* metricLabel = new QLabel("초당 키 입력");
        metricLabel->setFont(pretendardFont(FontRole::Regular));
        metricLabel->setStyleSheet("color: #62748E; font-size: 12px;");
        auto* subHeader = new QHBoxLayout;
        subHeader->addWidget(metricLabel);
        subHeader->addStretch();
        cbL->addLayout(subHeader);

        auto* barChart = new BarChartWidget;
        inputSpeedChart = barChart;
        barChart->setMinimumHeight(200);
        cbL->addWidget(barChart, 1);

        auto* legendRow = new QHBoxLayout;
        legendRow->addStretch();
        legendRow->addWidget(mkLegendItem(UiTheme::ChartNormal, "정상 범위"));
        legendRow->addSpacing(16);
        legendRow->addWidget(mkLegendItem(UiTheme::ChartThreshold, "이상 구간"));
        cbL->addLayout(legendRow);

        leftL->addWidget(createCard("입력 속도 (keys/sec)", chartBody, rangeLabel), 1);
    }

    // Line chart card "위험 점수 추이"
    {
        auto* rangeLabel2 = new QLabel("최근 30분");
        rangeLabel2->setFont(pretendardFont(FontRole::Medium));
        rangeLabel2->setStyleSheet("color: #62748E; font-size: 13px;");

        auto* lineBody = new QWidget;
        auto* lbL = new QVBoxLayout(lineBody);
        lbL->setContentsMargins(16, 8, 16, 16);
        lbL->setSpacing(8);

        auto* cumulLabel = new QLabel("누적 위험 점수");
        cumulLabel->setFont(pretendardFont(FontRole::Regular));
        cumulLabel->setStyleSheet("color: #62748E; font-size: 12px;");
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
        legendRow2->addWidget(mkLegendItem("#2563C7", "명령어"));
        legendRow2->addSpacing(12);
        legendRow2->addWidget(mkLegendItem("#7C3AED", "URL"));
        legendRow2->addSpacing(12);
        legendRow2->addWidget(mkLegendItem("#0891B2", "입력 속도"));
        legendRow2->addSpacing(12);
        legendRow2->addWidget(mkLegendItem("#DB2777", "핫키"));
        lbL->addLayout(legendRow2);

        leftL->addWidget(createCard("위험 점수 추이", lineBody, rangeLabel2), 1);
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
        hdL->addWidget(label("현재 세션 요약", "cardTitle"));
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
            warn->setStyleSheet("QFrame { background: transparent; border: none; }");
            auto* wl = new QHBoxLayout(warn);
            wl->setContentsMargins(0, 0, 0, 0);
            wl->setSpacing(8);
            auto* wIcon = new QLabel("⚠");
            wIcon->setStyleSheet("color: #F59E0B; font-size: 14px; background: transparent; border: none;");
            auto* wText = new QVBoxLayout;
            wText->setSpacing(2);
            auto* wTitle = new QLabel("WARNING");
            warningTitle = wTitle;
            wTitle->setFont(pretendardFont(FontRole::Bold));
            wTitle->setStyleSheet("color: #B45309; font-size: 13px; background: transparent; border: none;");
            auto* wDesc = new QLabel("불필요한 명령 시퀀스가 감지되었습니다.");
            warningDesc = wDesc;
            wDesc->setFont(pretendardFont(FontRole::Regular));
            wDesc->setStyleSheet("color: #92400E; font-size: 12px; background: transparent; border: none;");
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
            lbl->setFont(pretendardFont(FontRole::SemiBold));
            lbl->setStyleSheet("QLabel { color: #1D293D; font-size: 13px; }");
            bL->addWidget(lbl);
            bL->addSpacing(6);
        };

        auto addRow = [&](const QString& key, const QString& val, bool valRed = false) -> QLabel* {
            auto* row = new QHBoxLayout;
            auto* k = new QLabel(key);
            k->setFont(pretendardFont(FontRole::Regular));
            k->setStyleSheet("QLabel { color: #62748E; font-size: 13px; }");
            auto* v = new QLabel(val);
            v->setFont(pretendardFont(FontRole::Medium));
            v->setStyleSheet(QString("QLabel { color: %1; font-size: 13px; }").arg(valRed ? UiTheme::DangerText : UiTheme::Text));
            row->addWidget(k);
            row->addStretch();
            row->addWidget(v);
            bL->addLayout(row);
            bL->addSpacing(8);
            return v;
        };

        // 세션 정보
        addSection("세션 정보");
        addRow("장치 유형",  "Keyboard (HID)");
        sessionStartValue = addRow("시작 시각", analyticsSessionStart.toString("yyyy-MM-dd HH:mm:ss"));
        durationValue = addRow("지속 시간", "00:00:00");
        bL->addWidget(makeDivider());
        bL->addSpacing(12);

        // 입력 지표
        addSection("입력 지표");
        totalInputValue = addRow("총 키 입력 수", "0");
        avgSpeedValue = addRow("평균 입력 간격", "-");
        commandCountValue = addRow("감지 명령", "0");
        hotkeyCountValue = addRow("핫키 이벤트", "0");
        bL->addWidget(makeDivider());
        bL->addSpacing(12);

        // 위험 평가
        addSection("위험 평가");
        commandScoreValue = addRow("명령어 위험도", "0.0 / 10.0");
        urlScoreValue = addRow("URL 위험도", "0.0 / 10.0");
        typingScoreValue = addRow("입력 속도 위험도", "0.0 / 10.0");
        hotkeyScoreValue = addRow("핫키 위험도", "0.0 / 10.0");
        threatScoreValue = addRow("현재 위협 점수", "0.0 / 10.0", true);
        levelValue = addRow("판정 등급",  "SAFE");
        addRow("설정된 임계값",  "8.0");

        bL->addStretch();
        scL->addWidget(body, 1);
        contentRow->addWidget(sessionCard);
    }

    auto* analyticsTimer = new QTimer(page);
    auto updateThreatTrend = [inputSpeedChart, threatTrendChart, analyticsSessionStart, durationValue,
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
                appendThreatTrendSample(lastSnapshot.verdict.final_score);
                if (threatTrendChart)
                    threatTrendChart->setData(g_threatScoreTrend);
            }
        }
        const auto& snapshot = lastSnapshot;
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
                    warningDesc->setText(QStringLiteral("고위험 명령 시퀀스가 감지되었습니다."));
                } else {
                    warningTitle->setText("WARNING");
                    warningDesc->setText(QStringLiteral("불필요한 명령 시퀀스가 감지되었습니다."));
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
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setSpacing(16);

    // Hoist settable widget pointers
    QComboBox*        comboDev    = nullptr;
    ToggleSwitchWidget* toggleSw  = nullptr;
    QSpinBox*         threshSpin  = nullptr;
    QCheckBox*        cbAutostart = nullptr;
    QCheckBox*        cbMinimize  = nullptr;
    QCheckBox*        cbNotif     = nullptr;

    QPushButton* saveBtn = nullptr;

    // Header
    {
        auto* headerRow = new QHBoxLayout;
        headerRow->addWidget(label("설정", "pageTitle"));
        headerRow->addStretch();
        saveBtn = new QPushButton(" 변경 사항 저장");
        saveBtn->setObjectName("primaryButton");
        saveBtn->setFont(pretendardFont(FontRole::Medium));
        saveBtn->setIcon(QIcon(":/icons/ui-save.svg"));
        saveBtn->setIconSize(QSize(16, 16));
        headerRow->addWidget(saveBtn);
        layout->addLayout(headerRow);
    }

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    // Left: settings card with tab bar
    {
        auto* settingsCard = cardFrame();
        auto* scL = new QVBoxLayout(settingsCard);
        scL->setContentsMargins(0, 0, 0, 0);
        scL->setSpacing(0);

        // Tab bar
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
            {"일반 환경 설정", ":/icons/action-filter.svg"},
            {"위험 탐지 정책", ":/icons/ui-lock.svg"},
            {"알림 및 리포트", ":/icons/ui-bell.svg"},
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

        // Connect tab buttons
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

        // Tab 0: General settings
        {
            auto* w = new QWidget;
            auto* wl = new QVBoxLayout(w);
            wl->setContentsMargins(24, 24, 24, 24);
            wl->setSpacing(0);

            auto addSection = [&](const QString& heading) {
                auto* lbl = label(heading, "sectionHeading");
                lbl->setFont(pretendardFont(FontRole::Regular));
                wl->addWidget(lbl);
                wl->addSpacing(16);
            };

            addSection("시작 및 트레이 동작");
            cbAutostart = new QCheckBox("Windows 시작 시 자동으로 실행");
            cbAutostart->setObjectName("settingsCheck");
            cbAutostart->setFont(pretendardFont(FontRole::Medium));
            wl->addWidget(cbAutostart);
            wl->addSpacing(16);
            cbMinimize = new QCheckBox("창을 닫을 때 시스템 트레이로 최소화");
            cbMinimize->setObjectName("settingsCheck");
            cbMinimize->setFont(pretendardFont(FontRole::Medium));
            wl->addWidget(cbMinimize);
            wl->addSpacing(16);
            wl->addStretch();
            tabStack->addWidget(w);
        }

        // Tab 1: Threat detection policy
        {
            auto* w = new QWidget;
            auto* wl = new QVBoxLayout(w);
            wl->setContentsMargins(24, 24, 24, 24);
            wl->setSpacing(0);

            // Section: 기본 제어 정책
            auto* secLabel1 = label("기본 제어 정책", "sectionHeading");
            secLabel1->setFont(pretendardFont(FontRole::Regular));
            wl->addWidget(secLabel1);
            wl->addSpacing(16);

            auto* dropDesc = new QLabel("새로운 미등록 장치 연결 시 처리 방법");
            dropDesc->setFont(pretendardFont(FontRole::Medium));
            dropDesc->setStyleSheet("color: #314158; font-size: 14px;");
            wl->addWidget(dropDesc);
            wl->addSpacing(8);

            comboDev = new QComboBox;
            comboDev->addItem("승인 대기");
            comboDev->addItem("자동 차단");
            comboDev->addItem("자동 허용");
            comboDev->setFixedWidth(320);
            comboDev->setFixedHeight(36);
            comboDev->setFont(pretendardFont(FontRole::Regular));
            wl->addWidget(comboDev);
            wl->addSpacing(32);

            wl->addWidget(makeDivider());
            wl->addSpacing(32);

            // Section: BadUSB 차단
            auto* secLabel2 = label("BadUSB 및 키로거 차단 (행위 기반)", "sectionHeading");
            secLabel2->setFont(pretendardFont(FontRole::Regular));
            wl->addWidget(secLabel2);
            wl->addSpacing(16);

            // Toggle card
            auto* toggleCard = new QFrame;
            toggleCard->setStyleSheet(
                "QFrame { background: #F8FAFC; border-radius: 8px; border: 1px solid #E2E8F0; }");
            auto* toggleCardL = new QHBoxLayout(toggleCard);
            toggleCardL->setContentsMargins(17, 17, 17, 17);
            toggleCardL->setSpacing(16);

            auto* toggleTextBox = new QVBoxLayout;
            toggleTextBox->setSpacing(4);
            auto* toggleTitle = new QLabel("행위 기반 입력 장치 모니터링 활성화");
            toggleTitle->setFont(pretendardFont(FontRole::Regular));
            toggleTitle->setStyleSheet("color: #1D293D; font-size: 14px; border: none; background: transparent;");
            auto* toggleDesc = new QLabel(
                "인간의 물리적 한계를 넘어서는 빠른 키 입력(매크로/스크립트)을 감지하고 차단합니다.");
            toggleDesc->setFont(pretendardFont(FontRole::Regular));
            toggleDesc->setStyleSheet("color: #62748E; font-size: 12px; border: none; background: transparent;");
            toggleDesc->setWordWrap(true);
            toggleTextBox->addWidget(toggleTitle);
            toggleTextBox->addWidget(toggleDesc);

            toggleSw = new ToggleSwitchWidget(true);
            toggleCardL->addLayout(toggleTextBox, 1);
            toggleCardL->addWidget(toggleSw);
            wl->addWidget(toggleCard);
            wl->addSpacing(20);

            // Threshold row
            auto* threshRow = new QHBoxLayout;
            threshRow->setContentsMargins(4, 0, 0, 0);
            threshRow->setSpacing(0);
            auto* threshLabel = new QLabel("경고 임계치 (키 입력 속도):");
            threshLabel->setFont(pretendardFont(FontRole::Regular));
            threshLabel->setStyleSheet("color: #314158; font-size: 14px;");
            threshRow->addWidget(threshLabel);
            threshRow->addSpacing(16);

            threshSpin = new QSpinBox;
            threshSpin->setValue(50);
            threshSpin->setRange(1, 9999);
            threshSpin->setFixedSize(80, 34);
            threshSpin->setObjectName("numberInput");
            threshSpin->setFont(pretendardFont(FontRole::Regular));
            threshSpin->setAlignment(Qt::AlignRight);
            threshRow->addWidget(threshSpin);
            threshRow->addSpacing(8);

            auto* threshUnit = new QLabel("ms/key 이하일 경우 차단");
            threshUnit->setFont(pretendardFont(FontRole::Regular));
            threshUnit->setStyleSheet("color: #62748E; font-size: 14px;");
            threshRow->addWidget(threshUnit);
            threshRow->addStretch();

            wl->addLayout(threshRow);
            wl->addStretch();
            tabStack->addWidget(w);
        }

        // Tab 2: Alerts & reports
        {
            auto* w = new QWidget;
            auto* wl = new QVBoxLayout(w);
            wl->setContentsMargins(24, 24, 24, 24);
            wl->setSpacing(0);

            auto* sectionLabel = label("데스크톱 알림", "sectionHeading");
            sectionLabel->setFont(pretendardFont(FontRole::Regular));
            wl->addWidget(sectionLabel);
            wl->addSpacing(16);

            cbNotif = new QCheckBox("위험 장치 연결 시 우측 하단 팝업 알림 표시");
            cbNotif->setObjectName("settingsCheck");
            cbNotif->setFont(pretendardFont(FontRole::Medium));
            cbNotif->setChecked(true);
            wl->addWidget(cbNotif);
            wl->addSpacing(16);
            wl->addStretch();
            tabStack->addWidget(w);
        }

        scL->addWidget(tabBar);
        scL->addWidget(tabStack, 1);
        contentRow->addWidget(settingsCard, 1);
    }

    // Right: Sidebar (340px)
    {
        auto* sidebar = new QWidget;
        sidebar->setFixedWidth(340);
        auto* sidebarL = new QVBoxLayout(sidebar);
        sidebarL->setContentsMargins(0, 0, 0, 0);
        sidebarL->setSpacing(16);

        // Policy summary card
        {
            auto* body = new QWidget;
            auto* bl = new QVBoxLayout(body);
            bl->setContentsMargins(16, 8, 16, 16);
            bl->setSpacing(0);

            struct PolicyRow { QString lbl, val, badge; };
            const QList<PolicyRow> rows = {
                {"활성 정책",   "기본 보안 프리셋", ""},
                {"미등록 장치", "승인 대기",        "warningBadge"},
                {"행위 분석",   "가동 중",          "successBadge"},
                {"허용된 예외", "24개 규칙",        ""},
            };
            for (int i = 0; i < rows.size(); ++i) {
                const auto& r = rows[i];
                auto* line = new QHBoxLayout;
                line->setContentsMargins(0, 8, 0, 8);
                auto* lbl = label(r.lbl);
                lbl->setStyleSheet("color: #45556C; font-size: 14px;");
                lbl->setFont(pretendardFont(FontRole::Regular));
                line->addWidget(lbl);
                line->addStretch();
                if (!r.badge.isEmpty())
                    line->addWidget(label(r.val, r.badge.toUtf8().constData()));
                else {
                    auto* valLbl = label(r.val);
                    valLbl->setStyleSheet("color: #1D293D; font-size: 14px;");
                    valLbl->setFont(pretendardFont(FontRole::Regular));
                    line->addWidget(valLbl);
                }
                bl->addLayout(line);
                if (i < rows.size() - 1) bl->addWidget(makeDivider());
            }

            sidebarL->addWidget(createCard("적용된 정책 요약", body));
        }

        // System info card
        {
            auto* body = new QWidget;
            auto* bl = new QVBoxLayout(body);
            bl->setContentsMargins(16, 16, 16, 16);
            bl->setSpacing(0);

            struct SysInfo { QString lbl, val; };
            const QList<SysInfo> infos = {
                {"에이전트 버전",     "Abang-All Agent v1.4.2 (64-bit)"},
                {"엔진 데이터베이스", "2023.10.25.01 (최신)"},
                {"라이선스 만료일",   "2024-12-31"},
            };
            for (int idx = 0; idx < infos.size(); ++idx) {
                const auto& info = infos[idx];
                auto* col = new QVBoxLayout;
                col->setSpacing(4);
                auto* lbl = label(info.lbl, "systemInfoLabel");
                col->addWidget(lbl);
                auto* val = label(info.val, "systemInfoValue");
                col->addWidget(val);
                bl->addLayout(col);
                if (idx < infos.size() - 1) bl->addSpacing(16);
            }
            bl->addSpacing(16);

            auto* licenseBtn = new QPushButton("라이선스 상세 정보");
            licenseBtn->setObjectName("secondaryButton");
            licenseBtn->setFont(pretendardFont(FontRole::Medium));
            bl->addWidget(licenseBtn);

            sidebarL->addWidget(createCard("시스템 정보", body));
        }

        sidebarL->addStretch();
        contentRow->addWidget(sidebar);
    }

    layout->addLayout(contentRow, 1);

    // Load persisted settings into widgets
    {
        QSettings s("Abang-All", "Abang-All");
        if (cbAutostart)  cbAutostart->setChecked(s.value("general/autostart",  false).toBool());
        if (cbMinimize)   cbMinimize->setChecked( s.value("general/minimize",   false).toBool());
        if (comboDev)     comboDev->setCurrentIndex(s.value("policy/unknownDeviceAction", 0).toInt());
        if (toggleSw)     toggleSw->setChecked(s.value("policy/behaviorMonitor", true).toBool());
        if (threshSpin)   threshSpin->setValue(s.value("policy/thresholdMs", 50).toInt());
        if (cbNotif)      cbNotif->setChecked(s.value("alert/popup", true).toBool());
    }

    // Wire Save button
    if (saveBtn) {
        connect(saveBtn, &QPushButton::clicked, this,
            [this, cbAutostart, cbMinimize, comboDev, toggleSw, threshSpin, cbNotif] {
                QSettings s("Abang-All", "Abang-All");
                if (cbAutostart)  s.setValue("general/autostart",  cbAutostart->isChecked());
                if (cbMinimize)   s.setValue("general/minimize",   cbMinimize->isChecked());
                if (comboDev) {
                    const int idx = comboDev->currentIndex();
                    s.setValue("policy/unknownDeviceAction", idx);
                    unknownDeviceAction_ = idx;
                }
                if (toggleSw)   s.setValue("policy/behaviorMonitor", toggleSw->isChecked());
                if (threshSpin) s.setValue("policy/thresholdMs",     threshSpin->value());
                if (cbNotif)    s.setValue("alert/popup", cbNotif->isChecked());
                s.sync();
            });
    }

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
        headerRow->addWidget(label("도움말", "pageTitle"));
        headerRow->addStretch();
        headerRow->addWidget(makeSearchBox("도움말 검색...", 288));
        layout->addLayout(headerRow);
    }

    auto* contentRow = new QHBoxLayout;
    contentRow->setSpacing(16);

    // ── Right: stacked content (built first so nav can reference it) ──────────
    auto* contentStack = new QStackedWidget;

    // Helper: bullet row
    auto makeBullet = [](const QString& text, QVBoxLayout* parent) {
        auto* row = new QHBoxLayout;
        row->setSpacing(8);
        row->setContentsMargins(8, 0, 0, 0);
        auto* dot = new QLabel("•");
        dot->setStyleSheet("color: #62748E; font-size: 16px;");
        dot->setFixedWidth(12);
        row->addWidget(dot, 0, Qt::AlignTop);
        auto* bText = new QLabel(text);
        bText->setFont(pretendardFont(FontRole::Regular));
        bText->setStyleSheet("color: #45556C; font-size: 14px;");
        bText->setWordWrap(true);
        row->addWidget(bText, 1);
        parent->addLayout(row);
        parent->addSpacing(8);
    };

    auto makeH2 = [](const QString& t) {
        auto* h = new QLabel(t);
        h->setFont(pretendardFont(FontRole::Bold));
        h->setStyleSheet("font-size: 20px; color: #1D293D;");
        return h;
    };
    auto makeH3 = [](const QString& t) {
        auto* h = new QLabel(t);
        h->setFont(pretendardFont(FontRole::SemiBold));
        h->setStyleSheet("font-size: 18px; color: #1D293D;");
        return h;
    };
    auto makePara = [](const QString& t) {
        auto* p = new QLabel(t);
        p->setFont(pretendardFont(FontRole::Regular));
        p->setStyleSheet("color: #45556C; font-size: 14px;");
        p->setWordWrap(true);
        return p;
    };

    // Content page 0: 에이전트 설치 및 초기 설정
    {
        auto* card = cardFrame();
        auto* cl = new QVBoxLayout(card);
        cl->setContentsMargins(32, 32, 80, 32);
        cl->setSpacing(0);
        cl->addWidget(makeH2("Abang-All 에이전트 설치 및 초기 설정"));
        cl->addSpacing(16);
        cl->addWidget(makePara("Abang-All 에이전트는 기업의 엔드포인트 보안을 위해 USB 장치 연결을 제어하고 위험한 행위를 차단합니다."));
        cl->addSpacing(28);
        cl->addWidget(makeH3("1. 설치 과정"));
        cl->addSpacing(16);
        for (const auto& b : {
            "제공된 설치 파일(AbangAll_Setup.exe)을 실행합니다.",
            "관리자 권한을 승인합니다.",
            "화면의 지시에 따라 설치를 완료하고 시스템을 재부팅합니다.",
        }) makeBullet(b, cl);
        cl->addSpacing(8);
        auto* note = new QFrame;
        note->setStyleSheet(QString("QFrame { background: %1; border: 1px solid %2; border-radius: 8px; }")
            .arg(UiTheme::PrimarySoft, UiTheme::PrimarySoftBorder));
        auto* nl = new QHBoxLayout(note);
        nl->setContentsMargins(17, 14, 17, 14);
        auto* noteLbl = new QLabel("참고: 설치 후 트레이 아이콘이 나타나면 정상적으로 보호가 시작된 것입니다.");
        noteLbl->setFont(pretendardFont(FontRole::Medium));
        noteLbl->setStyleSheet(QString("color: %1; font-size: 14px;").arg(UiTheme::PrimaryText));
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
        cl->addWidget(makeH2("승인 대기 장치 허용 방법"));
        cl->addSpacing(16);
        cl->addWidget(makePara("새로운 USB 장치가 연결되면 기본적으로 차단 및 \"승인 대기\" 상태가 됩니다."));
        cl->addSpacing(20);
        for (const auto& b : {
            "좌측 메뉴에서 USB 목록으로 이동합니다.",
            "상태가 [승인 대기]인 장치를 찾아 클릭합니다.",
            "우측 장치 상세 정보 패널에서 하단의 [이 장치 허용] 버튼을 클릭합니다.",
            "확인 후 해당 장치(VID/PID 기준)가 영구 허용 목록에 추가됩니다.",
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
        cl->addWidget(makeH2("행위 분석(BadUSB 차단) 원리"));
        cl->addSpacing(16);
        cl->addWidget(makePara("BadUSB는 일반 USB 저장 장치로 위장하여 시스템에 연결된 후, 키보드처럼 동작하여 악성 스크립트를 초고속으로 자동 입력하는 해킹 기법입니다."));
        cl->addSpacing(28);
        cl->addWidget(makeH3("탐지 및 차단 방식"));
        cl->addSpacing(16);
        for (const auto& b : {
            "인간의 물리적인 키 입력 속도의 한계(일반적으로 50ms 이상)를 모니터링합니다.",
            "설정된 임계치(기본 50ms)보다 빠른 속도로 키 입력이 연속 발생하면, 매크로 또는 스크립트 공격으로 간주합니다.",
            "즉시 해당 인터페이스의 입력을 무효화하고 장치를 차단합니다.",
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
        navHeader->setStyleSheet("QWidget#cardHeader { background: #F8FAFC; border-bottom: 1px solid #F1F5F9; }");
        auto* nhL = new QHBoxLayout(navHeader);
        nhL->setContentsMargins(16, 0, 16, 0);
        nhL->setSpacing(8);
        nhL->addWidget(iconLabel(":/icons/nav-help.svg", 16));
        nhL->addWidget(label("도움말 항목", "cardTitle"));
        nhL->addStretch();
        ncL->addWidget(navHeader);

        auto* navContent = new QWidget;
        auto* navL = new QVBoxLayout(navContent);
        navL->setContentsMargins(8, 8, 8, 8);
        navL->setSpacing(4);

        struct HelpItem { QString category, title; int index; };
        const QList<HelpItem> items = {
            {"시작하기",   "Abang-All 에이전트 설치 및 초기 설정", 0},
            {"정책 관리",  "승인 대기 장치 허용 방법",             1},
            {"기능 가이드","행위 분석(BadUSB 차단) 원리",          2},
        };

        auto navItems = std::make_shared<QVector<QWidget*>>();
        auto titleLabels = std::make_shared<QVector<QLabel*>>();

        for (const auto& item : items) {
            auto* itemW = new QWidget;
            itemW->setCursor(Qt::PointingHandCursor);
            const bool active = (item.index == 2);
            itemW->setStyleSheet(active
                ? QString("QWidget { background: %1; border-radius: 8px; }").arg(UiTheme::PrimarySoft)
                : "QWidget { background: transparent; border-radius: 8px; }");
            auto* iL = new QHBoxLayout(itemW);
            iL->setContentsMargins(12, 10, 12, 10);
            iL->setSpacing(0);

            auto* textBox = new QVBoxLayout;
            textBox->setSpacing(2);
            auto* catLbl = new QLabel(item.category.toUpper());
            catLbl->setStyleSheet("color: #90A1B9; font-size: 10px; letter-spacing: 0.5px;");
            catLbl->setFont(pretendardFont(FontRole::SemiBold));
            textBox->addWidget(catLbl);

            auto* titleLbl = new QLabel(item.title);
            titleLbl->setFont(pretendardFont(FontRole::Medium));
            titleLbl->setStyleSheet(active
                ? QString("color: %1; font-size: 14px;").arg(UiTheme::PrimaryText)
                : QString("color: %1; font-size: 14px;").arg(UiTheme::SubtextMid));
            titleLbl->setWordWrap(true);
            textBox->addWidget(titleLbl);
            iL->addLayout(textBox, 1);

            // Chevron for active item
            auto* chevron = new QLabel("›");
            chevron->setStyleSheet(active
                ? QString("color: %1; font-size: 16px;").arg(UiTheme::PrimaryText)
                : "color: transparent; font-size: 16px;");
            iL->addWidget(chevron);

            navL->addWidget(itemW);
            navItems->push_back(itemW);
            titleLabels->push_back(titleLbl);

            const int idx = item.index;
            auto* mouseFilter = new QObject(itemW);
            connect(mouseFilter, &QObject::destroyed, []{}); // keep alive

            // Use event filter via a helper object
            class NavFilter : public QObject {
                int idx_;
                QStackedWidget* stack_;
                std::shared_ptr<QVector<QWidget*>> navItems_;
                std::shared_ptr<QVector<QLabel*>> titleLabels_;
            public:
                NavFilter(int i, QStackedWidget* s,
                          std::shared_ptr<QVector<QWidget*>> ni,
                          std::shared_ptr<QVector<QLabel*>> tl,
                          QObject* parent)
                    : QObject(parent), idx_(i), stack_(s),
                      navItems_(std::move(ni)), titleLabels_(std::move(tl)) {}
                bool eventFilter(QObject*, QEvent* e) override {
                    if (e->type() == QEvent::MouseButtonRelease) {
                        stack_->setCurrentIndex(idx_);
                        for (int i = 0; i < navItems_->size(); ++i) {
                            const bool sel = (i == idx_);
                            navItems_->at(i)->setStyleSheet(sel
                                ? QString("QWidget { background: %1; border-radius: 8px; }").arg(UiTheme::PrimarySoft)
                                : "QWidget { background: transparent; border-radius: 8px; }");
                            titleLabels_->at(i)->setStyleSheet(sel
                                ? QString("color: %1; font-size: 14px;").arg(UiTheme::PrimaryText)
                                : QString("color: %1; font-size: 14px;").arg(UiTheme::SubtextMid));
                        }
                        return true;
                    }
                    return false;
                }
            };
            itemW->installEventFilter(new NavFilter(idx, contentStack, navItems, titleLabels, itemW));
        }

        navL->addStretch();
        ncL->addWidget(navContent, 1);

        auto* navFooter = new QWidget;
        navFooter->setStyleSheet("background: #F8FAFC; border-top: 1px solid #F1F5F9;");
        auto* nfL = new QVBoxLayout(navFooter);
        nfL->setContentsMargins(16, 14, 16, 14);
        auto* supportBtn = new QPushButton("고객 지원 문의하기");
        supportBtn->setObjectName("secondaryButton");
        supportBtn->setFont(pretendardFont(FontRole::Medium));
        nfL->addWidget(supportBtn);
        ncL->addWidget(navFooter);

        contentRow->addWidget(navCard);
    }

    contentRow->addWidget(contentStack, 1);
    layout->addLayout(contentRow, 1);
    return page;
}

// ===========================================================================
// Helpers
// ===========================================================================
QWidget* MainWindow::createPolicySummaryPage()
{
    auto* page = new QWidget;
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(20, 8, 20, 16);
    layout->setSpacing(0);

    const QList<QStringList> rows = {
        {"미등록 장치 동작", "차단 및 알림",   "policyDangerText"},
        {"행위 분석 임계값", "8.0 keys/sec",   "policyValueText"},
        {"로그 보관 기간",   "30일",           "policyValueText"},
        {"실시간 모니터링",  "사용 중",        "policySuccessText"},
    };
    for (int i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        auto* line = new QHBoxLayout;
        line->setContentsMargins(0, 12, 0, 12);
        line->addWidget(label(row[0]));
        line->addStretch();
        line->addWidget(label(row[1], row[2].toUtf8().constData()));
        layout->addLayout(line);
        if (i < rows.size() - 1) layout->addWidget(makeDivider());
    }

    layout->addStretch();
    auto* editButton = new QPushButton("정책 편집");
    editButton->setObjectName("secondaryButton");
    editButton->setFont(pretendardFont(FontRole::SemiBold));
    editButton->setIcon(QIcon(":/icons/ui-lock.svg"));
    editButton->setIconSize(QSize(16, 16));
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
        if (item.fingerprint != 0 && Policy::IsBlacklisted(item.fingerprint)) {
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
    auto badgeForLine = [](const QString& line) -> std::pair<QString,QString> {
        if (line.contains("block", Qt::CaseInsensitive) || line.contains("blacklist", Qt::CaseInsensitive) || line.contains("차단"))
            return {"차단", "blockBadge"};
        if (line.contains("allow", Qt::CaseInsensitive) || line.contains("whitelist", Qt::CaseInsensitive) || line.contains("허용"))
            return {"허용", "successBadge"};
        if (line.contains("warning", Qt::CaseInsensitive) || line.contains("anomaly", Qt::CaseInsensitive) || line.contains("unknown", Qt::CaseInsensitive))
            return {"확인 필요", "warningBadge"};
        return {"기록", "neutralBadge"};
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
            setCellWidgetBackground(w, Qt::white);
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            auto* lbl = new QLabel(text);
            lbl->setAttribute(Qt::WA_TransparentForMouseEvents);
            lbl->setFont(pretendardFont(FontRole::Regular));
            lbl->setStyleSheet(QString("font-size: 14px; color: %1; background: transparent; border: none;").arg(color));
            l->addWidget(lbl);
            l->addStretch();
            return w;
        };
        auto mkEvent = [&](const QString& iconRes, const QString& text) -> QWidget* {
            auto* w = new QWidget;
            w->setAttribute(Qt::WA_TransparentForMouseEvents);
            setCellWidgetBackground(w, Qt::white);
            auto* l = new QHBoxLayout(w);
            l->setContentsMargins(20, 0, 8, 0);
            l->setSpacing(8);
            auto* ico = new QLabel;
            ico->setAttribute(Qt::WA_TransparentForMouseEvents);
            ico->setFixedSize(16, 16);
            ico->setPixmap(QIcon(iconRes).pixmap(16, 16));
            ico->setStyleSheet("background: transparent;");
            l->addWidget(ico);
            auto* txt = new QLabel(text);
            txt->setAttribute(Qt::WA_TransparentForMouseEvents);
            txt->setFont(pretendardFont(FontRole::Medium));
            txt->setStyleSheet("font-size: 14px; color: #1D293D; background: transparent;");
            l->addWidget(txt);
            l->addStretch();
            return w;
        };

        auto* timeW = mkText(ts, "#62748E");
        timeW->setProperty("log_icon",   icon);
        timeW->setProperty("log_event",  line);
        timeW->setProperty("log_time",   ts);
        timeW->setProperty("log_device", device);
        timeW->setProperty("log_result", result);
        timeW->setProperty("log_badge",  badge);
        logTable_->setCellWidget(insertRow, 0, timeW);
        logTable_->setCellWidget(insertRow, 1, mkEvent(icon, line));
        logTable_->setCellWidget(insertRow, 2, mkText(device, "#45556C"));
        logTable_->setCellWidget(insertRow, 3, makeBadgeCell(result, badge.toUtf8().constData()));
        ++insertRow;
    }
    // Trim any unfilled rows
    if (insertRow < logTable_->rowCount())
        logTable_->setRowCount(insertRow);

    lastLogLineCount_ = total;
    logTable_->scrollToBottom();
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
                        if (!g_activeBufferDlg && unknownDeviceAction_ == 0) {
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
                                    if (Policy::IsWhitelisted(item.fingerprint)) {
                                        // Trusted — silently close the pre-opened dialog.
                                        Logger::Add(L"[Qt] Whitelisted; closing immediate dialog: " + item.name);
                                        if (g_activeBufferDlg) g_activeBufferDlg->close();
                                        enforceBlockedDeviceHook();
                                    } else if (Policy::IsBlacklisted(item.fingerprint)) {
                                        Logger::Add(L"[Qt] Blocked HID device; hook active: " + item.name);
                                        updateDlgInfo();
                                        if (settingEnabled("alert/popup") && trayIcon_) {
                                            trayIcon_->showMessage(
                                                "Abang-All",
                                                QString("차단된 장치가 연결되었습니다.\n%1").arg(fromWString(item.name)),
                                                QSystemTrayIcon::Warning,
                                                4000
                                            );
                                        }
                                        if (!KeyboardHook::IsActive()) KeyboardHook::Install();
                                        blockedDeviceHookActive_ = true;
                                    } else {
                                        // Unknown device — dialog is already open; just update info.
                                        Logger::Add(L"[Qt] Unknown HID device: " + item.name);
                                        if (unknownDeviceAction_ == 1) {
                                            // Auto-block
                                            Policy::AddBlacklist(item);
                                            Logger::Add(L"[Qt] Auto-blocked (설정): " + item.name);
                                            if (settingEnabled("alert/popup") && trayIcon_) {
                                                trayIcon_->showMessage(
                                                    "Abang-All",
                                                    QString("위험 장치가 자동 차단되었습니다.\n%1").arg(fromWString(item.name)),
                                                    QSystemTrayIcon::Critical,
                                                    4000
                                                );
                                            }
                                            if (g_activeBufferDlg) g_activeBufferDlg->close();
                                            if (!KeyboardHook::IsActive()) KeyboardHook::Install();
                                            blockedDeviceHookActive_ = true;
                                        } else if (unknownDeviceAction_ == 2) {
                                            // Auto-allow
                                            Policy::AddWhitelist(item);
                                            Logger::Add(L"[Qt] Auto-allowed (설정): " + item.name);
                                            if (g_activeBufferDlg) g_activeBufferDlg->close();
                                            enforceBlockedDeviceHook();
                                        } else {
                                            updateDlgInfo();
                                            if (settingEnabled("alert/popup") && trayIcon_) {
                                                trayIcon_->showMessage(
                                                    "Abang-All",
                                                    QString("승인 대기 장치가 연결되었습니다.\n%1").arg(fromWString(item.name)),
                                                    QSystemTrayIcon::Warning,
                                                    4000
                                                );
                                            }
                                            lastPromptFingerprint_ = item.fingerprint;
                                        }
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
