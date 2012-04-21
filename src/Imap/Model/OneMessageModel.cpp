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

#include "OneMessageModel.h"
#include "ItemRoles.h"
#include "Model.h"
#include "SubtreeModel.h"

namespace Imap
{
namespace Mailbox
{

OneMessageModel::OneMessageModel(Model *model): QObject(model), m_subtree(0)
{
    m_subtree = new SubtreeModel(this);
    m_subtree->setSourceModel(model);
    connect(m_subtree, SIGNAL(modelReset()), this, SIGNAL(envelopeChanged()));
    connect(m_subtree, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SIGNAL(envelopeChanged()));
}

void OneMessageModel::setMessage(const QString &mailbox, const uint uid)
{
    QAbstractItemModel *abstractModel = qobject_cast<QAbstractItemModel*>(QObject::parent());
    Q_ASSERT(abstractModel);
    Model *model = qobject_cast<Model*>(abstractModel);
    Q_ASSERT(model);
    setMessage(model->messageIndexByUid(mailbox, uid));
}

void OneMessageModel::setMessage(const QModelIndex &message)
{
    Q_ASSERT(!message.isValid() || message.model() == QObject::parent());
    m_message = message;
    m_subtree->setRootItem(message);
}

QDateTime OneMessageModel::date() const
{
    return m_message.data(RoleMessageDate).toDateTime();
}

QString OneMessageModel::subject() const
{
    return m_message.data(RoleMessageSubject).toString();
}

}
}
