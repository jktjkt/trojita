/* Copyright (C) 2014 Boren Zhang <bobo1993324@gmail.com>

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
#ifndef IMAP_NETWORK_QQUICKNETWORKREPLYWRAPPER_H
#define IMAP_NETWORK_QQUICKNETWORKREPLYWRAPPER_H

#include <QNetworkReply>

namespace Imap {

namespace Network {

/** @short send QQuickReply when QNetworkReply is finished

This class is used in MsgPartNetworkAccessManager to handle network request in QML
When the QNetworkReply is finished, the QNetworkReply and this object are deleted.
*/
class QQuickNetworkReplyWrapper : public QObject {
    Q_OBJECT
public:
    QQuickNetworkReplyWrapper(QObject *qquickreply, QNetworkReply *qNetworkReply);
public slots:
    void dataRecieved();
private:
    QObject *m_quickReply;
    QNetworkReply *m_qNetworkReply;
};
}
}

#endif // IMAP_NETWORK_QQUICKNETWORKREPLYWRAPPER_H
