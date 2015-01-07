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

#ifndef IPC_H
#define IPC_H

class QString;
class QStringList;

namespace Gui {
    class MainWindow;
}

namespace IPC {

    namespace Instance {
        /** Return true if Trojita instance is running. This is blocking function. */
        bool isRunning();
        /** Show main window of running instance. */
        void showMainWindow();
        /** Show addressbook window of running instance. */
        void showAddressbookWindow();
        /** Invoke compose window from mailto: url */
        void composeMail(const QString &url);
    }

    /**
     * @short: Register Trojita instance with main window
     *
     * It will be automatically unregistred when window is deleted.
     * Only one process may register instance at same time.
     * Function is blocking and is process/thread safe.
     */
    bool registerInstance(Gui::MainWindow *window, QString &error);

}

#endif //IPC_H
