// UTF-8
#include "TrayWidget.h"
#include "Theme.h"

#include <QApplication>
#include <QFrame>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

namespace
{
    QLabel* makeLabel(const QString& text, const QString& style)
    {
        auto* label = new QLabel(text);
        label->setStyleSheet(style);
        return label;
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

    QFrame* makeCard(int radius = 12)
    {
        auto* card = new QFrame;
        card->setObjectName("trayCard");
        card->setStyleSheet(QString(
            "QFrame#trayCard { background: #FFFFFF; border: 1px solid %1;"
            " border-radius: %2px; }")
            .arg(UiTheme::PrimarySoftBorder)
            .arg(radius));
        return card;
    }

    QFrame* makeIconCircle(const QString& icon, const QString& border,
                           const QString& bg, int size, int iconSize)
    {
        auto* circle = new QFrame;
        circle->setFixedSize(size, size);
        circle->setStyleSheet(QString(
            "QFrame { background: %1; border: 1px solid %2; border-radius: %3px; }")
            .arg(bg, border)
            .arg(size / 2));

        auto* layout = new QHBoxLayout(circle);
        layout->setContentsMargins(0, 0, 0, 0);

        auto* label = new QLabel;
        label->setFixedSize(iconSize, iconSize);
        label->setStyleSheet("QLabel { background: transparent; border: none; }");
        label->setPixmap(QIcon(icon).pixmap(iconSize, iconSize));
        layout->addWidget(label, 0, Qt::AlignCenter);
        return circle;
    }

    QPushButton* makeHeaderButton(const QString& text)
    {
        auto* button = new QPushButton(text);
        button->setFixedSize(34, 34);
        button->setCursor(Qt::PointingHandCursor);
        button->setStyleSheet(QString(
            "QPushButton { background: transparent; border: none; border-radius: 17px;"
            " color: %1; font-size: 24px; font-weight: 300; }"
            "QPushButton:hover { background: %2; color: %3; }")
            .arg(UiTheme::SubtextMid, UiTheme::PrimarySoft, UiTheme::PrimaryText));
        return button;
    }
}

TrayWidget::TrayWidget(QWidget* parent)
    : QWidget(parent,
              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    setFixedSize(603, 296);

    auto* container = new QFrame(this);
    container->setGeometry(0, 0, width(), height());
    container->setStyleSheet(QString(
        "QFrame { background: #F8FBFF; border: 1px solid %1; border-radius: 14px; }")
        .arg(UiTheme::PrimarySoftBorder));

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* header = new QWidget;
    header->setFixedHeight(68);
    header->setStyleSheet("QWidget { background: transparent; border: none; }");

    auto* headerL = new QHBoxLayout(header);
    headerL->setContentsMargins(20, 6, 20, 6);
    headerL->setSpacing(10);

    auto* brand = new QLabel;
    brand->setFixedSize(40, 40);
    brand->setStyleSheet("QLabel { background: transparent; border: none; }");
    brand->setPixmap(QIcon(":/icons/brand-mark.png").pixmap(38, 38));
    brand->setAlignment(Qt::AlignCenter);
    headerL->addWidget(brand);

    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(2);
    titleBox->setContentsMargins(0, 0, 0, 0);
    auto* titleLabel = makeLabel("Abang-All", labelStyle(21, UiTheme::TextDark, 800));
    auto* subtitleLabel = makeLabel("보안 위젯", labelStyle(13, UiTheme::SubtextMid, 600));
    titleLabel->setAlignment(Qt::AlignLeft);
    subtitleLabel->setAlignment(Qt::AlignLeft);
    titleBox->addWidget(titleLabel);
    titleBox->addWidget(subtitleLabel);
    headerL->addLayout(titleBox);
    headerL->addStretch();

    auto* monitoringBadge = makeLabel("●  모니터링 ON",
        "QLabel { background: #E8FFF3; color: #0B8F52; font-size: 12px;"
        " font-weight: 800; padding: 0; border-radius: 10px;"
        " border: 1px solid #A7E8C8; }");
    monitoringBadge->setFixedSize(104, 32);
    monitoringBadge->setAlignment(Qt::AlignCenter);
    headerL->addWidget(monitoringBadge);
    headerL->addSpacing(6);

    auto* expandBtn = makeHeaderButton("↗");
    auto* closeBtn = makeHeaderButton("×");
    headerL->addWidget(expandBtn);
    headerL->addWidget(closeBtn);
    root->addWidget(header);

    auto* content = new QWidget;
    content->setStyleSheet("QWidget { background: transparent; border: none; }");

    auto* contentL = new QVBoxLayout(content);
    contentL->setContentsMargins(18, 0, 15, 0);
    contentL->setSpacing(0);

    auto* usbCard = makeCard();
    usbCard->setFixedHeight(74);
    auto* usbL = new QHBoxLayout(usbCard);
    usbL->setContentsMargins(24, 7, 25, 7);
    usbL->setSpacing(16);
    usbL->addWidget(makeIconCircle(":/icons/status-shield-blue.svg",
                                   UiTheme::PrimarySoftBorder,
                                   UiTheme::PrimarySoft,
                                   38, 24));

    auto* usbText = new QVBoxLayout;
    usbText->setSpacing(0);
    usbText->addStretch();
    usbText->addWidget(makeLabel("연결된 USB", labelStyle(14, UiTheme::SubtextMid, 700)));
    usbCountLabel_ = makeLabel("0개 장치", labelStyle(22, UiTheme::TextDark, 900));
    usbText->addWidget(usbCountLabel_);
    usbText->addStretch();
    usbL->addLayout(usbText);
    usbL->addStretch();

    auto* realtimeBtn = new QPushButton("실시간");
    realtimeBtn->setFixedSize(72, 30);
    realtimeBtn->setCursor(Qt::PointingHandCursor);
    realtimeBtn->setStyleSheet(QString(
        "QPushButton { color: %1; background: #FFFFFF; border: 1px solid #8AB8FF;"
        " border-radius: 9px; font-size: 14px; font-weight: 800; }"
        "QPushButton:hover { background: #EAF2FF; }")
        .arg(UiTheme::PrimaryHover));
    usbL->addWidget(realtimeBtn);
    contentL->addWidget(usbCard);

    auto* threatCard = makeCard();
    threatCard->setFixedHeight(74);
    auto* threatL = new QHBoxLayout(threatCard);
    threatL->setContentsMargins(24, 7, 25, 7);
    threatL->setSpacing(16);

    threatIconLabel_ = new QLabel;
    threatIconLabel_->setFixedSize(38, 38);
    threatIconLabel_->setAlignment(Qt::AlignCenter);
    threatL->addWidget(threatIconLabel_);

    auto* threatText = new QVBoxLayout;
    threatText->setSpacing(0);
    threatText->addStretch();
    threatText->addWidget(makeLabel("위험 탐지 상태", labelStyle(14, UiTheme::SubtextMid, 700)));

    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(8);
    statusDot_ = makeLabel("●",
        "QLabel { font-size: 19px; color: #F97316; background: transparent; border: none; }");
    statusText_ = makeLabel("경고", labelStyle(22, UiTheme::TextDark, 900));
    statusRow->addWidget(statusDot_);
    statusRow->addWidget(statusText_);
    statusRow->addStretch();
    threatText->addLayout(statusRow);
    threatText->addStretch();
    threatL->addLayout(threatText);
    threatL->addStretch();

    auto* mascot = new QLabel;
    mascot->setFixedSize(76, 76);
    mascot->setStyleSheet("QLabel { background: transparent; border: none; }");
    mascot->setPixmap(QPixmap(":/icons/mascot_ver3.png").scaled(
        76, 76, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    mascot->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* opacity = new QGraphicsOpacityEffect(mascot);
    opacity->setOpacity(0.32);
    mascot->setGraphicsEffect(opacity);

    statusBadge_ = new QPushButton("주의");
    statusBadge_->setFixedSize(72, 30);
    statusBadge_->setCursor(Qt::PointingHandCursor);
    threatL->addWidget(mascot, 0, Qt::AlignVCenter);
    threatL->addWidget(statusBadge_, 0, Qt::AlignVCenter);

    contentL->addWidget(threatCard);
    root->addWidget(content, 1);

    auto* footer = new QWidget;
    footer->setFixedHeight(44);
    footer->setStyleSheet(QString(
        "QWidget { background: #F8FBFF; border-top: 1px solid %1;"
        " border-bottom-left-radius: 14px; border-bottom-right-radius: 14px; }")
        .arg(UiTheme::PrimarySoftBorder));

    auto* footerL = new QHBoxLayout(footer);
    footerL->setContentsMargins(18, 0, 17, 0);
    footerL->setSpacing(8);
    footerL->addWidget(makeLabel("최근 이벤트", labelStyle(14, UiTheme::PrimaryHover, 800)));

    auto* divider = new QFrame;
    divider->setFixedSize(1, 20);
    divider->setStyleSheet(QString("QFrame { background: %1; border: none; }").arg(UiTheme::Border));
    footerL->addWidget(divider);

    eventText_ = makeLabel("-", labelStyle(14, UiTheme::TextDark, 700));
    eventText_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    footerL->addWidget(eventText_, 1);

    eventTime_ = makeLabel("--:--:--", labelStyle(13, UiTheme::SubtextMid, 700));
    footerL->addWidget(eventTime_);
    root->addWidget(footer);

    connect(expandBtn, &QPushButton::clicked, this, &TrayWidget::expandRequested);
    connect(realtimeBtn, &QPushButton::clicked, this, &TrayWidget::expandRequested);
    connect(closeBtn, &QPushButton::clicked, this, &QWidget::hide);

    setUsbCount(0);
    setRecentEvent("--:--:--", "이벤트 대기중");
    setThreatLevel(ThreatLevel::Normal);
}

void TrayWidget::setThreatLevel(ThreatLevel level)
{
    level_ = level;
    applyLevel();
}

void TrayWidget::setUsbCount(int count)
{
    if (usbCountLabel_)
        usbCountLabel_->setText(QString("%1개 장치").arg(count));
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
    struct Cfg {
        const char* dotColor;
        const char* icon;
        const char* statusStr;
        const char* badgeText;
        const char* iconBg;
        const char* iconBorder;
        QString badgeStyle;
    };

    const QString safeBadge =
        "QPushButton { font-size: 13px; font-weight: 800; color: #087A45;"
        " background: #ECFFF6; border: 1px solid #9EE7C0; border-radius: 9px; }"
        "QPushButton:hover { background: #DDFBEA; }";
    const QString warningBadge = QString(
        "QPushButton { font-size: 13px; font-weight: 800; color: %1;"
        " background: %2; border: 1px solid %3; border-radius: 9px; }"
        "QPushButton:hover { background: #FFF3D9; }")
        .arg(UiTheme::WarningText, UiTheme::WarningSoft, UiTheme::WarningBorder);
    const QString dangerBadge = QString(
        "QPushButton { font-size: 13px; font-weight: 800; color: #FFFFFF;"
        " background: %1; border: 1px solid %1; border-radius: 9px; }"
        "QPushButton:hover { background: %2; }")
        .arg(UiTheme::DangerAction, UiTheme::DangerHover);

    const Cfg cfgs[3] = {
        {"#00BC7D", ":/icons/status-shield-green.svg", "정상", "안전", "#F0FFF8", "#BCE7D4", safeBadge},
        {UiTheme::Warning, ":/icons/status-warning.svg", "경고", "주의", UiTheme::WarningSoft, UiTheme::WarningBorder, warningBadge},
        {UiTheme::DangerAction, ":/icons/status-blocked.svg", "위험", "차단", "#FFF1F2", "#FECDD3", dangerBadge},
    };

    const int idx = static_cast<int>(level_);
    const Cfg& cfg = cfgs[idx];

    statusDot_->setStyleSheet(
        QString("QLabel { font-size: 19px; color: %1; background: transparent; border: none; }")
            .arg(cfg.dotColor));
    statusText_->setText(cfg.statusStr);
    statusBadge_->setText(cfg.badgeText);
    statusBadge_->setStyleSheet(cfg.badgeStyle);

    if (threatIconLabel_) {
        threatIconLabel_->setStyleSheet(QString(
            "QLabel { background: %1; border: 1px solid %2; border-radius: 19px; }")
            .arg(cfg.iconBg, cfg.iconBorder));
        threatIconLabel_->setPixmap(QIcon(cfg.icon).pixmap(21, 21));
    }
}

void TrayWidget::positionNearTray()
{
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    move(screen.right() - width() - 8,
         screen.bottom() - height() - 8);
}
