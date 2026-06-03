// UTF-8
#include "TrayWidget.h"
#include "Theme.h"

#include <QApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace
{
    QLabel* makeLabel(const QString& text)
    {
        return new QLabel(text);
    }

    QString labelStyle(int size, const QString& color, int weight = 400)
    {
        return QString(
            "QLabel { font-size: %1px; font-weight: %2; color: %3;"
            " background: transparent; border: none; }")
            .arg(size)
            .arg(weight)
            .arg(color);
    }
}

TrayWidget::TrayWidget(QWidget* parent)
    : QWidget(parent,
              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    setFixedSize(520, 300);

    container_ = new QFrame(this);
    container_->setGeometry(0, 0, width(), height());

    auto* root = new QVBoxLayout(container_);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget;
    header->setFixedHeight(78);
    header->setStyleSheet("QWidget { background: transparent; border: none; }");

    auto* headerL = new QHBoxLayout(header);
    headerL->setContentsMargins(24, 12, 22, 8);
    headerL->setSpacing(12);

    auto* brand = new QLabel;
    brand->setFixedSize(52, 52);
    brand->setStyleSheet("QLabel { background: transparent; border: none; }");
    brand->setPixmap(QIcon(":/icons/brand-mark.png").pixmap(50, 50));
    brand->setAlignment(Qt::AlignCenter);
    headerL->addWidget(brand);

    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleLabel_ = makeLabel("Abang-All");
    subtitleLabel_ = makeLabel(tr("보안 위젯"));
    titleLabel_->setAlignment(Qt::AlignLeft);
    subtitleLabel_->setAlignment(Qt::AlignLeft);
    titleBox->addWidget(titleLabel_);
    titleBox->addWidget(subtitleLabel_);
    headerL->addLayout(titleBox);
    headerL->addStretch();

    monitoringBadge_ = makeLabel(tr("● 모니터링 ON"));
    monitoringBadge_->setFixedSize(144, 34);
    monitoringBadge_->setAlignment(Qt::AlignCenter);
    headerL->addWidget(monitoringBadge_);
    headerL->addSpacing(4);

    minimizeBtn_ = new QPushButton("-");
    minimizeBtn_->setObjectName("trayIconButton");
    minimizeBtn_->setFixedSize(34, 34);
    minimizeBtn_->setCursor(Qt::PointingHandCursor);

    expandBtn_ = new QPushButton("↗");
    expandBtn_->setObjectName("trayExpandButton");
    expandBtn_->setFixedSize(34, 34);
    expandBtn_->setCursor(Qt::PointingHandCursor);

    closeBtn_ = new QPushButton("×");
    closeBtn_->setObjectName("trayIconButton");
    closeBtn_->setFixedSize(34, 34);
    closeBtn_->setCursor(Qt::PointingHandCursor);

    headerL->addWidget(minimizeBtn_);
    headerL->addWidget(expandBtn_);
    headerL->addWidget(closeBtn_);
    root->addWidget(header);

    auto* content = new QWidget;
    content->setStyleSheet("QWidget { background: transparent; border: none; }");

    auto* contentL = new QVBoxLayout(content);
    contentL->setContentsMargins(24, 8, 24, 0);
    contentL->setSpacing(12);

    usbCard_ = new QFrame;
    usbCard_->setObjectName("trayCard");
    usbCard_->setFixedHeight(82);
    auto* usbL = new QHBoxLayout(usbCard_);
    usbL->setContentsMargins(22, 12, 22, 12);
    usbL->setSpacing(20);

    usbIconCircle_ = new QFrame;
    usbIconCircle_->setFixedSize(54, 54);
    {
        auto* layout = new QHBoxLayout(usbIconCircle_);
        layout->setContentsMargins(0, 0, 0, 0);
        auto* label = new QLabel;
        label->setFixedSize(28, 28);
        label->setStyleSheet("QLabel { background: transparent; border: none; }");
        label->setPixmap(QIcon(":/icons/status-shield-blue.svg").pixmap(28, 28));
        layout->addWidget(label, 0, Qt::AlignCenter);
    }
    usbL->addWidget(usbIconCircle_);

    auto* usbText = new QVBoxLayout;
    usbText->setSpacing(4);
    usbText->setContentsMargins(0, 0, 0, 0);
    usbText->addStretch();
    usbHeaderLabel_ = makeLabel(tr("연결된 USB"));
    usbText->addWidget(usbHeaderLabel_);
    usbCountLabel_ = makeLabel(tr("%1개 장치").arg(0));
    usbText->addWidget(usbCountLabel_);
    usbText->addStretch();
    usbL->addLayout(usbText);
    usbL->addStretch();

    realtimeBtn_ = new QPushButton(tr("실시간"));
    realtimeBtn_->setFixedSize(88, 36);
    realtimeBtn_->setCursor(Qt::PointingHandCursor);
    usbL->addWidget(realtimeBtn_);
    contentL->addWidget(usbCard_);

    threatCard_ = new QFrame;
    threatCard_->setObjectName("trayCard");
    threatCard_->setFixedHeight(82);
    auto* threatL = new QHBoxLayout(threatCard_);
    threatL->setContentsMargins(22, 12, 22, 12);
    threatL->setSpacing(20);

    threatIconLabel_ = new QLabel;
    threatIconLabel_->setFixedSize(54, 54);
    threatIconLabel_->setAlignment(Qt::AlignCenter);
    threatL->addWidget(threatIconLabel_);

    auto* threatText = new QVBoxLayout;
    threatText->setSpacing(4);
    threatText->setContentsMargins(0, 0, 0, 0);
    threatText->addStretch();
    threatHeaderLabel_ = makeLabel(tr("위험 탐지 상태"));
    threatText->addWidget(threatHeaderLabel_);
    statusText_ = makeLabel(tr("주의"));
    threatText->addWidget(statusText_);
    threatText->addStretch();
    threatL->addLayout(threatText);
    threatL->addStretch();

    mascot_ = nullptr;

    contentL->addWidget(threatCard_);
    root->addWidget(content, 1);

    footer_ = new QWidget;
    footer_->setFixedHeight(46);

    auto* footerL = new QHBoxLayout(footer_);
    footerL->setContentsMargins(24, 0, 24, 0);
    footerL->setSpacing(12);
    footerLabel_ = makeLabel(tr("최근 이벤트"));
    footerL->addWidget(footerLabel_);

    footerDivider_ = new QFrame;
    footerDivider_->setFixedSize(1, 20);
    footerL->addWidget(footerDivider_);

    eventText_ = makeLabel("-");
    eventText_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    footerL->addWidget(eventText_, 1);

    eventTime_ = makeLabel("--:--:--");
    footerL->addWidget(eventTime_);
    root->addWidget(footer_);

    connect(expandBtn_, &QPushButton::clicked, this, &TrayWidget::expandRequested);
    connect(realtimeBtn_, &QPushButton::clicked, this, &TrayWidget::expandRequested);
    connect(minimizeBtn_, &QPushButton::clicked, this, &QWidget::hide);
    connect(closeBtn_, &QPushButton::clicked, this, &QWidget::hide);

    setUsbCount(0);
    setRecentEvent("--:--:--", tr("이벤트 대기중"));

    applyTheme();
    setThreatLevel(ThreatLevel::Normal);
}

