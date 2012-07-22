/* Copyright (C) 2012 Thomas Lübking <thomas.luebking@gmail.com>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or the version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
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
