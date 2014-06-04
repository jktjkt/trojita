/* Copyright (C) 2014 Dan Chapman <dpniel@ubuntu.com>

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
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QQmlContext>
#include <QSettings>
#include <QtGui/QGuiApplication>
#include <QtQml/QtQml>
#include <QtQuick/QQuickView>
#include "AppVersion/SetCoreApplication.h"
#include "Common/Application.h"
#include "Common/MetaTypes.h"
#include "Common/SettingsNames.h"
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ThreadingMsgListModel.h"
#include "MSA/Account.h"
#include "Plugins/PluginManager.h"
#include "static_plugins.h"

int main(int argc, char *argv[])
{

    QGuiApplication app(argc, argv);

    QQuickView viewer;
    viewer.setResizeMode(QQuickView::SizeRootObjectToView);

    // let's first check all standard locations
    QString qmlFile;
    const QString filePath = QLatin1String("qml/trojita/main.qml");
    QStringList paths = QStandardPaths::standardLocations(QStandardPaths::DataLocation);
    paths.prepend(QCoreApplication::applicationDirPath());
    Q_FOREACH (const QString &path, paths) {
        QString myPath = path + QLatin1Char('/') + filePath;
        if (QFile::exists(myPath)) {
            qmlFile = myPath;
            break;
        }
    }
    if (qmlFile.isEmpty()) {
        // ok not a standard location, lets see if we can find trojita.json, to see if we are a click package
        QString manifestPath = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("../../../trojita.json");
        if (QFile::exists(manifestPath)) {
            // we are a click so lets tidy up our manifest path and find qmlfile
            QDir clickRoot = QFileInfo(manifestPath).absoluteDir();
            QString myPath = clickRoot.absolutePath() + QLatin1Char('/') + filePath;
            if (QFile::exists(myPath)) {
                qmlFile = myPath;
            }
        }
    }
    // sanity check
    if (qmlFile.isEmpty()) {
        qFatal("File: %s does not exist at any of the standard paths!", qPrintable(filePath));
    }

    Common::registerMetaTypes();
    Common::Application::name = QString::fromLatin1("net.flaska.trojita");
    Common::Application::organization = QString::fromLatin1("net.flaska.trojita");
    AppVersion::setGitVersion();
    AppVersion::setCoreApplicationData();

    QSettings s;
    auto pluginManager = new Plugins::PluginManager(0, &s,
                                                    Common::SettingsNames::addressbookPlugin, Common::SettingsNames::passwordPlugin);
    Imap::ImapAccess imapAccess(0, &s, pluginManager, QLatin1String("defaultAccount"));
    viewer.engine()->rootContext()->setContextProperty(QLatin1String("imapAccess"), &imapAccess);
    MSA::Account smtpAccountSettings(0, &s, QLatin1String("defaultAccount"));
    viewer.engine()->rootContext()->setContextProperty(QLatin1String("smtpAccountSettings"), &smtpAccountSettings);

    qmlRegisterUncreatableType<Imap::Mailbox::ThreadingMsgListModel>("trojita.models.ThreadingMsgListModel", 0, 1, "ThreadingMsgListModel",
            QLatin1String("ThreadingMsgListModel can only be created from the C++ code. Use ImapAccess if you need access to an instance."));
    qmlRegisterSingletonType<UiUtils::Formatting>("trojita.UiFormatting", 0, 1, "UiFormatting", UiUtils::Formatting::factory);
    qmlRegisterUncreatableType<MSA::Account>("trojita.MSA.Account", 0, 1, "MSAAccount",
            QLatin1String("MSA::Account can be only created from the C++ code."));
    qmlRegisterUncreatableType<UiUtils::PasswordWatcher>("trojita.PasswordWatcher", 0, 1, "PasswordWatcher",
            QLatin1String("PasswordWatcher can only be created from the C++ code. Use ImapAccess if you need access to an instance."));

    viewer.setTitle(QObject::trUtf8("Trojitá"));
    viewer.setSource(QUrl::fromLocalFile(qmlFile));
    viewer.show();
    return app.exec();
}
