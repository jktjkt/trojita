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
#ifndef GUI_PARTWIDGETFACTORY_H
#define GUI_PARTWIDGETFACTORY_H

#include "Imap/Network/MsgPartNetAccessManager.h"

#include <QCoreApplication>

namespace Gui {

class PartWidgetFactory
{
    Q_DECLARE_TR_FUNCTIONS(PartWidgetFactory)
    enum { ExpensiveFetchThreshold = 50*1024 };
public:
    PartWidgetFactory( Imap::Network::MsgPartNetAccessManager* _manager, QObject* _wheelEventFilter );
    QWidget* create( Imap::Mailbox::TreeItemPart* part );
    QWidget* create( Imap::Mailbox::TreeItemPart* part, int recursionDepth );
    Imap::Mailbox::Model* model() const;
private:
    Imap::Network::MsgPartNetAccessManager* manager;
    QObject* wheelEventFilter;

    PartWidgetFactory(const PartWidgetFactory&); // don't implement
    PartWidgetFactory& operator=(const PartWidgetFactory&); // don't implement
};

}

#endif // GUI_PARTWIDGETFACTORY_H
