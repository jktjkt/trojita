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
#ifndef IMAP_MAILBOX_UTILS_H
#define IMAP_MAILBOX_UTILS_H

#include <QObject>
#include <QString>

class QSslCertificate;
class QSslError;

namespace Imap
{
namespace Mailbox
{

class PrettySize: public QObject
{
    Q_OBJECT
public:
    typedef enum {
        COMPACT_FORM, /**< @short Do not append "B" when the size is less than 1kB */
        WITH_BYTES_SUFFIX /**< @short Always prepend the units, even if it's just in bytes */
    } ShowBytesSuffix;
    static QString prettySize(uint bytes, const ShowBytesSuffix compactUnitFormat=COMPACT_FORM);
};

/** @short Return the name of a log file for logging IMAP communication */
QString persistentLogFileName();

/** @short Return a system/platform version */
QString systemPlatformVersion();

class CertificateUtils: public QObject
{
    Q_OBJECT
public:
    static QString chainToHtml(const QList<QSslCertificate> &sslChain);
    static QString errorsToHtml(const QList<QSslError> &sslErrors);

    /** @short Shamelessly stolen from QMessageBox */
    typedef enum {
        NoIcon = 0,
        Information = 1,
        Warning = 2,
        Critical = 3,
        Question = 4
    } IconType;

    static void formatSslState(const QList<QSslCertificate> &sslChain, const QList<QSslCertificate> &oldSslChain,
                               const QByteArray &oldCertificatePem, const QList<QSslError> &sslErrors,
                               QString *title, QString *message, IconType *icon);
};

}
}

#endif // IMAP_MAILBOX_UTILS_H
