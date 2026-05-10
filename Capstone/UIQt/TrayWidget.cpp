// UTF-8
#include "TrayWidget.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>
#include <QApplication>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
namespace
{
    QLabel* makeLabel(const QString& text, const QString& style)
    {
        auto* lbl = new QLabel(text);
        lbl->setStyleSheet(style);
        return lbl;
    }

    QFrame* makeCard()
    {
        auto* card = new QFrame;
        card->setStyleSheet(
            "QFrame { background: white; border: 1px solid #E2E8F0; border-radius: 8px; }");
        return card;
    }
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------
TrayWidget::TrayWidget(QWidget* parent)
    : QWidget(parent,
              Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint)
{
    setFixedSize(470, 249);

    // ---- Outer container fills the widget exactly — no shadow padding gap ----
    auto* container = new QFrame(this);
    container->setGeometry(0, 0, 470, 249);
    container->setStyleSheet(R"(
        QFrame {
            background: white;
            border-radius: 12px;
            border: 1px solid #E2E8F0;
        }
    )");

    auto* root = new QVBoxLayout(container);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // ==================================================================
    // HEADER
    // ==================================================================
    auto* header = new QWidget;
    header->setFixedHeight(48);
    header->setStyleSheet(R"(
        QWidget {
            background: white;
            border-top-left-radius: 12px;
            border-top-right-radius: 12px;
            border-bottom: 1px solid #F1F5F9;
        }
    )");

    auto* headerL = new QHBoxLayout(header);
    headerL->setContentsMargins(14, 0, 10, 0);
    headerL->setSpacing(8);

    // Shield icon
    auto* shieldLbl = new QLabel;
    shieldLbl->setFixedSize(18, 18);
    shieldLbl->setPixmap(QIcon(":/icons/brand-shield.svg").pixmap(18, 18));
    shieldLbl->setStyleSheet("QLabel { background: transparent; border: none; }");
    headerL->addWidget(shieldLbl);

    // App name + subtitle
    auto* titleBox = new QVBoxLayout;
    titleBox->setSpacing(0);
    titleBox->setContentsMargins(0, 0, 0, 0);
    titleBox->addWidget(makeLabel("Abang-All",
        "QLabel { font-size:13px; font-weight:700; color:#1D293D; background:transparent; border:none; }"));
    titleBox->addWidget(makeLabel("보안 위젯",
        "QLabel { font-size:10px; color:#62748E; background:transparent; border:none; }"));
    headerL->addLayout(titleBox);
    headerL->addStretch();

    // Monitoring badge
    auto* monBadge = makeLabel("● 모니터링 ON", R"(
        QLabel {
            background: #DCFCE7;
            color: #15803D;
            font-size: 10px;
            font-weight: 600;
            padding: 2px 6px;
            border-radius: 6px;
            border: none;
        }
    )");
    monBadge->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    headerL->addWidget(monBadge);
    headerL->addSpacing(4);

    // Expand button
    auto* expandBtn = new QPushButton("↗");
    expandBtn->setFixedSize(28, 28);
    expandBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            font-size: 15px;
            color: #62748E;
        }
        QPushButton:hover { color: #1D293D; }
    )");
    headerL->addWidget(expandBtn);

