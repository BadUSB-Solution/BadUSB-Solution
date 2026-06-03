#include "UpdateChecker.h"

#include "AppVersion.h"

#include <QDesktopServices>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QPushButton>
#include <QScopeGuard>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

namespace
{
    // latest.json 위치 및 기본 다운로드 페이지 (Vercel 배포 사이트)
    constexpr char kLatestJsonUrl[]    = "https://abang-all.vercel.app/latest.json";
    constexpr char kDefaultDownloadUrl[] = "https://abang-all.vercel.app/";

    // "1.0.10" 같은 점 구분 버전 비교. remote > local 이면 true.
    // 자릿수가 달라도(예: "1.0" vs "1.0.0") 모자란 쪽을 0 으로 채워 비교한다.
    bool isRemoteNewer(const QString& remote, const QString& local)
    {
        const QStringList r = remote.split('.', Qt::SkipEmptyParts);
        const QStringList l = local.split('.', Qt::SkipEmptyParts);
        const int count = qMax(r.size(), l.size());
        for (int i = 0; i < count; ++i) {
            const int rv = (i < r.size()) ? r.at(i).toInt() : 0;
            const int lv = (i < l.size()) ? l.at(i).toInt() : 0;
            if (rv != lv) {
                return rv > lv;
            }
        }
        return false;
    }

    // 동시에 여러 번 확인하거나(주기 타이머 + 시작 확인 겹침) 다이얼로그가
    // 중첩되는 것을 막는 가드. 확인이 끝나거나 다이얼로그가 닫히면 해제된다.
    bool g_checkInFlight = false;
}

void UpdateChecker::checkAsync(QWidget* parent)
{
    if (g_checkInFlight) {
        return; // 이미 확인 중(또는 알림 다이얼로그 표시 중) — 중복 방지
    }
    g_checkInFlight = true;

    // manager 는 요청이 끝날 때까지 살아 있어야 하므로 힙에 만들고 finished 에서 정리.
    auto* manager = new QNetworkAccessManager(parent ? static_cast<QObject*>(parent) : nullptr);

    QNetworkRequest request{ QUrl(QString::fromLatin1(kLatestJsonUrl)) };
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QByteArrayLiteral("AbangAll-UpdateChecker"));

    QNetworkReply* reply = manager->get(request);
    const QPointer<QWidget> parentGuard(parent);

    QObject::connect(reply, &QNetworkReply::finished, manager, [reply, manager, parentGuard]() {
        // 어느 경로로 끝나든(오류/무시/다이얼로그 닫힘) 가드를 풀어준다.
        const auto resetInFlight = qScopeGuard([]() { g_checkInFlight = false; });

        reply->deleteLater();
        manager->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            return; // 네트워크 오류 — 조용히 무시
        }

        QJsonParseError parseError{};
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
            return; // 파싱 실패 — 무시
        }

        const QJsonObject obj = doc.object();
        const QString remoteVersion = obj.value(QStringLiteral("version")).toString();
        if (remoteVersion.isEmpty()) {
            return;
        }

        if (!isRemoteNewer(remoteVersion, QStringLiteral(ABANGALL_VERSION))) {
            return; // 최신이거나 동일 — 알림 없음
        }

        const QString downloadUrl =
            obj.value(QStringLiteral("url")).toString(QString::fromLatin1(kDefaultDownloadUrl));
        const QString notes = obj.value(QStringLiteral("notes")).toString();

        QString text = QObject::tr("새 버전 %1 이(가) 있습니다. (현재 버전 %2)")
                           .arg(remoteVersion, QStringLiteral(ABANGALL_VERSION));
        if (!notes.isEmpty()) {
            text += QStringLiteral("\n\n") + notes;
        }
        text += QStringLiteral("\n\n") + QObject::tr("지금 다운로드 페이지를 여시겠습니까?");

        QMessageBox box(parentGuard);
        box.setIcon(QMessageBox::Information);
        box.setWindowTitle(QObject::tr("업데이트 알림"));
        box.setText(text);
        // 전역 폰트가 커서 버튼/본문 글씨가 과도하게 보임 — 이 다이얼로그에만 축소.
        box.setStyleSheet(
            QStringLiteral("QLabel { font-size: 13px; }"
                           "QPushButton { font-size: 12px; padding: 5px 16px; min-width: 64px; }"));
        QPushButton* updateButton = box.addButton(QObject::tr("업데이트"), QMessageBox::AcceptRole);
        box.addButton(QObject::tr("나중에"), QMessageBox::RejectRole);
        box.setDefaultButton(updateButton);
        box.exec();

        if (box.clickedButton() == updateButton) {
            QDesktopServices::openUrl(QUrl(downloadUrl));
        }
    });
}

void UpdateChecker::start(QWidget* parent)
{
    checkAsync(parent); // 시작 시 즉시 1회

    // 이후 12시간마다 재확인. timer 는 parent 에 종속 → 창이 사라지면 함께 정리됨.
    auto* timer = new QTimer(parent ? static_cast<QObject*>(parent) : nullptr);
    timer->setInterval(12 * 60 * 60 * 1000); // 12시간(ms)
    QObject::connect(timer, &QTimer::timeout, timer, [parent]() {
        UpdateChecker::checkAsync(parent);
    });
    timer->start();
}
