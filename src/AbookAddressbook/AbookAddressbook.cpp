/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2013 Caspar Schutijser <caspar@schutijser.com>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#include "AbookAddressbook.h"

#include <QDir>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringBuilder>
#include <QTimer>
#include "Common/SettingsCategoryGuard.h"

using namespace Gui;

AbookAddressbook::AbookAddressbook(): m_updateTimer(0)
{
#define ADD(TYPE, KEY) \
    m_fields << qMakePair<Type,QString>(TYPE, QLatin1String(KEY))
    ADD(Name, "name");
    ADD(Mail, "email");
    ADD(Address, "address");
    ADD(City, "city");
    ADD(State, "state");
    ADD(ZIP, "zip");
    ADD(Country, "country");
    ADD(Phone, "phone");
    ADD(Workphone, "workphone");
    ADD(Fax, "fax");
    ADD(Mobile, "mobile");
    ADD(Nick, "nick");
    ADD(URL, "url");
    ADD(Anniversary, "anniversary");
    ADD(Photo, "photo");
#undef ADD

    m_contacts = new QStandardItemModel(this);

    ensureAbookPath();

    // read abook
    readAbook(false);

    m_filesystemWatcher = new QFileSystemWatcher(this);
    m_filesystemWatcher->addPath(QDir::homePath() + "/.abook/addressbook");
    connect (m_filesystemWatcher, SIGNAL(fileChanged(QString)), SLOT(scheduleAbookUpdate()));
}

AbookAddressbook::~AbookAddressbook()
{
}

QStandardItemModel *AbookAddressbook::model() const
{
    return m_contacts;
}

void AbookAddressbook::remonitorAdressbook()
{
    m_filesystemWatcher->addPath(QDir::homePath() + "/.abook/addressbook");
}

void AbookAddressbook::ensureAbookPath()
{
    if (!QDir::home().exists(".abook")) {
        QDir::home().mkdir(".abook");
    }
    QDir abook(QDir::homePath() + "/.abook/");
    QStringList abookrc;
    QFile file(QDir::homePath() + "/.abook/abookrc");
    if (file.exists() && file.open(QIODevice::ReadWrite|QIODevice::Text)) {
        abookrc = QString::fromLocal8Bit(file.readAll()).split('\n');
        bool havePhoto = false;
        for (QStringList::iterator it = abookrc.begin(), end = abookrc.end(); it != end; ++it) {
            if (it->contains("preserve_fields"))
                *it = "set preserve_fields=all";
            else if (it->contains("photo") && it->contains("field"))
                havePhoto = true;
        }
        if (!havePhoto)
            abookrc << "field photo = Photo";
    } else {
        abookrc << "field photo = Photo" << "set preserve_fields=all";
        file.open(QIODevice::WriteOnly|QIODevice::Text);
    }
    if (file.isOpen()) {
        if (file.isWritable()) {
            file.seek(0);
            file.write(abookrc.join("\n").toLocal8Bit());
        }
        file.close();
    }
    QFile abookFile(abook.filePath(QLatin1String("addressbook")));
    if (!abookFile.exists()) {
        abookFile.open(QIODevice::WriteOnly);
    }
}

void AbookAddressbook::scheduleAbookUpdate()
{
    // we need to schedule this because the filesystemwatcher usually fires while the file is re/written
    if (!m_updateTimer) {
        m_updateTimer = new QTimer(this);
        m_updateTimer->setSingleShot(true);
        connect (m_updateTimer, SIGNAL(timeout()), SLOT(updateAbook()));
    }
    m_updateTimer->start(500);
}

void AbookAddressbook::updateAbook()
{
    readAbook(true);
    // QFileSystemWatcher will usually unhook from the file when it's re/written - the entire watcher ain't so great :-(
    m_filesystemWatcher->addPath(QDir::homePath() + "/.abook/addressbook");
}

void AbookAddressbook::readAbook(bool update)
{
//     QElapsedTimer profile;
//     profile.start();
    QSettings abook(QDir::homePath() + "/.abook/addressbook", QSettings::IniFormat);
    abook.setIniCodec("UTF-8");
    QStringList contacts = abook.childGroups();
    foreach (const QString &contact, contacts) {
        Common::SettingsCategoryGuard guard(&abook, contact);
        QStandardItem *item = 0;
        QStringList mails;
        if (update) {
            QList<QStandardItem*> list = m_contacts->findItems(abook.value("name").toString());
            if (list.count() == 1)
                item = list.at(0);
            else if (list.count() > 1) {
                mails = abook.value("email", QString()).toStringList();
                const QString mailString = mails.join("\n");
                foreach (QStandardItem *it, list) {
                    if (it->data(Mail).toString() == mailString) {
                        item = it;
                        break;
                    }
                }
            }
            if (item && item->data(Dirty).toBool()) {
                continue;
            }
        }
        bool add = !item;
        if (add)
            item = new QStandardItem;

        QMap<QString,QVariant> unknownKeys;

        foreach (const QString &key, abook.allKeys()) {
            QList<QPair<Type,QString> >::const_iterator field = m_fields.constBegin();
            while (field != m_fields.constEnd()) {
                if (field->second == key)
                    break;
                ++field;
            }
            if (field == m_fields.constEnd())
                unknownKeys.insert(key, abook.value(key));
            else if (field->first == Mail) {
                if (mails.isEmpty())
                    mails = abook.value(field->second, QString()).toStringList(); // to fix the name field
                item->setData( mails.join("\n"), Mail );
            }
            else
                item->setData( abook.value(field->second, QString()), field->first );
        }

        // attempt to fix the name field
        if (item->data(Name).toString().isEmpty()) {
            if (!mails.isEmpty())
                item->setData( mails.at(0), Name );
        }
        if (item->data(Name).toString().isEmpty()) {
            delete item;
            continue; // junk or format spec entry
        }

        item->setData( unknownKeys, UnknownKeys );

        if (add)
            m_contacts->appendRow( item );
    }
//     const qint64 elapsed = profile.elapsed();
//     qDebug() << "reading too" << elapsed << "ms";
}

