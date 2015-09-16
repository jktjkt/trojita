/* Copyright (C) 2006 - 2015 Jan Kundrát <jkt@flaska.net>
   Copyright (C) 2013 - 2014 Pali Rohár <pali.rohar@gmail.com>

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
#include <QFile>
#include <QLibraryInfo>
#include <QObject>
#include <QSettings>
#include <QTextStream>
#include <QTranslator>

#include "AppVersion/SetCoreApplication.h"
#include "Common/Application.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Gui/Util.h"
#include "Gui/Window.h"
#include "IPC/IPC.h"
#include "UiUtils/IconLoader.h"

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
    QString localeName(QLatin1String("trojita_common_") +
                       (qgetenv("KDE_LANG") == "x-test" ? QStringLiteral("x_test") : QLocale::system().name()));

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

    app.setWindowIcon(UiUtils::loadIcon(QStringLiteral("trojita")));

    QTextStream qOut(stdout, QIODevice::WriteOnly);
    QTextStream qErr(stderr, QIODevice::WriteOnly);

    const QStringList &arguments = app.arguments();

    bool error = false;
    bool showHelp = false;
    bool showMainWindow = false;
    bool showComposeWindow = false;
    bool showAddressbookWindow = false;
    bool logToDisk = false;

    QString profileName;

    QString url;

    for (int i = 1; i < arguments.size(); ++i) {
        const QString &arg = arguments.at(i);

        if (arg.startsWith(QLatin1Char('-'))) {
            if (arg == QLatin1String("-m") || arg == QLatin1String("--mainwindow")) {
                showMainWindow = true;
            } else if (arg == QLatin1String("-a") || arg == QLatin1String("--addressbook")) {
                showAddressbookWindow = true;
            } else if (arg == QLatin1String("-c") || arg == QLatin1String("--compose")) {
                showComposeWindow = true;
            } else if (arg == QLatin1String("-h") || arg == QLatin1String("--help")) {
                showHelp = true;
            } else if (arg == QLatin1String("-p") || arg == QLatin1String("--profile")) {
                if (i+1 == arguments.size() || arguments.at(i+1).startsWith(QLatin1Char('-'))) {
                    qErr << QObject::tr("Error: Profile was not specified") << endl;
                    error = true;
                    break;
                } else if (!profileName.isEmpty()) {
                    qErr << QObject::tr("Error: Duplicate profile option '%1'").arg(arg) << endl;
                    error = true;
                    break;
                } else {
                    profileName = arguments.at(i+1);
                    ++i;
                }
            } else if (arg == QLatin1String("--log-to-disk")) {
                logToDisk = true;
            } else {
                qErr << QObject::tr("Warning: Unknown option '%1'").arg(arg) << endl;
            }
        } else {
            if (!url.isEmpty() || !arg.startsWith(QLatin1String("mailto:"))) {
                qErr << QObject::tr("Warning: Unexpected argument '%1'").arg(arg) << endl;
            } else {
                url = arg;
                showComposeWindow = true;
            }
        }
    }

    if (!showMainWindow && !showComposeWindow && !showAddressbookWindow)
        showMainWindow = true;

    if (error)
        showHelp = true;

    if (showHelp) {
        qOut << endl << QObject::trUtf8(
            "Usage: %1 [options] [url]\n"
            "\n"
            "Trojitá %2 - fast Qt IMAP e-mail client\n"
            "\n"
            "Options:\n"
            "  -h, --help               Show this help\n"
            "  -m, --mainwindow         Show main window (default when no option is provided)\n"
            "  -a, --addressbook        Show addressbook window\n"
            "  -c, --compose            Compose new email (default when url is provided)\n"
            "  -p, --profile <profile>  Set profile (cannot start with char '-')\n"
            "  --log-to-disk            Activate debug traffic logging to disk by default\n"
            "\n"
            "Arguments:\n"
            "  url                      Mailto: url address for composing new email\n"
        ).arg(arguments.at(0), Common::Application::version) << endl;
        return error ? 1 : 0;
    }

    // Hack: support multiple "profiles"
    if (!profileName.isEmpty()) {
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

    if (IPC::Instance::isRunning()) {
        if (showMainWindow)
            IPC::Instance::showMainWindow();
        if (showAddressbookWindow)
            IPC::Instance::showAddressbookWindow();
        if (showComposeWindow)
            IPC::Instance::composeMail(url);
        return 0;
    }

    QSettings settings(Common::Application::organization,
                       profileName.isEmpty() ? Common::Application::name : Common::Application::name + QLatin1Char('-') + profileName);
    Gui::MainWindow win(&settings);

    QString errmsg;
    if (!IPC::registerInstance(&win, errmsg))
        qErr << QObject::tr("Error: Registering IPC instance failed: %1").arg(errmsg) << endl;

    if ( settings.value(Common::SettingsNames::guiStartMinimized, QVariant(false)).toBool() ) {
        if ( !settings.value(Common::SettingsNames::guiShowSystray, QVariant(true)).toBool() ) {
            win.show();
            win.setWindowState(Qt::WindowMinimized);
        }
    } else {
        win.show();
    }

    if (showAddressbookWindow)
        win.invokeContactEditor();

    if (showComposeWindow) {
        if (url.isEmpty())
            win.slotComposeMail();
        else
            win.slotComposeMailUrl(QUrl::fromEncoded(url.toUtf8()));
    }

    if (logToDisk) {
        win.enableLoggingToDisk();
    }

    return app.exec();
}
