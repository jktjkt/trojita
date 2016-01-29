/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>
   Copyright (C) 2013 Caspar Schutijser <caspar@schutijser.com>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>
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

#include "AbookAddressbook.h"
#include "be-contacts.h"

#include <QDir>
#include <QFileSystemWatcher>
#include <QSettings>
#include <QStandardItemModel>
#include <QStringBuilder>
#include <QTimer>
#include "Common/SettingsCategoryGuard.h"

class AbookAddressbookCompletionJob : public AddressbookCompletionJob
{
public:
    AbookAddressbookCompletionJob(const QString &input, const QStringList &ignores, int max, AbookAddressbook *parent) :
        AddressbookCompletionJob(parent), m_input(input), m_ignores(ignores), m_max(max), m_parent(parent) {}

public slots:
    virtual void doStart()
    {
        NameEmailList completion = m_parent->complete(m_input, m_ignores, m_max);
        emit completionAvailable(completion);
        finished();
    }

    virtual void doStop()
    {
        emit error(AddressbookJob::Stopped);
        finished();
    }

private:
    QString m_input;
    QStringList m_ignores;
    int m_max;
    AbookAddressbook *m_parent;

};

class AbookAddressbookNamesJob : public AddressbookNamesJob
{
public:
    AbookAddressbookNamesJob(const QString &email, AbookAddressbook *parent) :
        AddressbookNamesJob(parent), m_email(email), m_parent(parent) {}

public slots:
    virtual void doStart()
    {
        QStringList displayNames = m_parent->prettyNamesForAddress(m_email);
        emit prettyNamesForAddressAvailable(displayNames);
        finished();
    }

    virtual void doStop()
    {
        emit error(AddressbookJob::Stopped);
        finished();
    }

private:
    QString m_email;
    AbookAddressbook *m_parent;

};

AbookAddressbook::AbookAddressbook(QObject *parent): AddressbookPlugin(parent), m_updateTimer(0)
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
    ADD(Notes, "notes");
    ADD(Anniversary, "anniversary");
    ADD(Photo, "photo");
#undef ADD

    m_contacts = new QStandardItemModel(this);

    ensureAbookPath();

    // read abook
    readAbook(false);

    m_filesystemWatcher = new QFileSystemWatcher(this);
    m_filesystemWatcher->addPath(QDir::homePath() + QLatin1String("/.abook/addressbook"));
    connect (m_filesystemWatcher, &QFileSystemWatcher::fileChanged, this, &AbookAddressbook::scheduleAbookUpdate);
}

AbookAddressbook::~AbookAddressbook()
{
}

AddressbookPlugin::Features AbookAddressbook::features() const
{
    return FeatureAddressbookWindow | FeatureContactWindow | FeatureAddContact | FeatureEditContact | FeatureCompletion | FeaturePrettyNames;
}

AddressbookCompletionJob *AbookAddressbook::requestCompletion(const QString &input, const QStringList &ignores, int max)
{
    return new AbookAddressbookCompletionJob(input, ignores, max, this);
}

AddressbookNamesJob *AbookAddressbook::requestPrettyNamesForAddress(const QString &email)
{
    return new AbookAddressbookNamesJob(email, this);
}

void AbookAddressbook::openAddressbookWindow()
{
    BE::Contacts *window = new BE::Contacts(this);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    //: Translators: BE::Contacts is the name of a stand-alone address book application.
    //: BE refers to Bose/Einstein (condensate).
    window->setWindowTitle(BE::Contacts::tr("BE::Contacts"));
    window->show();
}

void AbookAddressbook::openContactWindow(const QString &email, const QString &displayName)
{
    BE::Contacts *window = new BE::Contacts(this);
    window->setAttribute(Qt::WA_DeleteOnClose, true);
    window->manageContact(email, displayName);
    window->show();
}

QStandardItemModel *AbookAddressbook::model() const
{
    return m_contacts;
}

void AbookAddressbook::remonitorAdressbook()
{
    m_filesystemWatcher->addPath(QDir::homePath() + QLatin1String("/.abook/addressbook"));
}

