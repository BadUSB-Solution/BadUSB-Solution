#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QIcon>
#include <QLocalServer>
#include <QLocalSocket>
#include <QSettings>
#include <QStringList>
#include <QTranslator>

#include "MainWindow.h"
#include "Theme.h"

namespace
{
    static constexpr char kServerName[] = "AbangAllSingleInstance";

    QString loadPretendardFamily()
    {
        const QStringList fontDirectories = {
            qEnvironmentVariable("LOCALAPPDATA") + "/Microsoft/Windows/Fonts",
            qEnvironmentVariable("WINDIR") + "/Fonts",
        };

        QString loadedRegularFamily;
        for (const auto& directoryPath : fontDirectories) {
            const QDir directory(directoryPath);
            if (!directory.exists())
                continue;

            const auto regularPath = directory.absoluteFilePath("Pretendard-Regular.otf");
            if (QFileInfo::exists(regularPath)) {
                const int regularId = QFontDatabase::addApplicationFont(regularPath);
                const auto regularFamilies = QFontDatabase::applicationFontFamilies(regularId);
                if (!regularFamilies.isEmpty())
                    loadedRegularFamily = regularFamilies.first();
            }

            const auto fontFiles = directory.entryInfoList({ "Pretendard*.otf", "Pretendard*.ttf" }, QDir::Files);
            for (const auto& fontFile : fontFiles)
                QFontDatabase::addApplicationFont(fontFile.absoluteFilePath());
        }

        if (!loadedRegularFamily.isEmpty())
            return loadedRegularFamily;

        QFontDatabase fontDatabase;
        const QStringList families = fontDatabase.families();
        for (const auto& preferredFamily : { "Pretendard", "Pretendard Variable" }) {
            if (families.contains(preferredFamily))
                return preferredFamily;
        }

        for (const auto& family : families) {
            if (family.startsWith("Pretendard"))
                return family;
        }

        return "Pretendard";
    }
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("Abang-All");
    QFont appFont(loadPretendardFamily());
    appFont.setStyleStrategy(QFont::PreferAntialias);
    appFont.setHintingPreference(QFont::PreferNoHinting);
    app.setFont(appFont);
    app.setStyleSheet(UiTheme::appStyle());
    app.setWindowIcon(QIcon(":/icons/brand-mark.png"));

    // Keep the app alive when the main window is closed/hidden to tray
    app.setQuitOnLastWindowClosed(false);

    // ── 단일 인스턴스 검사 ───────────────────────────────────────────
    // 이미 실행 중인 인스턴스가 있으면 "show" 신호를 보내고 종료
    {
        QLocalSocket probe;
        probe.connectToServer(QLatin1String(kServerName));
        if (probe.waitForConnected(300)) {
            probe.write("show");
            probe.flush();
            probe.waitForBytesWritten(300);
            return 0; // 기존 인스턴스에 위임하고 종료
        }
    }

    // 서버 시작 (이 인스턴스가 첫 번째)
    QLocalServer server;
    QLocalServer::removeServer(QLatin1String(kServerName)); // 잔여 소켓 정리
    server.listen(QLatin1String(kServerName));
    // ────────────────────────────────────────────────────────────────

    // ── 언어 설정 로드 ──────────────────────────────────────────────
    QTranslator translator;
    {
        QSettings langSettings(QStringLiteral("Abang-All"), QStringLiteral("Abang-All"));
        // 하위 호환: 기존 ui/languageToggleKorean 키도 읽는다
        QString lang = langSettings.value(QStringLiteral("ui/language"), QString()).toString();
        if (lang.isEmpty()) {
            // 이전 버전 설정 마이그레이션
            const bool wasKorean = langSettings.value(QStringLiteral("ui/languageToggleKorean"), true).toBool();
            lang = wasKorean ? QStringLiteral("ko") : QStringLiteral("en");
            langSettings.setValue(QStringLiteral("ui/language"), lang);
        }
        if (lang == QStringLiteral("en")) {
            if (translator.load(QStringLiteral(":/i18n/en.qm")))
                app.installTranslator(&translator);
        }
    }
    // ────────────────────────────────────────────────────────────────

    MainWindow window;
    QSettings uiSettings(QStringLiteral("Abang-All"), QStringLiteral("Abang-All"));
    if (uiSettings.value(QStringLiteral("ui/showMainOnStartup"), true).toBool())
        window.show();

    // 두 번째 실행 신호 수신 → 메인 창 표시 + 포커스
    QObject::connect(&server, &QLocalServer::newConnection, [&]() {
        QLocalSocket* client = server.nextPendingConnection();
        QObject::connect(client, &QLocalSocket::readyRead, [&window, client]() {
            client->readAll(); // "show" 메시지 소비
            window.show();
            window.setWindowState(window.windowState() & ~Qt::WindowMinimized);
            window.raise();
            window.activateWindow();
            client->deleteLater();
        });
    });

    return app.exec();
}
