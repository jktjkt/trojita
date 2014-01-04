/* Copyright (C) 2013  Ahmed Ibrahim Khalil <ahmedibrahimkhali@gmail.com>
   Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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

#ifndef FULLMESSAGEDOWNLOADER_H
#define FULLMESSAGEDOWNLOADER_H

#include <QObject>
#include <QPersistentModelIndex>


namespace Imap
{

namespace Mailbox
{

class Model;
class TreeItemMessage;
class TreeItemPart;

/** @short Combines both the header and the body parts of a message into one part.

Use FullMessageCombiner::load() to start loading the message, and when finished a SIGNAL(completed()) will be emitted
then you can retrieve the combined parts using FullMessageCombiner::data(). If both parts are already fetched a SIGNAL(completed())
will also be emitted.
*/

class FullMessageCombiner : public QObject
{
    Q_OBJECT
public:
    explicit FullMessageCombiner(const QModelIndex &m_messageIndex, QObject *parent = 0);
    QByteArray data() const;
    bool loaded() const;
    void load();

signals:
    void completed();
    void failed(const QString &message);

private:
    TreeItemPart *headerPartPtr() const;
    TreeItemPart *bodyPartPtr() const;

private slots:
    void slotDataChanged(const QModelIndex &left, const QModelIndex &right);

private:
    const Imap::Mailbox::Model *m_model;
    QPersistentModelIndex m_bodyPartIndex;
    QPersistentModelIndex m_headerPartIndex;
    QPersistentModelIndex m_messageIndex;
};


}
}
#endif // FULLMESSAGEDOWNLOADER_H