    // Close button
    auto* closeBtn = new QPushButton("✕");
    closeBtn->setFixedSize(28, 28);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background: transparent;
            border: none;
            font-size: 13px;
            color: #62748E;
        }
        QPushButton:hover { color: #1D293D; }
    )");
    headerL->addWidget(closeBtn);

    root->addWidget(header);

    // ==================================================================
    // CONTENT (gray background, two cards)
    // ==================================================================
    auto* content = new QWidget;
    content->setStyleSheet("QWidget { background: #F8FAFC; border: none; border-radius: 0; }");

    auto* contentL = new QVBoxLayout(content);
    contentL->setContentsMargins(12, 10, 12, 10);
    contentL->setSpacing(8);

    // ---- USB card ----
    auto* usbCard = makeCard();
    auto* usbL = new QHBoxLayout(usbCard);
    usbL->setContentsMargins(12, 10, 12, 10);
    usbL->setSpacing(8);

    auto* usbIcon = new QLabel;
    usbIcon->setFixedSize(16, 16);
    usbIcon->setPixmap(QIcon(":/icons/nav-usb.svg").pixmap(16, 16));
    usbIcon->setStyleSheet("QLabel { background: transparent; border: none; }");
    usbL->addWidget(usbIcon);

    auto* usbInfoBox = new QVBoxLayout;
    usbInfoBox->setSpacing(1);
    usbInfoBox->addWidget(makeLabel("연결된 USB",
        "QLabel { font-size:11px; color:#62748E; background:transparent; border:none; }"));
    usbCountLabel_ = new QLabel("— 개 장치");
    usbCountLabel_->setStyleSheet(
        "QLabel { font-size:17px; font-weight:700; color:#1D293D; background:transparent; border:none; }");
    usbInfoBox->addWidget(usbCountLabel_);
    usbL->addLayout(usbInfoBox);
    usbL->addStretch();

    auto* realtimeBtn = new QPushButton("실시간");
    realtimeBtn->setStyleSheet(R"(
        QPushButton {
            font-size: 11px;
            color: #62748E;
            background: white;
            border: 1px solid #E2E8F0;
            border-radius: 6px;
            padding: 3px 10px;
        }
    )");
    usbL->addWidget(realtimeBtn);
    contentL->addWidget(usbCard);

    // ---- Threat card ----
    auto* threatCard = makeCard();
    auto* threatL = new QHBoxLayout(threatCard);
    threatL->setContentsMargins(12, 10, 12, 10);
    threatL->setSpacing(8);

    auto* alertIcon = new QLabel;
    alertIcon->setFixedSize(16, 16);
    alertIcon->setPixmap(QIcon(":/icons/status-warning.svg").pixmap(16, 16));
    alertIcon->setStyleSheet("QLabel { background: transparent; border: none; }");
    threatL->addWidget(alertIcon);

    auto* threatInfoBox = new QVBoxLayout;
    threatInfoBox->setSpacing(2);
    threatInfoBox->addWidget(makeLabel("위협 탐지 상태",
        "QLabel { font-size:11px; color:#62748E; background:transparent; border:none; }"));

    auto* statusRow = new QHBoxLayout;
    statusRow->setSpacing(5);
    statusRow->setContentsMargins(0, 0, 0, 0);

    statusDot_ = new QLabel("●");
    statusDot_->setStyleSheet(
        "QLabel { font-size:13px; background:transparent; border:none; }");
    statusRow->addWidget(statusDot_);

    statusText_ = new QLabel("—");
    statusText_->setStyleSheet(
        "QLabel { font-size:17px; font-weight:700; color:#1D293D; background:transparent; border:none; }");
    statusRow->addWidget(statusText_);
    statusRow->addStretch();

    threatInfoBox->addLayout(statusRow);
    threatL->addLayout(threatInfoBox);
    threatL->addStretch();

    statusBadge_ = new QPushButton("—");
    statusBadge_->setFixedHeight(28);
    threatL->addWidget(statusBadge_);
    contentL->addWidget(threatCard);

    root->addWidget(content, 1);

    // ==================================================================
    // FOOTER (recent event)
    // ==================================================================
    auto* footer = new QWidget;
    footer->setFixedHeight(40);
    footer->setStyleSheet(R"(
        QWidget {
            background: white;
            border-top: 1px solid #F1F5F9;
            border-bottom-left-radius: 12px;
            border-bottom-right-radius: 12px;
        }
    )");

    auto* footerL = new QHBoxLayout(footer);
    footerL->setContentsMargins(14, 0, 14, 0);
    footerL->setSpacing(8);

    footerL->addWidget(makeLabel("최근 이벤트",
        "QLabel { font-size:11px; color:#62748E; background:transparent; border:none; min-width:58px; }"));

    eventText_ = new QLabel("—");
    eventText_->setStyleSheet(
        "QLabel { font-size:12px; color:#1D293D; background:transparent; border:none; }");
    eventText_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    footerL->addWidget(eventText_, 1);

    eventTime_ = new QLabel("--:--:--");
    eventTime_->setStyleSheet(
        "QLabel { font-size:11px; color:#62748E; background:transparent; border:none; }");
    footerL->addWidget(eventTime_);

    root->addWidget(footer);

    // ==================================================================
    // Signals
    // ==================================================================
    connect(expandBtn, &QPushButton::clicked, this, &TrayWidget::expandRequested);
    connect(closeBtn,  &QPushButton::clicked, this, &QWidget::hide);

    // ==================================================================
    // Initial mock data
    // ==================================================================
    setUsbCount(3);
    setRecentEvent("14:22:33", "미승인 HID 장치 연결 시도 차단됨");
    setThreatLevel(ThreatLevel::Warning);
}

// ---------------------------------------------------------------------------
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
    if (eventText_)  eventText_->setText(text);
    if (eventTime_)  eventTime_->setText(time);
}

// ---------------------------------------------------------------------------
void TrayWidget::applyLevel()
{
    struct Cfg {
        const char* dotColor;
        const char* statusStr;
        const char* badgeText;
        const char* badgeStyle;
    };

    static const Cfg cfgs[3] = {
        // Normal
        {
            "#00C950", "정상", "안전",
            "QPushButton { font-size:11px; font-weight:600; color:#15803D;"
            " background:#DCFCE7; border:none; border-radius:6px; padding:3px 12px; }"
        },
        // Warning
        {
            "#F97316", "경고", "주의 필요",
            "QPushButton { font-size:11px; font-weight:600; color:#F97316;"
            " background:white; border:1px solid #F97316; border-radius:6px; padding:3px 12px; }"
        },
        // Danger
        {
            "#EC003F", "위험", "즉시 대응",
            "QPushButton { font-size:11px; font-weight:600; color:white;"
            " background:#EC003F; border:none; border-radius:6px; padding:3px 12px; }"
        },
    };

    const int idx = static_cast<int>(level_);
    statusDot_->setStyleSheet(
        QString("QLabel { font-size:13px; color:%1; background:transparent; border:none; }")
            .arg(cfgs[idx].dotColor));
    statusText_->setText(cfgs[idx].statusStr);
    statusBadge_->setText(cfgs[idx].badgeText);
    statusBadge_->setStyleSheet(cfgs[idx].badgeStyle);
}

// ---------------------------------------------------------------------------
void TrayWidget::positionNearTray()
{
    const QRect screen = QApplication::primaryScreen()->availableGeometry();
    move(screen.right() - width() - 8,
         screen.bottom() - height() - 8);
}
