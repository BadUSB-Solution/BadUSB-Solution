#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QStringList>

#include "MainWindow.h"
#include "Theme.h"

namespace
{
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
    app.setStyleSheet(UiTheme::AppStyle);

    // Keep the app alive when the main window is closed/hidden to tray
    app.setQuitOnLastWindowClosed(false);

    MainWindow window;
    window.show();

    return app.exec();
}
