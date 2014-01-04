/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <QApplication>
#include <QLibraryInfo>
#include <QSettings>
#include <QTranslator>

#include "AppVersion/SetCoreApplication.h"
#include "Common/Application.h"
#include "Common/MetaTypes.h"
#include "Gui/Util.h"
#include "Gui/Window.h"

#include "static_plugins.h"

int main(int argc, char **argv)
{
    Common::registerMetaTypes();

    QApplication app(argc, argv);
    Q_INIT_RESOURCE(icons);
    Q_INIT_RESOURCE(license);

    QTranslator qtTranslator;
    qtTranslator.load(QLatin1String("qt_") + QLocale::system().name(),
                      QLibraryInfo::location(QLibraryInfo::TranslationsPath));
    app.installTranslator(&qtTranslator);

    QLatin1String localeSuffix("/locale");
    QString localeName(QLatin1String("trojita_common_") + QLocale::system().name());

    // The "installed to system" localization
    QTranslator appSystemTranslator;
    if (!Gui::Util::pkgDataDir().isEmpty()) {
        appSystemTranslator.load(localeName, Gui::Util::pkgDataDir() + localeSuffix);
        app.installTranslator(&appSystemTranslator);
    }

    // The "in the directory with the binary" localization
    QTranslator appDirectoryTranslator;
    appDirectoryTranslator.load(localeName, app.applicationDirPath() + localeSuffix);
    app.installTranslator(&appDirectoryTranslator);

    AppVersion::setGitVersion();
    AppVersion::setCoreApplicationData();
    app.setWindowIcon(QIcon(QLatin1String(":/icons/trojita.png")));

    // Hack: support multiple "profiles"
    QString profileName;
    if (argc == 3 && argv[1] == QByteArray("--profile")) {
        profileName = QString::fromLocal8Bit(argv[2]);
        // We are abusing the env vars here. Yes, it's a hidden global. Yes, it's ugly.
        // Take it or leave it, this is a time-limited hack.
        // The env var is also in UTF-8. I like UTF-8.
        qputenv("TROJITA_PROFILE", profileName.toUtf8());
    } else {
#ifndef Q_OS_WIN32
        unsetenv("TROJITA_PROFILE");
#else
        putenv("TROJITA_PROFILE=");
#endif
    }
    QSettings settings(Common::Application::organization,
                       profileName.isEmpty() ? Common::Application::name : Common::Application::name + QLatin1Char('-') + profileName);
    Gui::MainWindow win(&settings);
    win.show();
    return app.exec();
}
