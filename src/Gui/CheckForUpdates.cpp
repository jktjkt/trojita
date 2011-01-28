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

#include "CheckForUpdates.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include "Common/SettingsNames.h"

/** @short All user-facing widgets and related classes */
namespace Gui {

CheckForUpdates::CheckForUpdates(QObject *parent): QObject(parent)
{
}

void CheckForUpdates::checkForUpdates()
{
    QNetworkRequest request;
    request.setUrl(QUrl(QString::fromAscii("http://updates.trojita.flaska.net/updates-v1/trojita/%1").arg(QCoreApplication::applicationVersion())));
    // We're tracking current version of Qt, not the build version
    request.setRawHeader(QByteArray("User-Agent"), QString::fromAscii("Qt/%1").arg(qVersion()).toAscii());
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    manager->get(request);
    connect(manager, SIGNAL(finished(QNetworkReply*)), this, SLOT(slotReplyFinished(QNetworkReply*)));
}

void CheckForUpdates::slotReplyFinished(QNetworkReply *reply)
{
    switch ( reply->error() ) {
    case QNetworkReply::NoError:
        {
            QByteArray preamble = reply->readLine().trimmed();
            if ( preamble == QByteArray("UPDATE_FRESH") ) {
                // do nothing
            } else {
                emit updateAvailable(reply->readAll());
            }
        }
        break;
    case QNetworkReply::ContentNotFoundError:
        // Path wasn't found -> likely an obsolete version
        // fall through
    case QNetworkReply::ContentAccessDenied:
        // This update mechanism is not supported anymore
        // Let's just display a generic message
        emit updateAvailable(trUtf8("Trojitá is under heavy development, so please remember to update frequently."));
        break;
    default:
        // Just ignore everything else.
        break;
    }
    reply->deleteLater();
    reply->manager()->deleteLater();
    QSettings().setValue(Common::SettingsNames::appCheckUpdatesLastTime, QVariant(QDateTime::currentDateTime()));
    emit checkingDone();
}

}

