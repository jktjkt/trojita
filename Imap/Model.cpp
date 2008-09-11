/* Copyright (C) 2007 Jan Kundrát <jkt@gentoo.org>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/

#include "Imap/Model.h"
#include <QDebug>

namespace Imap {
namespace Mailbox {

Model::Model( QObject* parent, CachePtr cache, AuthenticatorPtr authenticator,
        ParserPtr parser ):
    // parent
    QAbstractItemModel( parent ),
    // our tools
    _cache(cache), _authenticator(authenticator), _parser(parser)
{
    connect( _parser.get(), SIGNAL( responseReceived() ), this, SLOT( responseReceived() ) );
}

QVariant Model::data(const QModelIndex&, int) const
{
    // FIXME
    return QVariant();
}

QModelIndex Model::index(int, int, const QModelIndex&) const
{
    // FIXME
    return QModelIndex();
}

QModelIndex Model::parent(const QModelIndex&) const
{
    // FIXME
    return QModelIndex();
}

int Model::rowCount(const QModelIndex&) const
{
    // FIXME
    return 0;
}

int Model::columnCount(const QModelIndex&) const
{
    // FIXME
    return 0;
}

}
}

#include "Model.moc"
