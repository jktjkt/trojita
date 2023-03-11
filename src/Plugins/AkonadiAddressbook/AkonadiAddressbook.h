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

#ifndef AKONADI_ADDRESSBOOK
#define AKONADI_ADDRESSBOOK

#include <QObject>
#include <QSettings>

#include "Plugins/AddressbookPlugin.h"
#include "Plugins/PluginInterface.h"

using namespace Plugins;

/** @short Akonadi address book interface

Akonadi is centralized database to store, index and retrieve the user's contacts and more since KDE SC 4.4.
Contact lookup is entirely based on searching capabilities of Akonadi.
*/
class AkonadiAddressbook : public AddressbookPlugin {
    Q_OBJECT

public:
    AkonadiAddressbook(QObject *parent);
    virtual ~AkonadiAddressbook();

    virtual AddressbookPlugin::Features features() const;

public slots:
    virtual AddressbookCompletionJob *requestCompletion(const QString &input, const QStringList &ignores = QStringList(), int max = -1);
    virtual AddressbookNamesJob *requestPrettyNamesForAddress(const QString &email);
    virtual void openAddressbookWindow();
    virtual void openContactWindow(const QString &email, const QString &displayName);
};

class trojita_plugin_AkonadiAddressbookPlugin : public QObject, public AddressbookPluginInterface
{
    Q_OBJECT
    Q_INTERFACES(Plugins::AddressbookPluginInterface)
    Q_PLUGIN_METADATA(IID "net.flaska.trojita.plugins.addressbook.akonadiaddressbook")

public:
    QString name() const override;
    QString description() const override;
    AddressbookPlugin *create(QObject *parent, QSettings *settings) override;
};

#endif
