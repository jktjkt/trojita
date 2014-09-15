/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#ifndef MSGPARTNETACCESSMANAGER_H
#define MSGPARTNETACCESSMANAGER_H

#include <QNetworkAccessManager>
#include <QPersistentModelIndex>

class QUrl;

namespace Imap
{

namespace Network
{

/** @short Implement access to the MIME Parts of the current message and optiojnally also to the public Internet */
class MsgPartNetAccessManager : public QNetworkAccessManager
{
    Q_OBJECT
public:
    explicit MsgPartNetAccessManager(QObject *parent=0);
    void setModelMessage(const QModelIndex &message);
    static QModelIndex pathToPart(const QModelIndex &message, const QByteArray &path);
    static QModelIndex cidToPart(const QModelIndex &rootIndex, const QByteArray &cid);
    QString translateToSupportedMimeType(const QString &originalMimeType) const;
    void registerMimeTypeTranslation(const QString &originalMimeType, const QString &translatedMimeType);
    Q_INVOKABLE void wrapQmlWebViewRequest(QObject *request, QObject *reply);
protected:
    virtual QNetworkReply *createRequest(Operation op, const QNetworkRequest &req, QIODevice *outgoingData=0);
signals:
    void requestingExternal(const QUrl &url);
public slots:
    void setExternalsEnabled(bool enabled);
private:
    QPersistentModelIndex message;

    bool externalsEnabled;
    QMap<QString, QString> m_mimeTypeFixups;

    MsgPartNetAccessManager(const MsgPartNetAccessManager &); // don't implement
    MsgPartNetAccessManager &operator=(const MsgPartNetAccessManager &); // don't implement
};

}
}
#endif // MSGPARTNETACCESSMANAGER_H
