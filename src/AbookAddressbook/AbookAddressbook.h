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

#ifndef ABOOK_ADDRESSBOOK
#define ABOOK_ADDRESSBOOK

#include <QPair>
#include "Gui/AbstractAddressbook.h"

class QFileSystemWatcher;
class QStandardItemModel;
class QTimer;

namespace Gui
{

/** @short A generic local adressbook interface*/
class AbookAddressbook : public QObject, public AbstractAddressbook {
    Q_OBJECT
public:
    AbookAddressbook();
    virtual ~AbookAddressbook();

    enum Type { Name = Qt::DisplayRole, Mail = Qt::UserRole + 1,
                Address, Address2, City, State, ZIP, Country,
                Phone, Workphone, Fax, Mobile,
                Nick, URL, Notes, Anniversary, Photo,
                UnknownKeys, Dirty };

    virtual QStringList complete(const QString &string, const QStringList &ignores, int max = -1) const;
    virtual QStringList prettyNamesForAddress(const QString &mail) const;

    QStandardItemModel *model() const;

public slots:
    void saveContacts();
    void readAbook(bool update = false);
    void updateAbook();

private slots:
    void scheduleAbookUpdate();

private:
    void ensureAbookPath();
    void remonitorAdressbook();
    static QString formatAddress(const QString &contactName, const QString &mail);

    QFileSystemWatcher *m_filesystemWatcher;
    QTimer *m_updateTimer;
    QStandardItemModel *m_contacts;

    QList<QPair<Type,QString> > m_fields;
};

}

#endif // ABOOK_ADDRESSBOOK
