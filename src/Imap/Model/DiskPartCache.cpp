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

#include "DiskPartCache.h"
#include <QDebug>
#include <QDir>

namespace
{
/** @short Convert the QFile::FileError to a string representation */
QString fileErrorToString(const QFile::FileError e)
{
    switch (e) {
    case QFile::NoError:
        return QStringLiteral("QFile::NoError");
    case QFile::ReadError:
        return QStringLiteral("QFile::ReadError");
    case QFile::WriteError:
        return QStringLiteral("QFile::WriteError");
    case QFile::FatalError:
        return QStringLiteral("QFile::FatalError");
    case QFile::ResourceError:
        return QStringLiteral("QFile::ResourceError");
    case QFile::OpenError:
        return QStringLiteral("QFile::OpenError");
    case QFile::AbortError:
        return QStringLiteral("QFile::AbortError");
    case QFile::TimeOutError:
        return QStringLiteral("QFile::TimeOutError");
    case QFile::UnspecifiedError:
        return QStringLiteral("QFile::UnspecifiedError");
    case QFile::RemoveError:
        return QStringLiteral("QFile::RemoveError");
    case QFile::RenameError:
        return QStringLiteral("QFile::RenameError");
    case QFile::PositionError:
        return QStringLiteral("QFile::PositionError");
    case QFile::ResizeError:
        return QStringLiteral("QFile::ResizeError");
    case QFile::PermissionsError:
        return QStringLiteral("QFile::PermissionsError");
    case QFile::CopyError:
        return QStringLiteral("QFile::CopyError");
    }
    return QObject::tr("Unrecognized QFile error");
}
}

namespace Imap
{
namespace Mailbox
{

DiskPartCache::DiskPartCache(QObject *parent, const QString &cacheDir_): QObject(parent), cacheDir(cacheDir_)
{
    if (!cacheDir.endsWith(QLatin1Char('/')))
        cacheDir.append(QLatin1Char('/'));
}

void DiskPartCache::clearAllMessages(const QString &mailbox)
{
    QDir dir(dirForMailbox(mailbox));
    Q_FOREACH(const QString& fname, dir.entryList(QStringList() << QLatin1String("*.cache"))) {
        if (! dir.remove(fname)) {
            emit error(tr("Couldn't remove file %1 for mailbox %2").arg(fname, mailbox));
        }
    }
}

void DiskPartCache::clearMessage(const QString mailbox, const uint uid)
{
    QDir dir(dirForMailbox(mailbox));
    Q_FOREACH(const QString& fname, dir.entryList(QStringList() << QString::fromUtf8("%1_*.cache").arg(QString::number(uid)))) {
        if (! dir.remove(fname)) {
            emit error(tr("Couldn't remove file %1 for message %2, mailbox %3").arg(fname, QString::number(uid), mailbox));
        }
    }
}

QByteArray DiskPartCache::messagePart(const QString &mailbox, const uint uid, const QByteArray &partId) const
{
    QFile buf(fileForPart(mailbox, uid, partId));
    if (! buf.open(QIODevice::ReadOnly)) {
        return QByteArray();
    }
    return qUncompress(buf.readAll());
}

void DiskPartCache::setMsgPart(const QString &mailbox, const uint uid, const QByteArray &partId, const QByteArray &data)
{
    QString myPath = dirForMailbox(mailbox);
    QDir dir(myPath);
    dir.mkpath(myPath);
    QString fileName(fileForPart(mailbox, uid, partId));
    QFile buf(fileName);
    if (! buf.open(QIODevice::WriteOnly)) {
        emit error(tr("Couldn't save the part %1 of message %2 (mailbox %3) into file %4: %5 (%6)").arg(
                       QString::fromUtf8(partId), QString::number(uid), mailbox, fileName, buf.errorString(), fileErrorToString(buf.error())));
    }
    buf.write(qCompress(data));
}

void DiskPartCache::forgetMessagePart(const QString &mailbox, const uint uid, const QByteArray &partId)
{
    QFile(fileForPart(mailbox, uid, partId)).remove();
}

QString DiskPartCache::dirForMailbox(const QString &mailbox) const
{
    return cacheDir + QString::fromUtf8(mailbox.toUtf8().toBase64());
}

QString DiskPartCache::fileForPart(const QString &mailbox, const uint uid, const QByteArray &partId) const
{
    return QStringLiteral("%1/%2_%3.cache").arg(dirForMailbox(mailbox), QString::number(uid), QString::fromUtf8(partId));
}

}
}