void AbookAddressbook::ensureAbookPath()
{
    if (!QDir::home().exists(QStringLiteral(".abook"))) {
        QDir::home().mkdir(QStringLiteral(".abook"));
    }
    QDir abook(QDir::homePath() + QLatin1String("/.abook/"));
    QStringList abookrc;
    QFile file(QDir::homePath() + QLatin1String("/.abook/abookrc"));
    if (file.exists() && file.open(QIODevice::ReadWrite|QIODevice::Text)) {
        abookrc = QString::fromLocal8Bit(file.readAll()).split(QStringLiteral("\n"));
        bool havePhoto = false;
        for (QStringList::iterator it = abookrc.begin(), end = abookrc.end(); it != end; ++it) {
            if (it->contains(QLatin1String("preserve_fields")))
                *it = QStringLiteral("set preserve_fields=all");
            else if (it->contains(QLatin1String("photo")) && it->contains(QLatin1String("field")))
                havePhoto = true;
        }
        if (!havePhoto)
            abookrc << QStringLiteral("field photo = Photo");
    } else {
        abookrc << QStringLiteral("field photo = Photo") << QStringLiteral("set preserve_fields=all");
        file.open(QIODevice::WriteOnly|QIODevice::Text);
    }
    if (file.isOpen()) {
        if (file.isWritable()) {
            file.seek(0);
            file.write(abookrc.join(QStringLiteral("\n")).toLocal8Bit());
        }
        file.close();
    }
    QFile abookFile(abook.filePath(QStringLiteral("addressbook")));
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
        connect(m_updateTimer, &QTimer::timeout, this, &AbookAddressbook::updateAbook);
    }
    m_updateTimer->start(500);
}

void AbookAddressbook::updateAbook()
{
    readAbook(true);
    // QFileSystemWatcher will usually unhook from the file when it's re/written - the entire watcher ain't so great :-(
    m_filesystemWatcher->addPath(QDir::homePath() + QLatin1String("/.abook/addressbook"));
}

void AbookAddressbook::readAbook(bool update)
{
//     QElapsedTimer profile;
//     profile.start();
    QSettings abook(QDir::homePath() + QLatin1String("/.abook/addressbook"), QSettings::IniFormat);
    abook.setIniCodec("UTF-8");
    QStringList contacts = abook.childGroups();
    foreach (const QString &contact, contacts) {
        Common::SettingsCategoryGuard guard(&abook, contact);
        QStandardItem *item = 0;
        QStringList mails;
        if (update) {
            QList<QStandardItem*> list = m_contacts->findItems(abook.value(QStringLiteral("name")).toString());
            if (list.count() == 1)
                item = list.at(0);
            else if (list.count() > 1) {
                mails = abook.value(QStringLiteral("email"), QString()).toStringList();
                const QString mailString = mails.join(QStringLiteral("\n"));
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
                item->setData( mails.join(QStringLiteral("\n")), Mail );
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
    QSettings abook(QDir::homePath() + QLatin1String("/.abook/addressbook"), QSettings::IniFormat);
    abook.setIniCodec("UTF-8");
    abook.clear();
    for (int i = 0; i < m_contacts->rowCount(); ++i) {
        Common::SettingsCategoryGuard guard(&abook, QString::number(i));
        QStandardItem *item = m_contacts->item(i);
        for (QList<QPair<Type,QString> >::const_iterator   it = m_fields.constBegin(),
                                            end = m_fields.constEnd(); it != end; ++it) {
            if (it->first == Mail)
                abook.setValue(QStringLiteral("email"), item->data(Mail).toString().split(QStringLiteral("\n")));
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

NameEmailList AbookAddressbook::complete(const QString &string, const QStringList &ignores, int max) const
{
    NameEmailList list;
    if (string.isEmpty())
        return list;
    // In e-mail addresses, dot, dash, _ and @ shall be treated as delimiters
    QRegExp mailMatch = QRegExp(QStringLiteral("[\\.\\-_@]%1").arg(QRegExp::escape(string)), Qt::CaseInsensitive);
    // In human readable names, match on word boundaries
    QRegExp nameMatch = QRegExp(QStringLiteral("\\b%1").arg(QRegExp::escape(string)), Qt::CaseInsensitive);
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
                list << NameEmail(contactName, mail);
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
                list << NameEmail(contactName, mail);
                if (list.count() == max)
                    return list;
            }
        }
    }
    return list;
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


QString trojita_plugin_AbookAddressbookPlugin::name() const
{
    return QStringLiteral("abookaddressbook");
}

QString trojita_plugin_AbookAddressbookPlugin::description() const
{
    return tr("Addressbook in ~/.abook/");
}

AddressbookPlugin *trojita_plugin_AbookAddressbookPlugin::create(QObject *parent, QSettings *)
{
    return new AbookAddressbook(parent);
}
