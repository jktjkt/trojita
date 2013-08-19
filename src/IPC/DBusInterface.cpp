/* Copyright (C) 2013 Pali Rohár <pali.rohar@gmail.com>

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

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>

#include "MainWindowBridge.h"

#include "IPC.h"

namespace IPC {

static QString service = QLatin1String("net.flaska.trojita");
static QString path = QLatin1String("/Trojita");

namespace Instance {

static QDBusInterface &interface()
{
    static QDBusInterface iface(service, path, service);
    return iface;
}

bool isRunning()
{
    return QDBusConnection::sessionBus().interface()->isServiceRegistered(service);
}

void showMainWindow()
{
    interface().call(QDBus::NoBlock, QLatin1String("showMainWindow"));
}

void showAddressbookWindow()
{
    interface().call(QDBus::NoBlock, QLatin1String("showAddressbookWindow"));
}

void composeMail(const QString &url)
{
    interface().call(QDBus::NoBlock, QLatin1String("composeMail"), url);
}

} //namespace Instance

bool registerInstance(Gui::MainWindow *window)
{
    QDBusConnection conn = QDBusConnection::sessionBus();

    bool serviceOk = conn.registerService(service);
    if (!serviceOk)
        return false;

    MainWindowBridge *object = new MainWindowBridge(window);
    bool objectOk = conn.registerObject(path, object, QDBusConnection::ExportAllSlots);
    if (!objectOk) {
        delete object;
        conn.unregisterService(service);
        return false;
    }

    return true;
}

} //namespace IPC
