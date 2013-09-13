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

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QUrlQuery>
#endif

#include "Mailto.h"
#include "Recipients.h"

#include "Imap/Encoders.h"

namespace {

QString fromRFC2047PercentEncoding(const QByteArray &str)
{
    return Imap::decodeRFC2047String(QByteArray::fromPercentEncoding(str));
}

QStringList stringListFromRFC2047PercentEncoding(const QByteArray &str)
{
    const QList<QByteArray> &list = str.split(',');
    QStringList ret;
    Q_FOREACH(const QByteArray &a, list) {
        if (a.isEmpty())
            continue;
        ret << fromRFC2047PercentEncoding(a);
    }
    return ret;
}

QList<QByteArray> arrayListFromPercentEncoding(const QByteArray &str)
{
    const QList<QByteArray> &list = str.split(',');
    QList<QByteArray> ret;
    Q_FOREACH(const QByteArray &a, list) {
        if (a.isEmpty())
            continue;
        ret << QByteArray::fromPercentEncoding(a);
    }
    return ret;
}

}

namespace Composer {

void parseRFC6068Mailto(const QUrl &url, QString &subject, QString &body,
                        QList<QPair<Composer::RecipientKind, QString>> &recipients,
                        QList<QByteArray> &inReplyTo, QList<QByteArray> &references)
{
    if (url.isEmpty() || url.scheme() != QLatin1String("mailto"))
        return;

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    const QByteArray &encodedPath = url.encodedPath();
#else
    const QByteArray &encodedPath = url.path(QUrl::FullyEncoded).toUtf8();
#endif

    Q_FOREACH(const QString &addr, stringListFromRFC2047PercentEncoding(encodedPath))
        recipients << qMakePair(Composer::ADDRESS_TO, addr);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    const QList<QPair<QByteArray,QByteArray>> &items = url.encodedQueryItems();
#else
    const QList<QPair<QString,QString>> &stringItems = QUrlQuery(url).queryItems(QUrl::FullyEncoded);
    QList<QPair<QByteArray,QByteArray>> items;
    for (int i = 0; i < stringItems.size(); ++i)
        items << qMakePair(stringItems[i].first.toUtf8(), stringItems[i].second.toUtf8());
#endif

    for (int i = 0; i < items.size(); ++i) {
        if (items[i].first.toLower() == "to") {
            Q_FOREACH(const QString &addr, stringListFromRFC2047PercentEncoding(items[i].second))
                recipients << qMakePair(Composer::ADDRESS_TO, addr);
        } else if (items[i].first.toLower() == "cc") {
            Q_FOREACH(const QString &addr, stringListFromRFC2047PercentEncoding(items[i].second))
                recipients << qMakePair(Composer::ADDRESS_CC, addr);
        } else if (items[i].first.toLower() == "bcc") {
            Q_FOREACH(const QString &addr, stringListFromRFC2047PercentEncoding(items[i].second))
                recipients << qMakePair(Composer::ADDRESS_BCC, addr);
        } else if (items[i].first.toLower() == "subject") {
            subject = fromRFC2047PercentEncoding(items[i].second);
        } else if (items[i].first.toLower() == "body") {
            // RFC 6080: Body does not contain MIME encoded words
            body = QUrl::fromPercentEncoding(items[i].second);
        } else if (items[i].first.toLower() == "in-reply-to") {
            inReplyTo << arrayListFromPercentEncoding(items[i].second);
        } else if (items[i].first.toLower() == "references") {
            references << arrayListFromPercentEncoding(items[i].second);
        }
    }
}

}
