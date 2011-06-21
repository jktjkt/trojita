/* Copyright (C) 2006 - 2011 Jan Kundrát <jkt@gentoo.org>

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

#ifndef TROJITA_CHECKFORUPDATES_H
#define TROJITA_CHECKFORUPDATES_H

#include <QObject>

class QNetworkReply;

namespace Gui {

/** @short Checking for new versions of Trojita

This class will send a HTTP request to the Trojita's webserver. The request contains the application
version, which is used to decide what kind of response to return.

The server will know the sender's IP address, current version of Qt, OS information and Trojita's version.
*/
class CheckForUpdates: public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(CheckForUpdates);

public:
    CheckForUpdates(QObject *parent);
signals:
    /** @short A new version is avaiable */
    void updateAvailable(const QString &message);
    /** @short The CheckForUpdates has finished

This signal will always be emitted when the checking finishes, either with a usable message or due to an error.
It should be safe to connect this signal to the deleteLater() slot.
 */
    void checkingDone();
public slots:
    /** @short Perform the check now */
    void checkForUpdates();
private slots:
    /** @short Process the incoming network response */
    void slotReplyFinished(QNetworkReply *reply);
};

}

#endif
