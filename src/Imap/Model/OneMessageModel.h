/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

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

#ifndef IMAP_MODEL_ONEMESSAGEMODEL_H
#define IMAP_MODEL_ONEMESSAGEMODEL_H

#include <QVariant>
#include "SubtreeModel.h"

/** @short Namespace for IMAP interaction */
namespace Imap
{

/** @short Classes for handling of mailboxes and connections */
namespace Mailbox
{

/** @short Publish contents of a selected message */
class OneMessageModel: public SubtreeModel
{
    Q_OBJECT
    Q_DISABLE_COPY(OneMessageModel)
    Q_PROPERTY(QString subject READ subject NOTIFY envelopeChanged)
    Q_PROPERTY(QDateTime date READ date NOTIFY envelopeChanged)

private slots:
    virtual void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

public:
    OneMessageModel(QObject *parent = 0);
    void setMessage(const QModelIndex &message);

    QString subject() const;
    QDateTime date() const;

signals:
    void envelopeChanged();
};

}

}

#endif /* IMAP_MODEL_ONEMESSAGEMODEL_H */
