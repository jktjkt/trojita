/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>

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

#include <QDateTime>

#include <QApplication>
#include <QDir>
#include <QSettings>

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PluginManager.h"

#include "Common/SettingsCategoryGuard.h"

#ifdef QT_STATICPLUGIN
#include <QtPlugin>
Q_IMPORT_PLUGIN(trojita_plugin_AbookAddressbookPlugin)
#endif

int main(int argc, char **argv) {
    if (argc > 1 && argv[1][0] != '-') {
        QCoreApplication a(argc, argv);
        if (qstrcmp(argv[1], "import")) {
            qWarning("unknown command \"%s\"", argv[1]);
            return 1;
        }
        if (argc == 2) {
            qWarning("you must specify an address string to import, eg.\n"
            "%s import \"Joe User <joe@users.com>\"", argv[0]);
            return 2;
        }
        QMap<QString, QString> imports;
        for (int i = 2; i < argc; ++i) {
            QString arg = QString::fromLocal8Bit(argv[i]);
            QString mail, name;
            QStringList contact = arg.split(QStringLiteral(" "), QString::SkipEmptyParts);
            foreach (const QString &token, contact) {
                if (token.contains(QLatin1Char('@')))
                    mail = token;
                else
                    name = name.isEmpty() ? token : name + QLatin1String(" ") + token;
            }
            if (mail.isEmpty()) {
                qWarning("error, \"%s\" does not seem to contain a mail address", arg.toLocal8Bit().data());
                continue;
            }
            mail.remove(QLatin1Char('<'));
            mail.remove(QLatin1Char('>'));
            if (name.isEmpty())
                name = mail;
            imports.insert(name, mail);
        }
        if (imports.isEmpty()) {
            qWarning("nothing to import");
            return 2;
        }

        int lastContact = -1, updates = 0, adds = 0;
        QSettings abook(QDir::homePath() + QLatin1String("/.abook/addressbook"), QSettings::IniFormat);
        abook.setIniCodec("UTF-8");
        QStringList contacts = abook.childGroups();
        foreach (const QString &contact, contacts) {
            Common::SettingsCategoryGuard guard(&abook, contact);
            int id = contact.toInt();
            if (id > lastContact)
                lastContact = id;
            const QString name = abook.value(QStringLiteral("name"), QString()).toString();
            QMap<QString,QString>::iterator it = imports.begin(), end = imports.end();
            while (it != end) {
                if (it.key() == name) {
                    QStringList mails = abook.value(QStringLiteral("email"), QString()).toStringList();
                    if (!mails.contains(it.value())) {
                        ++updates;
                        mails << it.value();
                        abook.setValue(QStringLiteral("email"), mails);
                    }
                    it = imports.erase(it);
                    continue;
                }
                else
                    ++it;
            }
        }
        QMap<QString,QString>::const_iterator it = imports.constBegin(), end = imports.constEnd();
        while (it != end) {
            ++adds;
            Common::SettingsCategoryGuard guard(&abook, QString::number(++lastContact));
            abook.setValue(QStringLiteral("name"), it.key());
            abook.setValue(QStringLiteral("email"), it.value());
            ++it;
        }
        qWarning("updated %d and added %d contacts", updates, adds);
        return 0;
    }
    QApplication a(argc, argv);
    QSettings s(QStringLiteral("flaska.net"), QStringLiteral("be.contacts"));
    s.setValue(QStringLiteral("plugin"), QLatin1String("abookaddressbook"));
    Plugins::PluginManager m(nullptr, &s, QStringLiteral("plugin"), QStringLiteral("plugin"));
    Plugins::AddressbookPlugin *addressbook = m.addressbook();
    if (addressbook) {
        addressbook->openAddressbookWindow();
        return a.exec();
    } else {
        qWarning("cannot load abookaddressbook plugin");
        return 1;
    }
}