void AbookAddressbook::saveContacts()
{
    m_filesystemWatcher->blockSignals(true);
    QSettings abook(QDir::homePath() + "/.abook/addressbook", QSettings::IniFormat);
    abook.setIniCodec("UTF-8");
    abook.clear();
    for (int i = 0; i < m_contacts->rowCount(); ++i) {
        Common::SettingsCategoryGuard guard(&abook, QString::number(i));
        QStandardItem *item = m_contacts->item(i);
        for (QList<QPair<Type,QString> >::const_iterator   it = m_fields.constBegin(),
                                            end = m_fields.constEnd(); it != end; ++it) {
            if (it->first == Mail)
                abook.setValue("email", item->data(Mail).toString().split('\n'));
            else {
                const QVariant v = item->data(it->first);
                if (!v.toString().isEmpty())
                    abook.setValue(it->second, v);
            }
        }
        QMap<QString,QVariant> unknownKeys = item->data( UnknownKeys ).toMap();
        for (QMap<QString,QVariant>::const_iterator it = unknownKeys.constBegin(),
                                                    end = unknownKeys.constEnd(); it != end; ++it) {
            abook.setValue(it.key(), it.value());
        }
    }
    abook.sync();
    m_filesystemWatcher->blockSignals(false);
}

static inline bool ignore(const QString &string, const QStringList &ignores)
{
    Q_FOREACH (const QString &ignore, ignores) {
        if (ignore.contains(string, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QStringList AbookAddressbook::complete(const QString &string, const QStringList &ignores, int max) const
{
    if (string.isEmpty())
        return QStringList();
    QStringList list;
    // In e-mail addresses, dot, dash, _ and @ shall be treated as delimiters
    QRegExp mailMatch = QRegExp(QString::fromUtf8("[\\.\\-_@]%1").arg(QRegExp::escape(string)), Qt::CaseInsensitive);
    // In human readable names, match on word boundaries
    QRegExp nameMatch = QRegExp(QString::fromUtf8("\\b%1").arg(QRegExp::escape(string)), Qt::CaseInsensitive);
    // These REs are still not perfect, they won't match on e.g. ".net" or "-project", but screw these I say
    for (int i = 0; i < m_contacts->rowCount(); ++i) {
        QStandardItem *item = m_contacts->item(i);
        QString contactName = item->data(Name).toString();
        // several mail addresses per contact are stored newline delimited
        QStringList contactMails(item->data(Mail).toString().split(QLatin1Char('\n'), QString::SkipEmptyParts));
        if (contactName.contains(nameMatch)) {
            Q_FOREACH (const QString &mail, contactMails) {
                if (ignore(mail, ignores))
                    continue;
                list << formatAddress(contactName, mail);
                if (list.count() == max)
                    return list;
            }
            continue;
        }
        Q_FOREACH (const QString &mail, contactMails) {
            if (mail.startsWith(string, Qt::CaseInsensitive) ||
                    // don't match on the TLD
                    mail.section(QLatin1Char('.'), 0, -2).contains(mailMatch)) {
                if (ignore(mail, ignores))
                    continue;
                list << formatAddress(contactName, mail);
                if (list.count() == max)
                    return list;
            }
        }
    }
    return list;
}

QString AbookAddressbook::formatAddress(const QString &contactName, const QString &mail)
{
    if (contactName.isEmpty() || contactName == mail)
        return mail;
    else
        return contactName % QLatin1String(" <") % mail % QLatin1String(">");
}

QStringList AbookAddressbook::prettyNamesForAddress(const QString &mail) const
{
    QStringList res;
    for (int i = 0; i < m_contacts->rowCount(); ++i) {
        QStandardItem *item = m_contacts->item(i);
        if (QString::compare(item->data(Mail).toString(), mail, Qt::CaseInsensitive) == 0)
            res << item->data(Name).toString();
    }
    return res;
}
