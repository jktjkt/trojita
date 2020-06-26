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
#include <QCommandLineOption>
#include <QCommandLineParser>
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

    app.setAttribute(Qt::AA_UseHighDpiPixmaps);
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
    app.setDesktopFileName(QStringLiteral("org.kde.trojita"));
#endif
    app.setWindowIcon(UiUtils::loadIcon(QStringLiteral("trojita")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QObject::tr("Trojitá %1 - fast Qt IMAP e-mail client").arg(Common::Application::version));
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption addressbookOption(QStringList() << QLatin1String("a") << QLatin1String("addressbook"),
                                         QObject::tr("Show addressbook window."));
    parser.addOption(addressbookOption);

    QCommandLineOption composeOption(QStringList() << QLatin1String("c") << QLatin1String("compose"),
                                     QObject::tr("Compose new e-mail (default when url is provided)."));
    parser.addOption(composeOption);

    QCommandLineOption logtodiskOption(QStringList() << QLatin1String("l") << QLatin1String("log-to-disk"),
                                       QObject::tr("Activate debug traffic logging to disk by default."));
    parser.addOption(logtodiskOption);

    QCommandLineOption mainwindowOption(QStringList() << QLatin1String("m") << QLatin1String("mainwindow"),
                                        QObject::tr("Show main window (default when no option is provided)."));
    parser.addOption(mainwindowOption);

    QCommandLineOption profileOption(QStringList() << QLatin1String("p") << QLatin1String("profile"),
                                     QObject::tr("Set profile."), QObject::tr("profile"));
    parser.addOption(profileOption);

    parser.addPositionalArgument(QLatin1String("url"), QObject::tr("Mailto: URL address for composing new e-mail."),
                                 QObject::tr("[url]"));

    parser.process(app);

    QTextStream qOut(stdout, QIODevice::WriteOnly);
    QTextStream qErr(stderr, QIODevice::WriteOnly);

    bool error = false;
    bool showMainWindow = false;
    bool showComposeWindow = false;
    bool showAddressbookWindow = false;
    QString profileName;
    QString url;

    showAddressbookWindow = parser.isSet(addressbookOption);
    showComposeWindow = parser.isSet(composeOption);
    showMainWindow = parser.isSet(mainwindowOption);
    if (parser.isSet(profileOption))
        profileName = parser.value(profileOption);
    if (parser.positionalArguments().size() > 0) {
        if (parser.positionalArguments().size() == 1) {
            url = parser.positionalArguments().at(0);
            if (!url.startsWith(QLatin1String("mailto:"))) {
                qErr << QObject::tr("Unexpected argument '%1'.").arg(url) << endl;
                error = true;
            } else
                showComposeWindow = true;
        } else {
            qErr << QObject::tr("Unexpected arguments.") << endl;
            error = true;
        }
    }

    if (!showMainWindow && !showComposeWindow && !showAddressbookWindow)
        showMainWindow = true;

    if (error) {
        qOut << endl << parser.helpText();
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

    if (parser.isSet(logtodiskOption)) {
        win.enableLoggingToDisk();
    }

    return app.exec();
}
