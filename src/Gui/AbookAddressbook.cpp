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

#include "AbookAddressbook.h"

#include <QDir>
#include <QSettings>

using namespace Gui;

AbookAddressbook::AbookAddressbook()
{
    // read abook
    QSettings abook(QDir::homePath() + "/.abook/addressbook", QSettings::IniFormat);
    abook.setIniCodec("UTF-8");
    QStringList contacts = abook.childGroups();
    Q_FOREACH (const QString &contact, contacts) {
        abook.beginGroup(contact);
        QStringList mails = abook.value("email", QString()).toStringList();
        if (!mails.isEmpty()) {
            QString name = abook.value("name", QString()).toString();
            m_contacts << Contact(name, mails);
        }
        abook.endGroup();
    }
    // that's it =)
}