void TrayWidget::applyTheme()
{
    const auto& t = UiTheme::tokens();
    const bool dark = UiTheme::isDark();

    container_->setStyleSheet(QString(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 16px; }")
        .arg(t.White, t.Border));

    titleLabel_->setStyleSheet(labelStyle(24, t.TextDark, 850));
    subtitleLabel_->setStyleSheet(labelStyle(13, t.SubtextMid, 500));

    const QString monBg     = dark ? "#0F2E1F" : "#E8FFF3";
    const QString monText   = dark ? "#5EE0A5" : "#0B8F52";
    const QString monBorder = dark ? "#1F5E3D" : "#A7E8C8";
    monitoringBadge_->setStyleSheet(QString(
        "QLabel { background: %1; color: %2; font-size: 14px;"
        " font-weight: 800; padding: 0 12px; border-radius: 17px;"
        " border: 1px solid %3; }")
        .arg(monBg, monText, monBorder));

    const QString iconBtn = QString(
        "QPushButton#trayIconButton { background: transparent; border: none;"
        " border-radius: 17px; color: %1; font-size: 24px; font-weight: 600; }"
        "QPushButton#trayIconButton:hover { background: %2; color: %3; }")
        .arg(t.SubtextMid, t.PrimarySoft, t.PrimaryText);
    minimizeBtn_->setStyleSheet(iconBtn);
    closeBtn_->setStyleSheet(iconBtn);

    expandBtn_->setStyleSheet(QString(
        "QPushButton#trayExpandButton { background: %1; border: 1px solid %2;"
        " border-radius: 17px; color: %3; font-size: 22px; font-weight: 800; }"
        "QPushButton#trayExpandButton:hover { background: %4; color: %5; }")
        .arg(t.SurfaceAlt, t.Border, t.SubtextMid, t.PrimarySoft, t.PrimaryText));

    const QString cardStyle = QString(
        "QFrame#trayCard { background: %1; border: 1px solid %2; border-radius: 14px; }")
        .arg(t.Surface, t.Border);
    usbCard_->setStyleSheet(cardStyle);
    threatCard_->setStyleSheet(cardStyle);

    usbIconCircle_->setStyleSheet(QString(
        "QFrame { background: %1; border: 1px solid %2; border-radius: 27px; }")
        .arg(t.PrimarySoft, t.PrimarySoftBorder));

    usbHeaderLabel_->setStyleSheet(labelStyle(15, t.SubtextMid, 650));
    usbCountLabel_->setStyleSheet(labelStyle(21, t.TextDark, 900));

    const QString rtBg      = dark ? "#102F56" : "#EAF2FF";
    const QString rtText    = dark ? "#9CCBFF" : t.PrimaryHover;
    const QString rtBorder  = dark ? "#235D9C" : "#B7D4FF";
    const QString rtHoverBg = dark ? "#173E6E" : "#DCE7FB";
    realtimeBtn_->setStyleSheet(QString(
        "QPushButton { color: %1; background: %2; border: 1px solid %3;"
        " border-radius: 10px; font-size: 14px; font-weight: 800; }"
        "QPushButton:hover { background: %4; }")
        .arg(rtText, rtBg, rtBorder, rtHoverBg));

    threatHeaderLabel_->setStyleSheet(labelStyle(15, t.SubtextMid, 650));

    footer_->setStyleSheet(QString(
        "QWidget { background: %1; border-top: 1px solid %2;"
        " border-bottom-left-radius: 16px; border-bottom-right-radius: 16px; }")
        .arg(t.Surface, t.Border));
    footerLabel_->setStyleSheet(labelStyle(15, t.PrimaryHover, 850));
    footerDivider_->setStyleSheet(QString("QFrame { background: %1; border: none; }").arg(t.Border));
    eventText_->setStyleSheet(labelStyle(15, t.TextDark, 650));
    eventTime_->setStyleSheet(labelStyle(14, t.SubtextMid, 650));

    applyLevel();
}

