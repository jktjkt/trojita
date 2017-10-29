/* Copyright (C) Roland Pallai <dap78@magex.hu>

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

#include "AkonadiAddressbook.h"
#include "AkonadiAddressbookCompletionJob.h"
#include "AkonadiAddressbookNamesJob.h"

AkonadiAddressbook::AkonadiAddressbook(QObject *parent): AddressbookPlugin(parent)
{
}

AkonadiAddressbook::~AkonadiAddressbook()
{
}

AddressbookPlugin::Features AkonadiAddressbook::features() const
{
    return FeatureCompletion | FeaturePrettyNames;
}

AddressbookCompletionJob *AkonadiAddressbook::requestCompletion(const QString &input, const QStringList &ignores, int max)
{
    return new AkonadiAddressbookCompletionJob(input, ignores, max, this);
}

AddressbookNamesJob *AkonadiAddressbook::requestPrettyNamesForAddress(const QString &email)
{
    return new AkonadiAddressbookNamesJob(email, this);
}

void AkonadiAddressbook::openAddressbookWindow()
{
    qWarning() << "AkonadiAddressbook::openAddressbookWindow not implemented";
}

void AkonadiAddressbook::openContactWindow(const QString &email, const QString &displayName)
{
    qWarning() << "AkonadiAddressbook::openContactWindow not implemented";
}

QString trojita_plugin_AkonadiAddressbookPlugin::name() const
{
    return QStringLiteral("akonadiaddressbook");
}

QString trojita_plugin_AkonadiAddressbookPlugin::description() const
{
    return tr("KDE Addressbook (Akonadi)");
}

AddressbookPlugin *trojita_plugin_AkonadiAddressbookPlugin::create(QObject *parent, QSettings *)
{
    return new AkonadiAddressbook(parent);
}
