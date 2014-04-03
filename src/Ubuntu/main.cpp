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
#include "Imap/Model/ImapAccess.h"
#include "Imap/Model/ThreadingMsgListModel.h"

int main(int argc, char *argv[])
{

    QGuiApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);
    parser.setApplicationDescription(QGuiApplication::translate("main", "Trojita is a fast Qt IMAP e-mail client"));
    parser.addHelpOption();
    QCommandLineOption phoneViewOption(QStringList() << QLatin1String("p") << QLatin1String("phone"), QGuiApplication::translate("main",
        "If running on Desktop, start in a phone sized window."));
    parser.addOption(phoneViewOption);
    QCommandLineOption tabletViewOption(QStringList() << QLatin1String("t") << QLatin1String("tablet"), QGuiApplication::translate("main",
        "If running on Desktop, start in a tablet sized window."));
    parser.addOption(tabletViewOption);
    QCommandLineOption testabilityOption(QLatin1String("testability"), QGuiApplication::translate("main",
        "DO NOT USE: autopilot sets this automatically"));
    parser.addOption(testabilityOption);
    parser.process(app);

    QQuickView viewer;
    viewer.setResizeMode(QQuickView::SizeRootObjectToView);

    viewer.engine()->rootContext()->setContextProperty("tablet", QVariant(false));
    viewer.engine()->rootContext()->setContextProperty("phone", QVariant(false));
    if (parser.isSet(tabletViewOption)) {
        qDebug() << "running in tablet mode";
        viewer.engine()->rootContext()->setContextProperty("tablet", QVariant(true));
    } else if (parser.isSet(phoneViewOption)) {
        qDebug() << "running in phone mode";
        viewer.engine()->rootContext()->setContextProperty("phone", QVariant(true));
    } else if (qgetenv("QT_QPA_PLATFORM") != "ubuntumirclient") {
        // Default to tablet size on X11
        viewer.engine()->rootContext()->setContextProperty("tablet", QVariant(true));
    }

    if (parser.isSet(testabilityOption) || getenv("QT_LOAD_TESTABILITY")) {
        QLibrary testLib(QLatin1String("qttestability"));
        if (testLib.load()) {
            typedef void (*TasInitialize)(void);
            TasInitialize initFunction = (TasInitialize)testLib.resolve("qt_testability_init");
            if (initFunction) {
                initFunction();
            } else {
                qCritical("Library qttestability resolve failed!");
            }
        } else {
            qCritical("Library qttestability load failed!");
        }
    }

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
    // sanity check
    if (qmlFile.isEmpty()) {
        qFatal("File: %s does not exist at any of the standard paths!", qPrintable(filePath));
    }

    Common::registerMetaTypes();
    Common::Application::name = QString::fromLatin1("trojita");
    AppVersion::setGitVersion();
    AppVersion::setCoreApplicationData();

    QSettings s;
    Imap::ImapAccess imapAccess(0, &s, QLatin1String("defaultAccount"));
    viewer.engine()->rootContext()->setContextProperty(QLatin1String("imapAccess"), &imapAccess);

    qmlRegisterUncreatableType<Imap::Mailbox::ThreadingMsgListModel>("trojita.models.ThreadingMsgListModel", 0, 1, "ThreadingMsgListModel",
            QLatin1String("ThreadingMsgListModel can only be created from the C++ code. Use ImapAccess if you need access to an instance."));

    viewer.setTitle(QObject::trUtf8("Trojitá"));
    viewer.setSource(QUrl::fromLocalFile(qmlFile));
    viewer.show();
    return app.exec();
}