void TrayWidget::setThreatLevel(ThreatLevel level)
{
    level_ = level;
    applyLevel();
}

void TrayWidget::setUsbCount(int count)
{
    if (usbCountLabel_)
        usbCountLabel_->setText(tr("%1개 장치").arg(count));
}

void TrayWidget::setRecentEvent(const QString& time, const QString& text)
{
    if (eventText_)
        eventText_->setText(text);
    if (eventTime_)
        eventTime_->setText(time);
}

void TrayWidget::applyLevel()
{
    if (!statusText_ || !threatIconLabel_)
        return;

    const auto& t = UiTheme::tokens();
    const bool dark = UiTheme::isDark();

    struct Cfg {
        const char* statusText;
        const char* statusColor;
        const char* icon;
        QString iconBg;
        QString iconBorder;
    };

    const Cfg cfgs[3] = {
        {
            "안전",
            "#2CC58A",
            ":/icons/status-shield-green.svg",
            dark ? QString("#0F2E1F") : QString("#F0FFF8"),
            dark ? QString("#1F5E3D") : QString("#BCE7D4"),
        },
        {
            "주의",
            t.Warning,
            ":/icons/status-warning.svg",
            QString(t.WarningSoft), QString(t.WarningBorder),
        },
        {
            "차단",
            t.DangerAction,
            ":/icons/status-blocked.svg",
            dark ? QString("#321421") : QString("#FFF1F2"),
            dark ? QString("#86324C") : QString("#FECDD3"),
        },
    };

    const int idx = static_cast<int>(level_);
    const Cfg& cfg = cfgs[idx];

    statusText_->setText(tr(cfg.statusText));
    statusText_->setStyleSheet(labelStyle(21, cfg.statusColor, 900));

    threatIconLabel_->setStyleSheet(QString(
        "QLabel { background: %1; border: 1px solid %2; border-radius: 27px; }")
        .arg(cfg.iconBg, cfg.iconBorder));
    threatIconLabel_->setPixmap(QIcon(cfg.icon).pixmap(28, 28));
}

void TrayWidget::positionNearTray()
{
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    move(screen.right() - width() - 12,
         screen.bottom() - height() - 12);
}
